#include "cmd_process.h"
#include "cmd_list.h"

#include "core.h"
#include "control.h"
#include "parameters.h"
#include "protection.h"

#include "bsp_cfg.h"
#include "bsp_time.h"
#include "stm32g4xx.h"
#include "uart_com.h"
#include "bus_com.h"

#include <string.h>

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

extern tCore core;
extern tCtrl ctrl;

#define STREAM_MAX_CHANNELS 5U

static uint32_t _last_host_ping_ms = 0;
static bool _host_connected = false;

static uint8_t _stream_ids[STREAM_MAX_CHANNELS];
static uint8_t _stream_count = 0;

static bool _param_read_active = false;
static uint8_t _param_read_start = 1U;
static ePort _param_read_port = PORT_USB;

static bool _send_frame(ePort port, uint8_t cmd_id,
                        const uint8_t *data, uint8_t len)
{
    if (port == PORT_CAN)
    {
        tBus_Frame frame;
        frame.id = cmd_id; // TODO(payload): CAN ID 映射规则未确认
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

// data.c 的 g_data 绑定尚未完成，这里直接读 core 的实时量，避免悬空指针。
static bool _data_get(eObsData id, uint8_t *out, uint8_t *len)
{
    if (!out || !len)
        return false;

    float f = 0.0f;
    uint8_t u8 = 0U;
    uint8_t n = 0U;

    switch (id)
    {
    case DATA_CURRENT_U: f = core.fb.is[0]; n = 4U; break;
    case DATA_CURRENT_V: f = core.fb.is[1]; n = 4U; break;
    case DATA_CURRENT_W: f = core.fb.is[2]; n = 4U; break;
    case DATA_VOLTAGE_Q: f = core.fb.uq; n = 4U; break;
    case DATA_VOLTAGE_D: f = core.fb.ud; n = 4U; break;
    case DATA_CURRENT_ALPHA: f = core.fb.ialpha; n = 4U; break;
    case DATA_CURRENT_BETA: f = core.fb.ibeta; n = 4U; break;
    case DATA_CURRENT_Q: f = core.fb.iq; n = 4U; break;
    case DATA_CURRENT_D: f = core.fb.id; n = 4U; break;
    case DATA_CURRENT_Q_REF: f = core.ref.iq; n = 4U; break;
    case DATA_CURRENT_D_REF: f = core.ref.id; n = 4U; break;
    case DATA_VELOCITY: f = core.fb.vel; n = 4U; break;
    case DATA_VELOCITY_REF: f = core.ref.vel; n = 4U; break;
    case DATA_THETA_ELEC: f = core.fb.theta_elec; n = 4U; break;
    case DATA_THETA_MECH: f = core.fb.theta_rotor; n = 4U; break;
    case DATA_POSITION: f = core.fb.pos; n = 4U; break;
    case DATA_POSITION_REF: f = core.ref.pos; n = 4U; break;
    case DATA_MINUTES: f = (float)(bsp_time_ms() / 60000U); n = 4U; break;
    case DATA_SECONDS: f = (float)((bsp_time_ms() / 1000U) % 60U); n = 4U; break;
    case DATA_STATE: u8 = (uint8_t)core.state; n = 1U; break;
    case DATA_FAULT: u8 = (uint8_t)core.fault; n = 1U; break;
    case DATA_WARNING: u8 = (uint8_t)core.warning; n = 1U; break;
    case DATA_VBUS: f = core.fb.udc; n = 4U; break;
    case DATA_DRIVER_TEMP: f = core.fb.temp; n = 4U; break;
    case DATA_MOTOR_TEMP: f = core.fb.temp; n = 4U; break; // TODO: 无独立电机温度源
    default: return false;
    }

    if (n == 1U)
        *out = u8;
    else
        memcpy(out, &f, sizeof(float));
    *len = n;
    return true;
}

// ---------------- 参数 ----------------
static void _param_send_tick(void)
{
    if (!_param_read_active)
        return;

    if (_param_read_port == PORT_CAN)
    {
        // TODO(payload): CAN 8 字节载荷无法承载批量参数
        _param_read_active = false;
        return;
    }

    uint8_t payload[UART_MAX_PKT_SIZE];
    if (_param_read_start >= PARAM_NUM)
    {
        payload[0] = _param_read_start;
        payload[1] = 0U;
        if (_send_frame(_param_read_port, CMD_PARAM_ALLREAD, payload, 2U))
            _param_read_active = false;
        return;
    }

    uint8_t id = _param_read_start;
    uint8_t pos = 2U;
    uint8_t count = 0U;
    while (id < PARAM_NUM)
    {
        uint8_t tmp[8];
        uint8_t n = 0U;
        param_get((eParameter)id, tmp, &n);
        if (n == 0U || (uint16_t)pos + n > UART_MAX_PKT_SIZE)
            break;
        memcpy(&payload[pos], tmp, n);
        pos += n;
        count++;
        id++;
    }

    payload[0] = _param_read_start;
    payload[1] = count;
    if (count == 0U)
    {
        _param_read_active = false;
        return;
    }

    if (_send_frame(_param_read_port, CMD_PARAM_ALLREAD, payload, pos))
        _param_read_start = id;
}

static bool _param_write_all(const uint8_t *data, uint8_t len)
{
    if (!data || len < 2U)
        return false;
    uint8_t start = data[0];
    uint8_t count = data[1];
    uint16_t pos = 2U;
    for (uint8_t i = 0U; i < count; i++)
    {
        uint8_t id = (uint8_t)(start + i);
        if (id >= PARAM_NUM)
            return false;

        uint8_t tmp[8];
        uint8_t n = 0U;
        param_get((eParameter)id, tmp, &n);
        if (n == 0U || (uint16_t)pos + n > len)
            return false;
        param_set((eParameter)id, (uint8_t *)&data[pos]);
        pos += n;
    }
    return true;
}

// ---------------- 数据流 / 心跳 ----------------
static void _stream_send(ePort port)
{
    static const eObsData status_ids[] = {
        DATA_STATE, DATA_FAULT, DATA_WARNING,
        DATA_VBUS, DATA_DRIVER_TEMP, DATA_MOTOR_TEMP,
    };

    uint8_t payload[UART_MAX_PKT_SIZE];
    uint8_t pos = 1U;
    payload[0] = _stream_count; // 订阅的示波通道数（最多 5）

    // 固定状态块：state/fault/warning/vbus/驱动温度/电机温度
    for (uint8_t i = 0U; i < (uint8_t)(sizeof(status_ids) / sizeof(status_ids[0])); i++)
    {
        uint8_t tmp[8];
        uint8_t n = 0U;
        if (!_data_get(status_ids[i], tmp, &n))
            return;
        if ((uint16_t)pos + n > UART_MAX_PKT_SIZE)
            return;
        memcpy(&payload[pos], tmp, n);
        pos += n;
    }

    // 示波订阅数据，顺序与 CMD_DATA_STREAM_SET 一致
    for (uint8_t i = 0U; i < _stream_count; i++)
    {
        uint8_t tmp[8];
        uint8_t n = 0U;
        if (!_data_get((eObsData)_stream_ids[i], tmp, &n))
            break;
        if ((uint16_t)pos + n > UART_MAX_PKT_SIZE)
            break;
        memcpy(&payload[pos], tmp, n);
        pos += n;
    }
    (void)_send_frame(port, CMD_HEARTBEAT, payload, pos); // TODO(payload): CAN 8 字节需拆分
}

// ---------------- 控制目标 ----------------
static void _target_openloop(ePort port, uint8_t cmd_id,
                             const uint8_t *data, uint8_t len)
{
    float duty = 0.0f;
    float value = 0.0f;
    if (len < 8U ||
        !_read_f32(data, 4U, &duty) ||
        !_read_f32(&data[4], (uint8_t)(len - 4U), &value))
    {
        _reply(port, cmd_id, false);
        return;
    }

    core.ref.duty = duty;
    core.state = CORE_OPENLOOP;
    core.enable = true;
    core.oc_enable = false;
    if (cmd_id == CMD_OPENLOOP_THETA)
    {
        core_ot_cmd_set_theta(value);
        core.ref.theta_elec = value;
    }
    else
    {
        core_ot_cmd_set_vel(value);
    }
    _reply(port, cmd_id, true);
}

static void _target_current(ePort port, uint8_t cmd_id,
                            const uint8_t *data, uint8_t len)
{
    if (len < 5U)
    {
        _reply(port, cmd_id, false);
        return;
    }

    bool is_torque = data[0] != 0U;
    float value = 0.0f;
    if (!_read_f32(&data[1], (uint8_t)(len - 1U), &value))
    {
        _reply(port, cmd_id, false);
        return;
    }

    core.state = CORE_CURRENT_MODE;
    core.enable = true;
    core.oc_enable = false;
    core.ref.id = 0.0f;
    if (is_torque)
    {
        core.ref.tau = value;
        core.ref.iq = (g_param.motor_ke != 0.0f) ? value / g_param.motor_ke : 0.0f;
    }
    else
    {
        core.ref.tau = 0.0f;
        core.ref.iq = value;
    }
    _reply(port, cmd_id, true);
}

static void _target_single(ePort port, uint8_t cmd_id,
                           const uint8_t *data, uint8_t len)
{
    float value = 0.0f;
    if (!_read_f32(data, len, &value))
    {
        _reply(port, cmd_id, false);
        return;
    }

    core.enable = true;
    core.oc_enable = false;
    if (cmd_id == CMD_PID_VEL)
    {
        core.state = CORE_PID_SPEED;
        core.ref.vel = value;
    }
    else
    {
        core.state = CORE_PID_POSITION;
        core.ref.pos = value;
    }
    _reply(port, cmd_id, true);
}

static void _target_mit_pos(ePort port, uint8_t cmd_id,
                            const uint8_t *data, uint8_t len)
{
    if (len >= 20U)
    {
        float kp, kd, pos, vel, tau_ff;
        memcpy(&kp, &data[0], sizeof(float));
        memcpy(&kd, &data[4], sizeof(float));
        memcpy(&pos, &data[8], sizeof(float));
        memcpy(&vel, &data[12], sizeof(float));
        memcpy(&tau_ff, &data[16], sizeof(float));
        ctrl.mit.Kp = kp;
        ctrl.mit.Kd = kd;
        core.ref.pos = pos;
        core.ref.vel = vel;
        core.ref.tau_ff = tau_ff;
    }
    else if (len >= 4U)
    {
        float pos;
        memcpy(&pos, data, sizeof(float));
        core.ref.pos = pos;
    }
    else
    {
        _reply(port, cmd_id, false);
        return;
    }

    core.state = CORE_MIT_MODE;
    core.enable = true;
    core.oc_enable = false;
    _reply(port, cmd_id, true);
}

static void _mit_config(ePort port, uint8_t cmd_id,
                        const uint8_t *data, uint8_t len)
{
    if (cmd_id == CMD_MIT_CONFIG)
    {
        if (len < 8U)
        {
            _reply(port, cmd_id, false);
            return;
        }
        memcpy(&ctrl.mit.Kp, &data[0], sizeof(float));
        memcpy(&ctrl.mit.Kd, &data[4], sizeof(float));
        _reply(port, cmd_id, true);
    }
    else // CMD_MIT_VT
    {
        if (len < 8U)
        {
            _reply(port, cmd_id, false);
            return;
        }
        memcpy(&core.ref.vel, &data[0], sizeof(float));
        memcpy(&core.ref.tau_ff, &data[4], sizeof(float));
        _reply(port, cmd_id, true);
    }
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
        _stream_send(port);
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
    case CMD_PARAM_ALLRELOAD:
        _reply(port, cmd_id, param_reset());
        break;

    case CMD_PARAM_ALLREAD:
        if (port == PORT_CAN)
        {
            // TODO(payload): CAN 不适合批量参数回传
            _reply(port, cmd_id, false);
            break;
        }
        _param_read_active = true;
        _param_read_start = 1U;
        _param_read_port = port;
        break;

    case CMD_PARAM_ALLWRITE:
        _reply(port, cmd_id, _param_write_all(data, len));
        break;

    case CMD_DATA_ASK:
        if (len < 1U)
        {
            _reply(port, cmd_id, false);
            break;
        }
        {
            uint8_t n = 0U;
            if (!_data_get((eObsData)data[0], response, &n))
            {
                _reply(port, cmd_id, false);
                break;
            }
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
        // TODO: 当前 obs 未公开设置零点接口
        _reply(port, cmd_id, false);
        break;
    case CMD_SET_LIMIT_POS:
        {
            // 无 payload：当前位置作为极限位置；正位置设上限，负位置设下限。
            float pos = core.fb.pos;
            if (pos > 0.0f)
                g_param.pos_max_limit = pos;
            else
                g_param.pos_min_limit = pos;
            pro_set_limit_position(g_param.pos_min_limit, g_param.pos_max_limit);
            _reply(port, cmd_id, true);
        }
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

    case CMD_OPENLOOP_THETA:
    case CMD_OPENLOOP_VEL:
        _target_openloop(port, cmd_id, data, len);
        break;
    case CMD_CUR_TAU_MODE:
        _target_current(port, cmd_id, data, len);
        break;
    case CMD_PID_VEL:
    case CMD_PID_POS:
        _target_single(port, cmd_id, data, len);
        break;
    case CMD_MIT_POS:
        _target_mit_pos(port, cmd_id, data, len);
        break;
    case CMD_MIT_CONFIG:
    case CMD_MIT_VT:
        _mit_config(port, cmd_id, data, len);
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
    tUart_Frame frame;
    while (uart_process_frame(&g_uart, &frame))
        _process_command(PORT_UART, frame.id, frame.data, frame.data_len);
    while (uart_process_frame(&g_usb, &frame))
        _process_command(PORT_USB, frame.id, frame.data, frame.data_len);

    tBus_Frame bus_frame;
    while (bus_process_frame(&g_can, &bus_frame))
    {
        if (bus_frame.data_len == 0U)
            continue;
        _process_command(PORT_CAN, bus_frame.data[0],
                         &bus_frame.data[1],
                         (uint8_t)(bus_frame.data_len - 1U));
    }

    if (_host_connected && _last_host_ping_ms > 0U &&
        bsp_time_ms() - _last_host_ping_ms > 1000U)
    {
        _host_connected = false;
        _stream_count = 0U;
    }

    _param_send_tick();
}
