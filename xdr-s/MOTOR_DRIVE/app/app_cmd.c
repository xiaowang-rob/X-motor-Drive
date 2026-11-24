#include "app_cmd.h"

#include "cmd_list.h"
#include "payload.h"

#include "app_main.h"
#include "bsp_config.h"
#include "bsp_mcu.h"

#include <string.h>

// ============================================================
// app_cmd.c — 上下位机命令分发与 payload 编解码
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
// ============================================================

#define STREAM_MAX_CHANNELS 3U

static tCmdValue rx_cmd;

static uint32_t _last_host_ping_ms = 0;
static bool _host_connected = false;

static uint8_t _stream_ids[STREAM_MAX_CHANNELS];
static uint8_t _stream_count = 0;
static ePort _stream_port = PORT_USB;
static uint32_t _last_stream_ms = 0;
#define STREAM_INTERVAL_MS 1U

static void _reply(const tAppCmdServices *services, ePort port, uint8_t cmd_id, bool ok) {
    uint8_t code = ok ? FB_EXECUTE : FB_FAILURE;
    (void)comm_send_frame(services->comm, port, cmd_id, &code, 1U);
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
static void _heartbeat_send(const tAppCmdServices *services, ePort port) {

    uint8_t motor_sta, _fault;
    float vbus, dtemp, mtemp;
    uint8_t send_len;
    dm_data_get(services->obs, DATA_MOTOR_STATE, &motor_sta, &send_len);
    dm_data_get(services->obs, DATA_FAULT, &_fault, &send_len);
    dm_data_get(services->obs, DATA_VBUS, (uint8_t *)&vbus, &send_len);
    dm_data_get(services->obs, DATA_DRIVER_TEMP, (uint8_t *)&dtemp, &send_len);
    dm_data_get(services->obs, DATA_MOTOR_TEMP, (uint8_t *)&mtemp, &send_len);

    uint8_t payload[UART_MAX_PKT_SIZE];
    ENPACK_HEARTBEAT_RESPONSE(
        send_len, payload, motor_sta, _fault, vbus, dtemp, mtemp);

    (void)comm_send_frame(services->comm, port, CMD_HEARTBEAT, payload, send_len);
}

static void _data_stream_send(const tAppCmdServices *services, ePort port) {
    uint8_t payload[UART_MAX_PKT_SIZE];
    uint8_t pos = 1U;
    payload[0] = _stream_count;
    for (uint8_t i = 0U; i < _stream_count; i++) {
        uint8_t tmp[8];
        uint8_t n = 0U;

        dm_data_get(services->obs, (eObsData)_stream_ids[i], tmp, &n);

        if ((uint16_t)pos + n > UART_MAX_PKT_SIZE)
            break;
        memcpy(&payload[pos], tmp, n);
        pos += n;
    }
    (void)comm_send_frame(services->comm, port, CMD_DATA_STREAM, payload, pos);
}

// 控制配置
static void _config_ctrl(const tAppCmdServices *services, const uint8_t *data, uint8_t len) {
    tCmdCfg cfg;
    DEPACK_CTRL_CONFIG_REQUEST(data, cfg.mode);
    switch (cfg.mode) {
    case MOTOR_OPENLOOP:
        DEPACK_CTRL_CONFIG_REQUEST(data, cfg.ol_vel_ctrl);
        break;
    case MOTOR_CURRENT_MODE:
        DEPACK_CTRL_CONFIG_REQUEST(data, cfg.tau_ctrl);
        break;
    case MOTOR_PID_SPEED:
        DEPACK_CTRL_CONFIG_REQUEST(data, cfg.fw_ctrl);
        break;
    case MOTOR_PID_POSITION:
        DEPACK_CTRL_CONFIG_REQUEST(data, cfg.pvt_ctrl);
        break;
    case MOTOR_MIT_MODE:
        DEPACK_CTRL_CONFIG_MIT_REQUEST(data, cfg.mit_kp, cfg.mit_kd);
        break;
    default:
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
        break;
    }
    if (!motor_cmd_cfg(services->motor, &cfg))
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
}
// ---------------- 控制目标 ----------------
static void _target_openloop(const tAppCmdServices *services, const uint8_t *data, uint8_t len) {
    // TODO:长度校验

    rx_cmd.mode = MOTOR_OPENLOOP;
    uint8_t payload_size;
    DEPACK_OPENLOOP_CTRL_REQUEST(
        payload_size, data, rx_cmd.value.openloop.duty, rx_cmd.value.openloop.value);
    if (payload_size != len)
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
    else if (!motor_cmd_set(services->motor, &rx_cmd))
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
}

static void _target_current(const tAppCmdServices *services, const uint8_t *data, uint8_t len) {

    rx_cmd.mode = MOTOR_CURRENT_MODE;
    uint8_t payload_size;
    DEPACK_CUR_TAU_CTRL_REQUEST(payload_size, data, rx_cmd.current_ctrl.value);
    if (payload_size != len)
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
    else if (!motor_cmd_set(services->motor, &rx_cmd))
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
}
static void _target_pid_vel(const tAppCmdServices *services, const uint8_t *data, uint8_t len) {
    rx_cmd.mode = MOTOR_PID_SPEED;
    uint8_t payload_size;
    DEPACK_PID_VEL_CTRL_REQUEST(payload_size, data, rx_cmd.vel_pid_ctrl.vel);
    if (payload_size != len)
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
    else if (!motor_cmd_set(services->motor, &rx_cmd))
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
}

static void _target_pid_pos(const tAppCmdServices *services, const uint8_t *data, uint8_t len) {
    rx_cmd.mode = MOTOR_PID_POSITION;
    uint8_t payload_size;
    DEPACK_PID_POS_CTRL_REQUEST(payload_size, data, rx_cmd.pos_pid_ctrl.pos);

    if (payload_size != len)
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
    else if (!motor_cmd_set(services->motor, &rx_cmd))
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
}

static void _target_mit_ctrl(const tAppCmdServices *services, const uint8_t *data, uint8_t len) {
    rx_cmd.mode = MOTOR_MIT_MODE;
    uint8_t payload_size;

    DEPACK_MIT_CTRL_REQUEST(payload_size,
                            data,
                            rx_cmd.mit_ctrl.pos,
                            rx_cmd.mit_ctrl.vel,
                            rx_cmd.mit_ctrl.tau_ff);

    if (payload_size != len)
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
    else if (!motor_cmd_set(services->motor, &rx_cmd))
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
}

// ---------------- 命令分发 ----------------
static void _process_bus_command(const tAppCmdServices *services, ePort port, uint8_t cmd_id,
                                 const uint8_t *data, uint8_t len) {
    uint8_t response[8];

    switch (cmd_id) {

    case CMD_PARAM_READ:
        if (len < 1U) {
            _reply(services, port, cmd_id, false);
            break;
        }
        response[0] = data[0];
        {
            uint8_t n = 0U;
            param_get(services->param, (eParameter)data[0], &response[1], &n);
            (void)comm_send_frame(services->comm, port, cmd_id, response, (uint8_t)(n + 1U));
        }
        break;

    case CMD_DATA_ASK:
        if (len < 1U) {
            _reply(services, port, cmd_id, false);
            break;
        }
        {
            uint8_t n = 0U;
            dm_data_get(services->obs, (eObsData)data[0], response, &n);
            (void)comm_send_frame(services->comm, port, cmd_id, response, n);
        }
        break;

    case CMD_DISABLE:
        motor_reset(services->motor);
        _reply(services, port, cmd_id, true);
        break;
    case CMD_CORE_NRST:
        motor_reset(services->motor);
        _reply(services, port, cmd_id, true);
        break;
    case CMD_SYSTEM_RESET:
        system_reset();
        break;

    case CMD_SET_ZERO_POS:
        em_set_pos_zero(services->enc);
        break;
    case CMD_SET_LIMIT_POS:
        em_set_pos_limit(services->enc);
        break;
    case CMD_CTRL_CONFIG:
        _config_ctrl(services, data, len);
        break;
    case CMD_OPENLOOP_CTRL:
        _target_openloop(services, data, len);
        break;
    case CMD_CUR_TAU_CTRL:
        _target_current(services, data, len);
        break;
    case CMD_PID_VEL_CTRL:
        _target_pid_vel(services, data, len);
        break;
    case CMD_PID_POS_CTRL:
        _target_pid_pos(services, data, len);
        break;
    case CMD_MIT_CTRL:
        _target_mit_ctrl(services, data, len);
        break;

    default:
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
        break;
    }
}

static void _process_command(const tAppCmdServices *services, ePort port, uint8_t cmd_id, const uint8_t *data,
                             uint8_t len) {
    uint8_t response[UART_MAX_PKT_SIZE];

    switch (cmd_id) {
    case CMD_CONNECT:
        _host_connected = (len == 0U) || (data[0] != 0U);
        _last_host_ping_ms = bsp_time_ms();
        if (!_host_connected)
            _stream_count = 0U;
        _reply(services, port, cmd_id, true);
        break;

    case CMD_HEARTBEAT:
        _last_host_ping_ms = bsp_time_ms();
        _heartbeat_send(services, port);
        break;

    case CMD_DATA_STREAM_SET:
        if (len < 1U || data[0] > STREAM_MAX_CHANNELS ||
            (uint8_t)(len - 1U) < data[0]) {
            _reply(services, port, cmd_id, false);
            break;
        }
        _stream_count = data[0];
        for (uint8_t i = 0U; i < _stream_count; i++)
            _stream_ids[i] = data[1U + i];
        _stream_port = port;
        _last_stream_ms = bsp_time_ms();
        _reply(services, port, cmd_id, true);
        break;

    case CMD_PARAM_READ:
        if (len < 1U) {
            _reply(services, port, cmd_id, false);
            break;
        }
        response[0] = data[0];
        {
            uint8_t n = 0U;
            param_get(services->param, (eParameter)data[0], &response[1], &n);
            (void)comm_send_frame(services->comm, port, cmd_id, response, (uint8_t)(n + 1U));
        }
        break;

    case CMD_PARAM_WRITE:
        // payload 0xFF：应用已写入参数，重新初始化控制/观测对象
        if (len == 1U && data[0] == 0xFFU) {
            app_main_config();
            _reply(services, port, cmd_id, true);
            break;
        }
        if (len < 2U) {
            _reply(services, port, cmd_id, false);
            break;
        }
        param_set(services->param, (eParameter)data[0], (uint8_t *)&data[1]);
        _reply(services, port, cmd_id, true);
        break;

    case CMD_PARAM_SAVE:
        _reply(services, port, cmd_id, param_save(services->param));
        _reply(services, port, cmd_id, true);
        break;
    case CMD_PARAM_RELOAD:
        _reply(services, port, cmd_id, param_reset(services->param));
        _reply(services, port, cmd_id, true);
        break;

    case CMD_DATA_ASK:
        if (len < 1U) {
            _reply(services, port, cmd_id, false);
            break;
        }
        {
            uint8_t n = 0U;
            dm_data_get(services->obs, (eObsData)data[0], response, &n);
            (void)comm_send_frame(services->comm, port, cmd_id, response, n);
        }
        break;

    case CMD_DISABLE:
        motor_disable(services->motor);
        break;
    case CMD_CORE_NRST:
        protection_reset(services->protection, services->motor);
        break;
    case CMD_SYSTEM_RESET:
        system_reset();
        break;

    case CMD_SET_ZERO_POS:
        em_set_pos_zero(services->enc);
        break;
    case CMD_SET_LIMIT_POS:
        em_set_pos_limit(services->enc);
        break;

    case CMD_IAP_ENTER: {
        uint32_t version, size;
        if (len != 8U || !_read_u32(data, len, &version) ||
            !_read_u32(data + 4, len - 4, &size) || size > BSP_APP_SIZE ||
            size < 8U || version > 0xFFFFFFU) {
            _reply(services, port, cmd_id, false);
            break;
        }
        motor_reset(services->motor);
        if (!upgrade_save(services->param, version, size)) {
            _reply(services, port, cmd_id, false);
            break;
        }
        _reply(services, port, cmd_id, true);
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
        (void)comm_send_frame(services->comm, port, cmd_id, response, n);
        // TODO：读param中的版本信息
        break;
    case CMD_ASK_DEVICE_MESSAGE:
        // TODO：将驱动器信息编成文本发送
        break;
    case CMD_CTRL_CONFIG:
        _config_ctrl(services, data, len);
        break;
    case CMD_OPENLOOP_CTRL:
        _target_openloop(services, data, len);
        break;
    case CMD_CUR_TAU_CTRL:
        _target_current(services, data, len);
        break;
    case CMD_PID_VEL_CTRL:
        _target_pid_vel(services, data, len);
        break;
    case CMD_PID_POS_CTRL:
        _target_pid_pos(services, data, len);
        break;
    case CMD_MIT_CTRL:
        _target_mit_ctrl(services, data, len);
        break;

    case CMD_RL_TUNNING: {
        uint8_t payload_size;
        float tune_current;
        DEPACK_RL_TUNNING_REQUEST(payload_size, data, tune_current);
        if (payload_size != len)
            protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
        else
            tune_start(services->tune, TUNE_RL, tune_current, 0.0f);
        break;
    }
    case CMD_PSIF_TUNNING: {
        uint8_t payload_size;
        float tune_vel;
        DEPACK_PSIF_TUNNING_REQUEST(payload_size, data, tune_vel);
        if (payload_size != len)
            protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
        else
            tune_start(services->tune, TUNE_Psif, 0.0f, tune_vel);
        break;
    }
    case CMD_MECH_TUNNING: {
        uint8_t payload_size;
        float tune_cur, tune_vel;
        DEPACK_MECH_TUNNING_REQUEST(payload_size, data, tune_cur, tune_vel);
        if (payload_size != len)
            protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
        else
            tune_start(services->tune, TUNE_MECH, tune_cur, tune_vel);
        break;
    }
    case CMD_OBS_TUNNING: {
        uint8_t payload_size;
        float tune_cur, tune_vel;
        DEPACK_OBS_TUNNING_REQUEST(payload_size, data, tune_cur, tune_vel);
        if (payload_size != len)
            protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
        else
            tune_start(services->tune, TUNE_OBS, tune_cur, tune_vel);
        break;
    }
    case CMD_PID_TUNNING: {
        uint8_t payload_size;
        float tune_cur, tune_vel;
        DEPACK_PID_TUNNING_REQUEST(payload_size, data, tune_cur, tune_vel);
        if (payload_size != len)
            protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
        else
            tune_start(services->tune, TUNE_PID, tune_cur, tune_vel);
        break;
    }
    case CMD_MIT_TUNNING: {
        uint8_t payload_size;
        float tune_cur, tune_vel;
        DEPACK_MIT_TUNNING_REQUEST(payload_size, data, tune_cur, tune_vel);
        if (payload_size != len)
            protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
        else
            tune_start(services->tune, TUNE_MIT, tune_cur, tune_vel);
        break;
    }
    default:
        protection_fault(services->protection, services->motor, FAULT_INVALID_INPUT);
        break;
    }
}

static void app_cmd_dispatch(const void *context, ePort port, uint8_t cmd_id,
                             const uint8_t *data, uint8_t len) {
    const tAppCmdServices *services = context;
    if (port == PORT_CAN)
        _process_bus_command(services, port, cmd_id, data, len);
    else
        _process_command(services, port, cmd_id, data, len);
}

void app_cmd_loop_task(const tAppCmdServices *services) {
    if (comm_bus_has_error(services->comm))
        protection_fault(services->protection, services->motor, FAULT_BUSCOM_RUN);

    comm_loop_task(services->comm, app_cmd_dispatch, services);

    if (_host_connected && _last_host_ping_ms > 0U &&
        bsp_time_ms() - _last_host_ping_ms > 1000U) {
        _host_connected = false;
        _stream_count = 0U;
    }

    if (_stream_count > 0U &&
        bsp_time_ms() - _last_stream_ms >= STREAM_INTERVAL_MS) {
        _last_stream_ms = bsp_time_ms();
        _data_stream_send(services, _stream_port);
    }
}
