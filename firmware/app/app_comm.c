
#include "cmd_list.h"

#include "bus_com.h"
#include "uart_com.h"
#include "usb_com.h"

#include "obsdata.h"
#include "parameters.h"
#include "protection.h"
#include "tune.h"

#include "app_main.h"
#include "bsp_cfg.h"
#include "bsp_mcu.h"
#include "motor_ctrl.h"
// ============================================================
// cmd_process.c — 上下位机命令分发与 payload 编解码
//
// 帧格式：[head][id][len][payload][crc8][tail]
// 多字节均为小端。
//
// 参数：类型感知 TLV
//   单读  [id:u8] / 回 [id:u8][raw N]
//   单写  [id:u8][raw N] / 回 [status:u8]
//   全读  回 [start:u8][count:u8][raw...]；count=0 结束
//   全写  [start:u8][count:u8][raw...] / 回 [status:u8]
//
// 数据流：订阅 [count:u8][id...]（count<=5，示波通道）；
// CMD_HEARTBEAT 回 [count:u8][固定状态块][订阅值...]
//
// 说明：仅修改本文件。周期发送依赖外部调用 comm_process()；
//       如果主循环尚未调用，数据流/心跳回包不会自动运行。
// ============================================================

#define STREAM_MAX_CHANNELS 5U

typedef enum {
    PORT_UART,
    PORT_USB,
    PORT_CAN,
} ePort;

static tCmdValue rx_cmd;

static uint32_t _last_host_ping_ms = 0;
static bool _host_connected = false;

static uint8_t _stream_ids[STREAM_MAX_CHANNELS];
static uint8_t _stream_count = 0;
static ePort _stream_port = PORT_USB;
static uint32_t _last_stream_ms = 0;
#define STREAM_INTERVAL_MS 1U

static bool _send_frame(ePort port, uint8_t cmd_id, const uint8_t *data,
                        uint8_t len) {
    if (port == PORT_CAN) {
        tBus_Frame frame;
        frame.id = cmd_id;
        frame.data_len =
            (len > sizeof(frame.data)) ? (uint8_t)sizeof(frame.data) : len;
        if (frame.data_len && data)
            memcpy(frame.data, data, frame.data_len);
        return bus_send(&g_bus_can, &frame);
    } else if (port == PORT_UART) {
        tUart_Frame frame;
        frame.id = cmd_id;
        frame.data_len = len;
        if (len && data)
            memcpy(frame.data, data, len);
        return uart_send(&g_uart, &frame);
    } else {
        return usb_send(&g_usb, cmd_id, data, len);
    }
}

static void _reply(ePort port, uint8_t cmd_id, bool ok) {
    uint8_t code = ok ? FB_EXECUTE : FB_FAILURE;
    (void)_send_frame(port, cmd_id, &code, 1U);
}

static bool _read_f32(const uint8_t *data, uint8_t len, float *value) {
    if (!data || !value || len < sizeof(float))
        return false;
    memcpy(value, data, sizeof(float));
    return true;
}

static bool _read_u32(const uint8_t *data, uint8_t len, uint32_t *value) {
    if (!data || !value || len < sizeof(uint32_t))
        return false;
    memcpy(value, data, sizeof(uint32_t));
    return true;
}

// ---------------- 状态心跳 / 示波数据流 ----------------
static void _heartbeat_send(ePort port) {

    uint8_t motor_sta, _fault, motor_warning;
    float vbus, dtemp, mtemp;
    uint8_t send_len;
    dm_data_get(DATA_MOTOR_STATE, &motor_sta, &send_len);
    dm_data_get(DATA_FAULT, &_fault, &send_len);
    dm_data_get(DATA_VBUS, (uint8_t *)&vbus, &send_len);
    dm_data_get(DATA_DRIVER_TEMP, (uint8_t *)&dtemp, &send_len);
    dm_data_get(DATA_MOTOR_TEMP, (uint8_t *)&mtemp, &send_len);

    uint8_t payload[UART_MAX_PKT_SIZE];
    ENPACK_HEARTBEAT_RESPONSE(
        send_len, payload, motor_sta, _fault, vbus, dtemp, mtemp);

    (void)_send_frame(port, CMD_HEARTBEAT, payload, send_len);
}

static void _data_stream_send(ePort port) {
    uint8_t payload[UART_MAX_PKT_SIZE];
    uint8_t pos = 1U;
    payload[0] = _stream_count;
    for (uint8_t i = 0U; i < _stream_count; i++) {
        uint8_t tmp[8];
        uint8_t n = 0U;

        dm_data_get((eObsData)_stream_ids[i], tmp, &n);

        if ((uint16_t)pos + n > UART_MAX_PKT_SIZE)
            break;
        memcpy(&payload[pos], tmp, n);
        pos += n;
    }
    (void)_send_frame(port, CMD_DATA_STREAM, payload, pos);
}

// 控制配置
static void _config_ctrl(const uint8_t *data, uint8_t len) {
    tCmdCfg cfg;
    uint8_t payload_size;
    DEPACK_CTRL_CONFIG_REQUEST(payload_size, data, cfg.mode);
    // if (payload_size != len)
    //     pm_handle_fault(FAULT_INVALID_INPUT);
    switch (cfg.mode) {
    case MOTOR_OPENLOOP:
        DEPACK_CTRL_CONFIG_OPENLOOP_REQUEST(data, cfg.ol_vel_ctrl);
        break;
    case MOTOR_CURRENT_MODE:
        DEPACK_CTRL_CONFIG_CURRENT_REQUEST(data, cfg.tau_ctrl);
        break;
    case MOTOR_PID_SPEED:
        DEPACK_CTRL_CONFIG_PID_SPEED_REQUEST(data, cfg.fw_ctrl);
        break;
    case MOTOR_PID_POSITION:
        DEPACK_CTRL_CONFIG_PID_POSITION_REQUEST(data, cfg.pvt_ctrl);
        break;
    case MOTOR_MIT_MODE:
        DEPACK_CTRL_CONFIG_MIT_REQUEST(data, cfg.mit_kp, cfg.mit_kd);
        break;
    default:
        pm_handle_fault(FAULT_INVALID_INPUT);
        break;
    }
    if (!motor_cmd_cfg(&cfg))
        pm_handle_fault(FAULT_INVALID_INPUT);
}
// ---------------- 控制目标 ----------------
static void _target_openloop(const uint8_t *data, uint8_t len) {
    // TODO:长度校验

    rx_cmd.mode = MOTOR_OPENLOOP;
    uint8_t payload_size;
    DEPACK_OPENLOOP_CTRL_REQUEST(
        payload_size, data, rx_cmd.openloop.duty, rx_cmd.openloop.value);
    rx_cmd.openloop.duty = rx_cmd.openloop.duty * 0.01f;
    if (payload_size != len)
        pm_handle_fault(FAULT_INVALID_INPUT);
    else if (!motor_cmd_set(&rx_cmd))
        pm_handle_fault(FAULT_INVALID_INPUT);
}

static void _target_current(const uint8_t *data, uint8_t len) {

    rx_cmd.mode = MOTOR_CURRENT_MODE;
    uint8_t payload_size;
    DEPACK_CUR_TAU_CTRL_REQUEST(payload_size, data, rx_cmd.current_ctrl.value);
    if (payload_size != len)
        pm_handle_fault(FAULT_INVALID_INPUT);
    else if (!motor_cmd_set(&rx_cmd))
        pm_handle_fault(FAULT_INVALID_INPUT);
}
static void _target_pid_vel(const uint8_t *data, uint8_t len) {
    rx_cmd.mode = MOTOR_PID_SPEED;
    uint8_t payload_size;
    DEPACK_PID_VEL_CTRL_REQUEST(payload_size, data, rx_cmd.vel_pid_ctrl.vel);
    if (payload_size != len)
        pm_handle_fault(FAULT_INVALID_INPUT);
    else if (!motor_cmd_set(&rx_cmd))
        pm_handle_fault(FAULT_INVALID_INPUT);
}

static void _target_pid_pos(const uint8_t *data, uint8_t len) {
    rx_cmd.mode = MOTOR_PID_POSITION;
    uint8_t payload_size;
    DEPACK_PID_POS_CTRL_REQUEST(payload_size, data, rx_cmd.pos_pid_ctrl.pos);

    if (payload_size != len)
        pm_handle_fault(FAULT_INVALID_INPUT);
    else if (!motor_cmd_set(&rx_cmd))
        pm_handle_fault(FAULT_INVALID_INPUT);
}

static void _target_mit_ctrl(const uint8_t *data, uint8_t len) {
    rx_cmd.mode = MOTOR_MIT_MODE;
    uint8_t payload_size;

    DEPACK_MIT_CTRL_REQUEST(payload_size,
                            data,
                            rx_cmd.mit_ctrl.pos,
                            rx_cmd.mit_ctrl.vel,
                            rx_cmd.mit_ctrl.tau_ff);

    if (payload_size != len)
        pm_handle_fault(FAULT_INVALID_INPUT);
    else if (!motor_cmd_set(&rx_cmd))
        pm_handle_fault(FAULT_INVALID_INPUT);
}

// ---------------- 命令分发 ----------------
static void _process_bus_command(ePort port, uint8_t cmd_id,
                                 const uint8_t *data, uint8_t len) {
    uint8_t response[8];

    switch (cmd_id) {

    case CMD_PARAM_READ:
        if (len < 1U) {
            _reply(port, cmd_id, false);
            break;
        }
        response[0] = data[0];
        {
            uint8_t n = 0U;
            param_get((eParameter)data[0], &response[1], &n);
            (void)_send_frame(port, cmd_id, response, (uint8_t)(n + 1U));
        }
        break;

    case CMD_DATA_ASK:
        if (len < 1U) {
            _reply(port, cmd_id, false);
            break;
        }
        {
            uint8_t n = 0U;
            dm_data_get((eObsData)data[0], response, &n);
            (void)_send_frame(port, cmd_id, response, n);
        }
        break;

    case CMD_DISABLE:
        motor_reset();
        _reply(port, cmd_id, true);
        break;
    case CMD_CORE_NRST:
        motor_reset();
        _reply(port, cmd_id, true);
        break;
    case CMD_SYSTEM_RESET:
        system_reset();
        break;

    case CMD_SET_ZERO_POS:
        motor_set_pos_zero();
        break;
    case CMD_SET_LIMIT_POS:
        motor_set_pos_limit();
        break;
    case CMD_CTRL_CONFIG:
        _config_ctrl(data, len);
        break;
    case CMD_OPENLOOP_CTRL:
        _target_openloop(data, len);
        break;
    case CMD_CUR_TAU_CTRL:
        _target_current(data, len);
        break;
    case CMD_PID_VEL_CTRL:
        _target_pid_vel(data, len);
        break;
    case CMD_PID_POS_CTRL:
        _target_pid_pos(data, len);
        break;
    case CMD_MIT_CTRL:
        _target_mit_ctrl(data, len);
        break;

    default:
        pm_handle_fault(FAULT_INVALID_INPUT);
        break;
    }
}

static void _process_command(ePort port, uint8_t cmd_id, const uint8_t *data,
                             uint8_t len) {
    uint8_t response[UART_MAX_PKT_SIZE];

    switch (cmd_id) {
    case CMD_CONNECT:
        _host_connected = (len == 0U) || (data[0] != 0U);
        _last_host_ping_ms = bsp_time_ms();
        if (!_host_connected)
            _stream_count = 0U;
        _reply(port, cmd_id, true);
        break;

    case CMD_HEARTBEAT:
        _last_host_ping_ms = bsp_time_ms();
        _heartbeat_send(port);
        break;

    case CMD_DATA_STREAM_SET:
        if (len < 1U || data[0] > STREAM_MAX_CHANNELS ||
            (uint8_t)(len - 1U) < data[0]) {
            _reply(port, cmd_id, false);
            break;
        }
        _stream_count = data[0];
        for (uint8_t i = 0U; i < _stream_count; i++)
            _stream_ids[i] = data[1U + i];
        _stream_port = port;
        _last_stream_ms = bsp_time_ms();
        _reply(port, cmd_id, true);
        break;

    case CMD_PARAM_READ:
        if (len < 1U) {
            _reply(port, cmd_id, false);
            break;
        }
        response[0] = data[0];
        {
            uint8_t n = 0U;
            param_get((eParameter)data[0], &response[1], &n);
            (void)_send_frame(port, cmd_id, response, (uint8_t)(n + 1U));
        }
        break;

    case CMD_PARAM_WRITE:
        // payload 0xFF：应用已写入参数，重新初始化控制/观测对象
        if (len == 1U && data[0] == 0xFFU) {
            app_main_config();
            _reply(port, cmd_id, true);
            break;
        }
        if (len < 2U) {
            _reply(port, cmd_id, false);
            break;
        }
        param_set((eParameter)data[0], (uint8_t *)&data[1]);
        _reply(port, cmd_id, true);
        break;

    case CMD_PARAM_SAVE:
        _reply(port, cmd_id, param_save());
        _reply(port, cmd_id, true);
        break;
    case CMD_PARAM_RELOAD:
        _reply(port, cmd_id, param_reset());
        _reply(port, cmd_id, true);
        break;

    case CMD_DATA_ASK:
        if (len < 1U) {
            _reply(port, cmd_id, false);
            break;
        }
        {
            uint8_t n = 0U;
            dm_data_get((eObsData)data[0], response, &n);
            (void)_send_frame(port, cmd_id, response, n);
        }
        break;

    case CMD_DISABLE:
        motor_disable();
        break;
    case CMD_CORE_NRST:
        pro_manager_reset();
        break;
    case CMD_SYSTEM_RESET:
        system_reset();
        break;

    case CMD_SET_ZERO_POS:
        motor_set_pos_zero();
        break;
    case CMD_SET_LIMIT_POS:
        motor_set_pos_limit();
        break;

    case CMD_IAP_ENTER: {
        uint32_t version, size;
        if (len != 8U || !_read_u32(data, len, &version) ||
            !_read_u32(data + 4, len - 4, &size) || size > BSP_APP_SIZE ||
            size < 8U || version > 0xFFFFFFU) {
            _reply(port, cmd_id, false);
            break;
        }
        motor_reset();
        if (!upgrade_save(version, size)) {
            _reply(port, cmd_id, false);
            break;
        }
        _reply(port, cmd_id, true);
        /* 为 UART DMA / USB 队列留出完成应答的时间。请求已持久化。 */
        bsp_time_delay_ms(100U);
        system_reset();
        break;
    }

    case CMD_ASK_BL_VERSION:
        // TODO：读param中的版本信息
        break;
    case CMD_ASK_APP_VERSION:

        uint8_t n = 8U;
        uint32_t hw_version = VERSION_ENCODE(
            HW_VERSION_MAJOR, HW_VERSION_MINOR, HW_VERSION_PATCH);
        uint32_t fw_version = VERSION_ENCODE(
            FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
        memcpy(response, &hw_version, 4U);
        memcpy(response + 4U, &fw_version, 4U);
        (void)_send_frame(port, cmd_id, response, n);
        // TODO：读param中的版本信息
        break;
    case CMD_ASK_DEVICE_MESSAGE:
        // TODO：将驱动器信息编成文本发送
        break;
    case CMD_CTRL_CONFIG:
        _config_ctrl(data, len);
        break;
    case CMD_OPENLOOP_CTRL:
        _target_openloop(data, len);
        break;
    case CMD_CUR_TAU_CTRL:
        _target_current(data, len);
        break;
    case CMD_PID_VEL_CTRL:
        _target_pid_vel(data, len);
        break;
    case CMD_PID_POS_CTRL:
        _target_pid_pos(data, len);
        break;
    case CMD_MIT_CTRL:
        _target_mit_ctrl(data, len);
        break;

    case CMD_RL_TUNNING: {
        uint8_t payload_size;
        float tune_current;
        DEPACK_RL_TUNNING_REQUEST(payload_size, data, tune_current);
        if (payload_size != len)
            pm_handle_fault(FAULT_INVALID_INPUT);
        else
            tune_start(TUNE_RL, tune_current, 0.0f);
        break;
    }
    case CMD_PSIF_TUNNING: {
        uint8_t payload_size;
        float tune_vel;
        DEPACK_PSIF_TUNNING_REQUEST(payload_size, data, tune_vel);
        if (payload_size != len)
            pm_handle_fault(FAULT_INVALID_INPUT);
        else
            tune_start(TUNE_Psif, 0.0f, tune_vel);
        break;
    }
    case CMD_MECH_TUNNING: {
        uint8_t payload_size;
        float tune_cur, tune_vel;
        DEPACK_MECH_TUNNING_REQUEST(payload_size, data, tune_cur, tune_vel);
        if (payload_size != len)
            pm_handle_fault(FAULT_INVALID_INPUT);
        else
            tune_start(TUNE_MECH, tune_cur, tune_vel);
        break;
    }
    case CMD_OBS_TUNNING: {
        uint8_t payload_size;
        float tune_cur, tune_vel;
        DEPACK_OBS_TUNNING_REQUEST(payload_size, data, tune_cur, tune_vel);
        if (payload_size != len)
            pm_handle_fault(FAULT_INVALID_INPUT);
        else
            tune_start(TUNE_OBS, tune_cur, tune_vel);
        break;
    }
    case CMD_PID_TUNNING: {
        uint8_t payload_size;
        float tune_cur, tune_vel;
        DEPACK_PID_TUNNING_REQUEST(payload_size, data, tune_cur, tune_vel);
        if (payload_size != len)
            pm_handle_fault(FAULT_INVALID_INPUT);
        else
            tune_start(TUNE_PID, tune_cur, tune_vel);
        break;
    }
    case CMD_MIT_TUNNING: {
        uint8_t payload_size;
        float tune_cur, tune_vel;
        DEPACK_MIT_TUNNING_REQUEST(payload_size, data, tune_cur, tune_vel);
        if (payload_size != len)
            pm_handle_fault(FAULT_INVALID_INPUT);
        else
            tune_start(TUNE_MIT, tune_cur, tune_vel);
        break;
    }
    default:
        pm_handle_fault(FAULT_INVALID_INPUT);
        break;
    }
}
// 辅助通讯初始化 USB UART
bool comm_sup_init(void) {
    // 4、启动副通讯 usb/uart
    if (!usb_start(&g_usb))
        return false;
    if (!uart_start(&g_uart))
        return false;

    return true;
}
// 主总线通讯初始化 CAN 485
bool comm_main_init(tParameter *param) {
    return bus_start(&g_bus_can, param->can_id);
}

void comm_loop_task(void) {
    if (g_bus_can.dstate == DEV_RUN_ERROR)
        pm_handle_fault(FAULT_BUSCOM_RUN);

    tBus_Frame bus_frame;
    while (bus_process_frame(&g_bus_can, &bus_frame)) {
        if (bus_frame.data_len == 0U)
            continue;
        _process_bus_command(PORT_CAN,
                             bus_frame.data[0],
                             &bus_frame.data[1],
                             (uint8_t)(bus_frame.data_len - 1U));
    }

    tUart_Frame uart_frame;
    while (uart_process_frame(&g_uart, &uart_frame))
        _process_command(
            PORT_UART, uart_frame.id, uart_frame.data, uart_frame.data_len);

    uint8_t usb_data[128];

    tUsb_Frame usb_frame = {
        .data = usb_data,
        .capacity = sizeof(usb_data),
    };

    while (usb_process_frame(&g_usb, &usb_frame)) {
        _process_command(
            PORT_USB, usb_frame.id, usb_frame.data, usb_frame.data_len);
    }

    if (_host_connected && _last_host_ping_ms > 0U &&
        bsp_time_ms() - _last_host_ping_ms > 1000U) {
        _host_connected = false;
        _stream_count = 0U;
    }

    if (_stream_count > 0U &&
        bsp_time_ms() - _last_stream_ms >= STREAM_INTERVAL_MS) {
        _last_stream_ms = bsp_time_ms();
        _data_stream_send(_stream_port);
    }
}
