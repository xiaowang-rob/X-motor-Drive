
#include "cmd_list.h"
#include "parameter_list.h"

#include "bsp_cfg.h"
#include "bus_com.h"

#include "obsdata.h"
#include "parameters.h"

#include "core.h"

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

typedef enum
{
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
#define STREAM_INTERVAL_MS 200U

static bool _send_frame(ePort port, uint8_t cmd_id,
                        const uint8_t *data, uint8_t len)
{
    if (port == PORT_CAN)
    {
        tBus_Frame frame;
        frame.id = cmd_id;
        frame.data_len = (len > sizeof(frame.data)) ? (uint8_t)sizeof(frame.data) : len;
        if (frame.data_len && data)
            memcpy(frame.data, data, frame.data_len);
        return bus_send(&g_can, &frame);
    }

    tUart_Frame frame;
    frame.id = cmd_id;
    frame.data_len = len;
    if (len && data)
        memcpy(frame.data, data, len);
    return uart_send((port == PORT_UART) ? &g_uart : &g_usb, &frame);
}

static void _reply(ePort port, uint8_t cmd_id, bool ok)
{
    uint8_t code = ok ? FB_EXECUTE : FB_FAILURE;
    (void)_send_frame(port, cmd_id, &code, 1U);
}

static bool _read_f32(const uint8_t *data, uint8_t len, float *value)
{
    if (!data || !value || len < sizeof(float))
        return false;
    memcpy(value, data, sizeof(float));
    return true;
}

static bool _read_u32(const uint8_t *data, uint8_t len, uint32_t *value)
{
    if (!data || !value || len < sizeof(uint32_t))
        return false;
    memcpy(value, data, sizeof(uint32_t));
    return true;
}

// ---------------- 状态心跳 / 示波数据流 ----------------
static void _heartbeat_send(ePort port)
{

    uint8_t core_sta, core_fault, core_warning;
    float vbus, dtemp, mtemp;
    uint8_t send_len;
    dm_data_get(DATA_STATE, &core_sta, &send_len);
    dm_data_get(DATA_FAULT, &core_fault, &send_len);
    dm_data_get(DATA_WARNING, &core_warning, &send_len);
    dm_data_get(DATA_VBUS, &vbus, &send_len);
    dm_data_get(DATA_DRIVER_TEMP, &dtemp, &send_len);
    dm_data_get(DATA_MOTOR_TEMP, &mtemp, &send_len);

    uint8_t payload[UART_MAX_PKT_SIZE];
    ENPACK_HEARTBEAT_RESPONSE(send_len, payload, core_sta, core_fault, core_warning, vbus, dtemp, mtemp);

    (void)_send_frame(port, CMD_HEARTBEAT, payload, send_len);
}

static void _data_stream_send(ePort port)
{
    uint8_t payload[UART_MAX_PKT_SIZE];
    uint8_t pos = 1U;
    payload[0] = _stream_count;
    for (uint8_t i = 0U; i < _stream_count; i++)
    {
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

// ---------------- 控制目标 ----------------
static void _target_openloop(const uint8_t *data, uint8_t len)
{
    // TODO:长度校验

    rx_cmd.mode = CORE_OPENLOOP;
    uint8_t payload_size;
    DEPACK_OPENLOOP_CTRL_REQUEST(payload_size, data, rx_cmd.openloop.theta_or_vel, rx_cmd.openloop.duty, rx_cmd.openloop.value);
    (void)payload_size; // 长度校验由 process 处理

    core_cmd_set(&rx_cmd);
}

static void _target_current(const uint8_t *data, uint8_t len)
{

    rx_cmd.mode = CORE_CURRENT_MODE;
    DEPACK_CUR_TAU_CTRL_REQUEST(len, data, rx_cmd.current_ctrl.cur_or_tau, rx_cmd.current_ctrl.value);

    core_cmd_set(&rx_cmd);
}
static void _target_pid_vel(const uint8_t *data, uint8_t len)
{
    rx_cmd.mode = CORE_PID_SPEED;
    DEPACK_CUR_TAU_CTRL_REQUEST(len, data, rx_cmd.current_ctrl.cur_or_tau, rx_cmd.current_ctrl.value);

    core_cmd_set(&rx_cmd);
}

static void _target_pid_pos(const uint8_t *data, uint8_t len)
{
    rx_cmd.mode = CORE_PID_POSITION;
    DEPACK_CUR_TAU_CTRL_REQUEST(len, data, rx_cmd.current_ctrl.cur_or_tau, rx_cmd.current_ctrl.value);

    core_cmd_set(&rx_cmd);
}

static void _target_mit_ctrl(const uint8_t *data, uint8_t len)
{
    rx_cmd.mode = CORE_MIT_MODE;
    DEPACK_CUR_TAU_CTRL_REQUEST(len, data, rx_cmd.current_ctrl.cur_or_tau, rx_cmd.current_ctrl.value);

    core_cmd_set(&rx_cmd);
}

static void _target_mit_config(const uint8_t *data, uint8_t len)
{
    rx_cmd.mode = CORE_MIT_MODE;
    DEPACK_CUR_TAU_CTRL_REQUEST(len, data, rx_cmd.current_ctrl.cur_or_tau, rx_cmd.current_ctrl.value);

    core_cmd_set(&rx_cmd);
}

static void _mit_config(const uint8_t *data, uint8_t len)
{
    rx_cmd.mode = CORE_MIT_MODE;
    DEPACK_CUR_TAU_CTRL_REQUEST(len, data, rx_cmd.current_ctrl.cur_or_tau, rx_cmd.current_ctrl.value);

    core_cmd_set(&rx_cmd);
}

// ---------------- 命令分发 ----------------
static void _process_command(ePort port, uint8_t cmd_id,
                             const uint8_t *data, uint8_t len)
{
    uint8_t response[UART_MAX_PKT_SIZE];

    switch (cmd_id)
    {
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
            (uint8_t)(len - 1U) < data[0])
        {
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
        if (len < 1U)
        {
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
        if (len == 1U && data[0] == 0xFFU)
        {
            core_init();
            _reply(port, cmd_id, true);
            break;
        }
        if (len < 2U)
        {
            _reply(port, cmd_id, false);
            break;
        }
        param_set((eParameter)data[0], (uint8_t *)&data[1]);
        _reply(port, cmd_id, true);
        break;

    case CMD_PARAM_SAVE:
        _reply(port, cmd_id, param_save());
        break;
    case CMD_PARAM_RELOAD:
        _reply(port, cmd_id, param_reset());
        break;

    case CMD_DATA_ASK:
        if (len < 1U)
        {
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
        core_disable();
        _reply(port, cmd_id, true);
        break;
    case CMD_CORE_NRST:
        core_disable();
        _reply(port, cmd_id, true);
        break;
    case CMD_SYSTEM_RESET:
        NVIC_SystemReset();
        break;

    case CMD_SET_ZERO_POS:
        core_set_pos_zero();
        break;
    case CMD_SET_LIMIT_POS:
        core_set_pos_limit();
        break;

    case CMD_IAP_ENTER:
        // TODO(payload): 进入 IAP 前应答/版本信息
        (void)iap_jump(&g_iap);
        break;
    case CMD_IAP_ERASE_FLASH:
    {
        uint32_t size = 0U;
        (void)_read_u32(data, len, &size); // TODO(payload): size 仅作协议预留
        _reply(port, cmd_id, iap_erase(&g_iap));
    }
    break;
    case CMD_IAP_WRITE_FLASH:
    {
        uint32_t offset = 0U;
        if (!_read_u32(data, len, &offset) || len <= 4U)
        {
            _reply(port, cmd_id, false);
            break;
        }
        bool ok = iap_write(&g_iap, offset, &data[4], (uint32_t)(len - 4U));
        response[0] = (uint8_t)(offset & 0xFFU);
        response[1] = (uint8_t)((offset >> 8) & 0xFFU);
        response[2] = (uint8_t)((offset >> 16) & 0xFFU);
        response[3] = (uint8_t)((offset >> 24) & 0xFFU);
        response[4] = ok ? FB_EXECUTE : FB_FAILURE;
        (void)_send_frame(port, cmd_id, response, 5U);
    }
    break;
    case CMD_IAP_VERIFY_FLASH:
    {
        uint32_t crc = 0U;
        uint32_t size = 0U;
        if (len < 8U || !_read_u32(data, 4U, &crc) ||
            !_read_u32(&data[4], (uint8_t)(len - 4U), &size))
        {
            _reply(port, cmd_id, false);
            break;
        }
        _reply(port, cmd_id, iap_verify_crc(&g_iap, crc)); // TODO(payload): size 预留
    }
    break;
    case CMD_IAP_EXIT:
        // TODO: iap_reset 当前未实现，先用系统复位退出升级流程
        NVIC_SystemReset();
        break;

    case CMD_ASK_BL_VERSION:
    case CMD_ASK_APP_VERSION:
    case CMD_ASK_DEVICE_MESSAGE:
        (void)_send_frame(port, cmd_id,
                          (const uint8_t *)XDR_VERSION_STR,
                          (uint8_t)strlen(XDR_VERSION_STR));
        break;

    case CMD_OPENLOOP_CTRL:
        _target_openloop(data, len);
        break;
    case CMD_CUR_TAU_CTRL:
        _target_current(data, len);
        break;
    case CMD_PID_VEL_CTRL:
    case CMD_PID_POS_CTRL:
        _target_single(data, len);
        break;
    case CMD_MIT_CTRL:
        _target_mit_ctrl(data, len);
        break;
    case CMD_MIT_CONFIG:
        _mit_config(data, len);
        break;

    case CMD_RL_TUNNING:
    case CMD_PSIF_TUNNING:
    case CMD_MECH_TUNNING:
    case CMD_OBS_TUNNING:
    case CMD_PID_TUNNING:
    case CMD_MIT_TUNNING:
        // TODO: 校准服务入口/全局 tTune 尚未在 cmd_process 之外初始化
        _reply(port, cmd_id, false);
        break;

    default:
        // TODO: 未处理命令
        break;
    }
}

void comm_process(void)
{
    tBus_Frame bus_frame;
    while (bus_process_frame(&g_can, &bus_frame))
    {
        if (bus_frame.data_len == 0U)
            continue;
        _process_command(PORT_CAN, bus_frame.data[0],
                         &bus_frame.data[1],
                         (uint8_t)(bus_frame.data_len - 1U));
    }

    tUart_Frame frame;
    while (uart_process_frame(&g_uart, &frame))
        _process_command(PORT_UART, frame.id, frame.data, frame.data_len);
    while (uart_process_frame(&g_usb, &frame))
        _process_command(PORT_USB, frame.id, frame.data, frame.data_len);

    if (_host_connected && _last_host_ping_ms > 0U &&
        bsp_time_ms() - _last_host_ping_ms > 1000U)
    {
        _host_connected = false;
        _stream_count = 0U;
    }

    if (_stream_count > 0U &&
        bsp_time_ms() - _last_stream_ms >= STREAM_INTERVAL_MS)
    {
        _last_stream_ms = bsp_time_ms();
        _data_stream_send(_stream_port);
    }
}
