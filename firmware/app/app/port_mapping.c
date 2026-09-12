// USB、串口、CAN 端口映射
#include "port_mapping.h"
#include "usr_config.h"

#include "DataMonitoring.h"
#include "foc_main.h"
#include "parameter_manager.h"
#include "log.h"
#include "protection_manager.h"
#include "can_port.h"
#include "usb_port.h"
#include "uart_port.h"
#include "string.h"
#include "stdio.h"
#include "tune.h"
#include "bsp_base.h"
#include "bsp_flash.h"

#define STR(x) #x
#define XSTR(x) STR(x)

// 多端口接收缓冲 — 每个端口独立，避免互斥
#define PORT_NUM 3
static struct
{
    eCOM port_id;
    u8 cmd_id;
    u8 rxbuffer[MAX_FRAME_LENGTH];
    u8 rxlen;
    bool pending;
} g_port_rx[PORT_NUM] = {
    {.port_id = CAN_port},
    {.port_id = USB_port},
    {.port_id = UART_port},
};

// 下发帧（应答/状态/数据）缓冲 — 互斥访问，由主循环使用
static tCOM_Frame com_frame;
tCommunicationState g_com_state = {.com_port = &com_frame.com_port, .is_busy = &com_frame.is_busy};

const u8 EXECUTE = FEEDBACK_EXECUTE;
const u8 FAILURE = FEEDBACK_FAILURE;

static u32 _last_host_ping_ms = 0; // 上次收到主机心跳时间
static bool system_message_send_flag = false;

// 通讯层初始化
void comm_init()
{
    if (can_port_init(g_Param.can_id, g_Param.sw_canqueue))
        g_device_status.can_state = ONLINE;
    else
        g_device_status.can_state = OFFLINE;

    uart_port_init();
    usb_init();
    g_com_state.Host_port = NONE_port;
}

// 写入can配置参数
void comm_write_can_config(u32 CAN_ID, bool canQUEUE)
{
    if (can_set_config(CAN_ID, canQUEUE))
        g_device_status.can_state = ONLINE;
    else
        g_device_status.can_state = OFFLINE;
}
// 上位机发送 缓存中的数据
void comm_host_send()
{
    if (com_frame.com_port == UART_port)
        uart_port_send_frame(com_frame.cmd_id, com_frame.txdata, com_frame.txdatalen);
    else if (!usb_send_frame(com_frame.cmd_id, com_frame.txdata, com_frame.txdatalen))
    {
        g_com_state.Host_port = NONE_port;
        g_device_status.usb_state = OFFLINE;
        system_message_send_flag = false;
        com_frame.stream_num = 0;
    }
}
// 参数发送
static bool param_send_flag = false;
static u8 param_index = 0;
static inline void _all_params_send()
{
    param_get((eParameter)param_index, &com_frame.txdata[1], &com_frame.txdatalen);
    com_frame.txdata[0] = param_index;
    com_frame.txdatalen += 1;
    comm_host_send();
    param_index++;
    if (param_index == PARAM_NUM)
    {
        param_index = 0;
        param_send_flag = false;
    }
}
// 日志发送
static bool log_send_flag = false;
static inline void _all_log_send()
{
    if (log_read_flash(com_frame.txdata, &com_frame.txdatalen))
        log_send_flag = false;
    else
        comm_host_send();
}
// 状态发送
static inline void _status_send()
{
    com_frame.cmd_id = UC_CONNECT;
    if (system_message_send_flag == false)
    {
        static char drive_msg[128] = {0};
        if (drive_msg[0] == '\0')
        {
            snprintf(drive_msg, sizeof(drive_msg),
                     "%s,%s,%s,%s,%s,%s,%s,%s,%s-%s,%s",
                     FIRM_NAME,
                     FIRM_V_DATE,
                     FIRM_AUTHOR,
                     XSTR(F_PWM),
                     XSTR(F_CURRENT),
                     XSTR(F_SPEED),
                     XSTR(F_POSITION),
                     XSTR(MAX_CURRENT),
                     XSTR(MIN_VOLTAGE),
                     XSTR(MAX_VOLTAGE),
                     XSTR(MAX_TEMPERATURE));
        }
        com_frame.txdata[0] = '\0';
        strcat((char *)com_frame.txdata, drive_msg);
        com_frame.txdatalen = strlen((char *)com_frame.txdata);
        system_message_send_flag = true;
    }
    else
    {
        // 状态包 对应 usr_config.json 中的状态包顺序
        com_frame.txdata[0] = tune_get_fault();
        com_frame.txdata[1] = g_foc.state;
        com_frame.txdata[2] = g_pro_manager.fault;
        com_frame.txdata[3] = g_pro_manager.warning;

        memcpy(&com_frame.txdata[4], &g_pro_manager.foc_val->temp, sizeof(float));
        memcpy(&com_frame.txdata[8], &g_pro_manager.foc_val->udc, sizeof(float));
        com_frame.txdatalen = 12;
    }
    // 超时检测：基于实际时间（5秒无心跳断开）
    if (_last_host_ping_ms > 0 && bsp_get_tick() - _last_host_ping_ms > 5000)
    {
        g_com_state.Host_port = NONE_port;
        system_message_send_flag = false;
        com_frame.stream_num = 0;
        _last_host_ping_ms = 0;
        return;
    }
    comm_host_send();
}

static u8 data_id = 0;
static float value_ref[2];
// 命令解析
static void _frame_data_deal()
{
    if (com_frame.com_port == CAN_port)
    {
        switch (com_frame.cmd_id)
        {
        case CMD_REFVALUE_SET:
            foc_set_target((float *)com_frame.rxdata);
            break;
        case CMD_ENABLE:
            foc_state_update(FOC_ENABLE);
            break;
        case CMD_DISABLE:
            foc_state_update(FOC_DISABLE);
            break;
        case CMD_MODE_SET:
            foc_set_run_mode(com_frame.rxdata[0]);
            break;
        case CMD_STREAM_GET:
            data_id = com_frame.rxdata[0];
            stream_data_get((eData_stream)data_id, (float *)com_frame.txdata);
            can_send_data(com_frame.txdata, 4);
            break;
        case CMD_SYSTEM_RESET:
            bsp_system_reset();
            break;
        case CMD_SET_ZERO_POS:
             foc_set_zero_pos(); // 以当前位置为0点
            break;
        case CMD_SET_LIMIT_POS:
            foc_set_limit_pos();
            break;
        default:
            break;
        }
    }
    else // usb or usart
    {
        if (com_frame.rxdatalen == 0)
        {
            switch (com_frame.cmd_id)
            {
            case UC_CONNECT:
                if (g_com_state.Host_port == NONE_port)
                {
                    if (com_frame.com_port == USB_port)
                        g_device_status.usb_state = ONLINE;
                    g_com_state.Host_port = com_frame.com_port;
                }
                _last_host_ping_ms = bsp_get_tick();
                break;
            case UC_DISCONNECT:
                system_message_send_flag = false;
                g_device_status.usb_state = OFFLINE;
                g_com_state.Host_port = NONE_port;
                com_frame.stream_num = 0;
                break;
            case START_TUNNING:
                foc_state_update(FOC_TUNE);
                break;
            case BRAKE:
                foc_state_update(FOC_SHUTDOWN);
                break;
            case FOC_NRST:
                foc_state_update(FOC_RESET);
                break;
            case CMD_ENABLE:
                foc_state_update(FOC_ENABLE);
                break;
            case CMD_DISABLE:
                foc_state_update(FOC_DISABLE);
                break;
            case LOG_GET:
                log_send_flag = true;
                break;
            case LOG_ERASE:
                log_erase();
                com_frame.txdata[0] = EXECUTE;
                com_frame.txdatalen = 1;
                comm_host_send();
                break;
            case PARAM_ERASE:
                if (param_erase())
                    com_frame.txdata[0] = EXECUTE;
                else
                    com_frame.txdata[0] = FAILURE;
                com_frame.txdatalen = 1;
                comm_host_send();
                break;
            case PARAM_SAVE: // 一键保存
                if (param_save())
                    com_frame.txdata[0] = EXECUTE;
                else
                    com_frame.txdata[0] = FAILURE;
                com_frame.txdatalen = 1;
                comm_host_send();
                break;
            case CMD_STREAM_SET: // 除了状态位清除检测值
                com_frame.stream_num = 0;
                break;
            case CMD_SET_ZERO_POS:
                 foc_set_zero_pos(); // 以当前位置为0点
                break;
            case CMD_SET_LIMIT_POS:
                 foc_set_limit_pos(); // 以当前位置为极限位置
                break;
            case CMD_SYSTEM_RESET: // 系统复位
                bsp_system_reset();
                break;

            case CMD_IAP_ENTER:
                memset(com_frame.txdata, 0, 24);
                strcat((char *)com_frame.txdata, FIRM_VERSION);
                if (bsp_jump_to_bootloader(com_frame.txdata, 24))
                    com_frame.txdata[0] = EXECUTE;
                else
                    com_frame.txdata[0] = FAILURE;
                com_frame.txdatalen = 1;
                comm_host_send();
                break;
            default:
                break;
            }
        }
        else if (com_frame.rxdatalen <= 5 || com_frame.rxdatalen == 8)
        {
            switch (com_frame.cmd_id)
            {
            case PARAM_WRITE: // 指定写入
                param_set(com_frame.rxdata[0], &com_frame.rxdata[1]);
                break;
            case PARAM_READ:
                if (com_frame.rxdata[0] == 0xff)
                { // 读取所有参数
                    param_send_flag = true;
                    break;
                } // 指定读取
                param_get(com_frame.rxdata[0], com_frame.txdata, &com_frame.txdatalen);
                comm_host_send();
                break;
            case CMD_REFVALUE_SET: // 目标值设置 4byte||8byte
                memcpy(value_ref, com_frame.rxdata, 4);
                value_ref[1] = 0.0f;
                foc_set_target(value_ref);
                break;
            case CMD_MODE_SET: // 模式设置 1byte
                foc_set_run_mode(com_frame.rxdata[0]);
                break;
            case CMD_STREAM_GET: // 监测值获取 单个值直接获取 1byte
                stream_data_get(com_frame.rxdata[1], (float *)com_frame.txdata);
                com_frame.txdatalen = 4;
                comm_host_send();
                break;
            case CMD_STREAM_SET: // 监测值设置 5byte
                com_frame.stream_num = com_frame.rxdatalen;
                for (u8 i = 0; i < com_frame.stream_num; i++)
                    com_frame.data_id_index[i] = com_frame.rxdata[i];

                break;
            default:
                break;
            }
        }
    }
    com_frame.is_busy = false;
}
// 端口映射 — 每个回调只做拷贝 + 挂起，由主循环统一处理
void can_rx_data_callback(u8 *RxData, u8 len)
{
    if (g_port_rx[0].pending) // CAN is index 0
        return;
    if (len > MAX_FRAME_LENGTH)
        return;
    // 拷贝数据到端口自己的缓冲
    g_device_status.can_state = RUNNING;
    memcpy(g_port_rx[0].rxbuffer, RxData, len);
    g_port_rx[0].rxlen = len;
    g_port_rx[0].pending = true;
}

void usb_rx_frame_callback(u8 id, u8 *data, u8 len)
{
    if (g_port_rx[1].pending) // USB is index 1
        return;
    if (len > MAX_FRAME_LENGTH)
        return;
    g_device_status.usb_state = RUNNING;
    g_port_rx[1].cmd_id = id;
    memcpy(g_port_rx[1].rxbuffer, data, len);
    g_port_rx[1].rxlen = len;
    g_port_rx[1].pending = true;
}

void uart_rx_frame_callback(u8 id, u8 *data, u8 len)
{
    if (g_port_rx[2].pending) // UART is index 2
        return;
    if (len > MAX_FRAME_LENGTH)
        return;
    g_port_rx[2].cmd_id = id;
    memcpy(g_port_rx[2].rxbuffer, data, len);
    g_port_rx[2].rxlen = len;
    g_port_rx[2].pending = true;
}

static u32 _time_ms = 0;
static u32 _time_prev_ms = 0;
static u32 _state_prev_ms = 0;
static u8 _datanum = 0;
void _stream_data_trans()
{
    if (com_frame.is_busy) // 端口忙
    {
        _state_prev_ms = bsp_get_tick();
        return;
    }
    _time_ms = bsp_get_tick();

    // 参数发送
    if (param_send_flag)
    {
        if (_time_ms - _time_prev_ms < T_DATA_STREAM)
            return;
        _all_params_send();
        _time_prev_ms = bsp_get_tick();
        return;
    }
    // 日志发送
    if (log_send_flag)
    {
        if (_time_ms - _time_prev_ms < T_DATA_STREAM)
            return;
        _all_log_send();
        _time_prev_ms = bsp_get_tick();
        return;
    }

    if (g_com_state.Host_port != NONE_port)
    { // 上位机连接状态下
        if ((_time_ms - _state_prev_ms > T_STATE_STREAM))
        { // 状态发送
            _status_send();
            _state_prev_ms = _time_ms;
            _time_prev_ms = _time_ms;
        }
        else if ((_time_ms - _time_prev_ms > T_DATA_STREAM))
        { // 数据发送
            if (com_frame.stream_num == 0)
                return;
            bool txflag = _datanum >= 12 / com_frame.stream_num * com_frame.stream_num - 1;
            stream_data_prepare(com_frame.data_id_index[_datanum % com_frame.stream_num], _datanum, com_frame.txdata, txflag);
            _datanum++;
            if (txflag)
            {
                com_frame.cmd_id = CMD_STREAM_SET;
                com_frame.txdatalen = _datanum * 4;
                comm_host_send();
                _datanum = 0;
            }
            _time_prev_ms = _time_ms;
        }
    }
    else
    { // vofa端口
        if (com_frame.stream_num == 0)
            return;
        if ((_time_ms - _time_prev_ms < T_DATA_STREAM))
            return;
        _time_prev_ms = _time_ms;
        for (u8 i = 0; i < com_frame.stream_num; i++)
        {
            stream_data_get(com_frame.data_id_index[i], (float *)&com_frame.txdata[i * 4]);
        }
        vofa_float_data_send((float *)com_frame.txdata, com_frame.stream_num);
    }
}

// 轮询各端口缓冲，处理待处理的数据包
static void _process_pending_rx(void)
{
    for (int i = 0; i < PORT_NUM; i++)
    {
        if (!g_port_rx[i].pending)
            continue;

        u8 *buf = g_port_rx[i].rxbuffer;
        u8 len = g_port_rx[i].rxlen;
        u8 id = g_port_rx[i].cmd_id;

        if (g_port_rx[i].port_id == CAN_port)
        {
            // CAN 帧格式特殊：cmd_id 由长度决定，需要重新解析
            // 但数据已经拷贝到 buffer，直接交给 _frame_data_deal 处理
            com_frame.is_busy = true;
            com_frame.com_port = CAN_port;

            if (len == 4)
            {
                com_frame.cmd_id = CMD_REFVALUE_SET;
                com_frame.rxdatalen = len;
                com_frame.rxdata = buf;
                memset(&buf[4], 0, 4);
            }
            else if (len == 8)
            {
                com_frame.cmd_id = CMD_REFVALUE_SET;
                com_frame.rxdatalen = len;
                com_frame.rxdata = buf;
            }
            else if (len == 1)
            {
                com_frame.cmd_id = buf[0];
                com_frame.rxdatalen = 0;
            }
            else if (len == 2)
            {
                com_frame.cmd_id = buf[0];
                com_frame.rxdatalen = 1;
                com_frame.rxdata = &buf[1];
            }
        }
        else
        {
            // USB/UART 通用帧格式：HEAD + id + len + data + chk + tail
            // CAN以外的端口使用 packet 协议，data 已经去掉了头尾
            com_frame.is_busy = true;
            com_frame.com_port = g_port_rx[i].port_id;
            com_frame.cmd_id = id;
            com_frame.rxdatalen = len;
            com_frame.rxdata = buf;
        }

        _frame_data_deal();
        g_port_rx[i].pending = false;
    }
}

void comm_main_loop()
{
    _process_pending_rx();
    _stream_data_trans();
    can_queue_data_deal();
}