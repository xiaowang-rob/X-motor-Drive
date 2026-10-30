
#include "bsp_base.h"
#include "bsp_flash.h"
#include "bsp_led.h"
#include "bsp_usb.h"
#include "usr_config.h"
#include "string.h"
#include "protocol.h"
typedef enum
{
    BL_LED_IDLE,    /* 交替慢闪：等待升级命令 */
    BL_LED_WRITING, /* 同步快闪：正在擦除/写入 Flash */
    BL_LED_SUCCESS, /* 同步慢闪：升级完成，准备跳转 */
    BL_LED_ERROR    /* 交替快闪：升级失败/校验错误 */
} BL_LED_State_t;

/* ========== 私有变量 ========== */
static u32 g_firmware_total_size = 0;
static u32 upgrade_addr;
static u8 g_fw_info_str[24] = {0};
static u16 fw_info_len;
static bool g_in_upgrade_mode = false;

/* 升级命令变量 */
static u8 g_upgrade_cmd;
static u16 g_upgrade_len;
static u8 g_upgrade_data[256];
static u8 g_cmd_received;

/* USB 发送缓冲区 */
static u8 firmware_info[36] = {USB_PACKET_HEAD, UC_CONNECT};
static u8 response[6] = {USB_PACKET_HEAD, 0x00, 0x01, 0x00, 0x00, USB_PACKET_TAIL};

/* LED 状态变量 */
static BL_LED_State_t g_bl_led_state = BL_LED_IDLE;
static u32 g_bl_led_timer = 0;
static bool g_bl_led_toggle = true;

/* ========== 内部函数声明 ========== */
static u8 Process_Upgrade_Cmd(u8 cmd, u8 *data, u16 len);

/* ========== BL 专用的 BSP 初始化函数 ========== */

/* ========== USB 接收处理 ========== */
// CRC8 校验（poly 0x07）
static inline u8 bl_crc8_update(u8 crc, u8 data)
{
    crc ^= data;
    for (int i = 0; i < 8; i++)
        crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
    return crc;
}
static u8 bl_crc8(const u8 *data, u8 len)
{
    u8 crc = 0;
    for (u8 i = 0; i < len; i++)
        crc = bl_crc8_update(crc, data[i]);
    return crc;
}

// USB 接收回调（强定义，覆盖 bsp_usb.c 中的弱定义）
// 由 CDC_Receive_FS → USB_RecvByte → bsp_usb_recv_byte 调用
bool bsp_usb_recv_byte(u8 *data, u8 *len)
{
    if (data == NULL || len == NULL || *len < 5)
        return false;

    // 验证帧头 0x55 和帧尾 0xAA
    if (data[0] != USB_PACKET_HEAD || data[*len - 1] != USB_PACKET_TAIL)
        return false;

    u8 cmd = data[1];                  // 命令 ID
    u8 dlen = data[2];                 // 数据长度
    u8 csum = data[*len - 2];          // CRC 字节
    u8 calc = bl_crc8(data + 3, dlen); // 计算 CRC

    if (calc != csum)
        return false;

    // 存入全局变量供主循环处理
    g_upgrade_cmd = cmd;
    g_upgrade_len = dlen;
    if (dlen > 0 && dlen <= 256)
        memcpy(g_upgrade_data, data + 3, dlen);
    g_cmd_received = 1;

    return true;
}

/* ========== 命令处理 ========== */
static u8 Process_Upgrade_Cmd(u8 cmd, u8 *data, u16 len)
{
    u8 result = FEEDBACK_EXECUTE;

    switch (cmd)
    {
    case UC_CONNECT:
        firmware_info[2] = (u8)strlen((char *)g_fw_info_str);
        memcpy(&firmware_info[3], g_fw_info_str, firmware_info[2]);
        {
            u16 checksum = 0;
            for (u8 i = 0; i < firmware_info[2]; i++)
                checksum += firmware_info[i + 3];
            firmware_info[3 + firmware_info[2]] = (u8)(checksum & 0xFF);
        }
        firmware_info[4 + firmware_info[2]] = USB_PACKET_TAIL;
        bsp_usb_cdc_transmit_fs(firmware_info, 5 + firmware_info[2]);
        return FEEDBACK_EXECUTE;

    case CMD_IAP_ENTER:
        if (len >= 4)
        {
            g_firmware_total_size = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
            result = FEEDBACK_EXECUTE;
        }
        else
        {
            result = FEEDBACK_FAILURE;
        }
        break;

    case CMD_IAP_ERASE_FLASH:
        result = bsp_flash_erase_app() ? FEEDBACK_EXECUTE : FEEDBACK_FAILURE;
        break;

    case CMD_IAP_WRITE_FLASH:
        if (len < 4)
        {
            result = FEEDBACK_FAILURE;
            break;
        }
        upgrade_addr = (u32)data[0] | ((u32)data[1] << 8) |
                       ((u32)data[2] << 16) | ((u32)data[3] << 24);
        if (upgrade_addr % 4 != 0)
        {
            result = FEEDBACK_FAILURE;
        }
        else
        {
            result = bsp_flash_write_app(upgrade_addr, &data[4], len - 4)
                         ? FEEDBACK_EXECUTE
                         : FEEDBACK_FAILURE;
        }
        break;

    case CMD_IAP_VERIFY_FLASH:
        if (len < 4)
        {
            result = FEEDBACK_FAILURE;
            break;
        }
        upgrade_addr = (u32)data[0] | ((u32)data[1] << 8) |
                       ((u32)data[2] << 16) | ((u32)data[3] << 24);
        result = bsp_flash_verify(upgrade_addr, &data[4], len - 4)
                     ? FEEDBACK_EXECUTE
                     : FEEDBACK_FAILURE;
        break;

    case CMD_IAP_EXIT:
        result = bsp_clear_upgrade_flag() ? FEEDBACK_EXECUTE : FEEDBACK_FAILURE;
        break;

    default:
        result = FEEDBACK_FAILURE;
        break;
    }

    response[1] = cmd;
    response[3] = result;
    response[4] = result;
    bsp_usb_cdc_transmit_fs(response, 6);

    return result;
}

/* ========== LED 接口 ========== */
void BL_LED_Init(void)
{
    bsp_led_can_set_pin(false);
    bsp_led_encoder_set_pin(false);
    g_bl_led_state = BL_LED_IDLE;
    g_bl_led_timer = bsp_get_tick();
    g_bl_led_toggle = true;
}

void BL_LED_SetState(BL_LED_State_t state)
{
    if (state == g_bl_led_state)
        return;
    g_bl_led_state = state;
    g_bl_led_timer = bsp_get_tick();
    g_bl_led_toggle = true;

    bsp_led_can_set_pin(false);
    bsp_led_encoder_set_pin(false);
}

void BL_LED_Process(void)
{
    u32 now = bsp_get_tick();
    u32 interval = 0;

    switch (g_bl_led_state)
    {
    case BL_LED_IDLE:
    case BL_LED_SUCCESS:
        interval = 500;
        break;
    case BL_LED_WRITING:
        interval = 100;
        break;
    case BL_LED_ERROR:
        interval = 100;
        break;
    default:
        return;
    }

    if (now - g_bl_led_timer >= interval)
    {
        g_bl_led_toggle = !g_bl_led_toggle;
        g_bl_led_timer = now;

        switch (g_bl_led_state)
        {
        case BL_LED_IDLE:
        case BL_LED_ERROR:
            bsp_led_can_set_pin(!g_bl_led_toggle);
            bsp_led_encoder_set_pin(g_bl_led_toggle);
            break;
        case BL_LED_SUCCESS:
        case BL_LED_WRITING:
            bsp_led_can_set_pin(g_bl_led_toggle);
            bsp_led_encoder_set_pin(g_bl_led_toggle);
            break;
        }
    }
}

void bsp_init_front(void)
{
}

void bsp_init_back(void)
{
    BL_LED_Init();
}

void bsp_main(void)
{

    BL_LED_SetState(BL_LED_IDLE);

    if (bsp_get_upgrade_flag(g_fw_info_str, &fw_info_len))
    {
        g_in_upgrade_mode = true;
    }

    if (g_in_upgrade_mode)
    {
        while (1)
        {
            BL_LED_Process();

            if (g_cmd_received)
            {
                g_cmd_received = 0;

                switch (g_upgrade_cmd)
                {
                case UC_CONNECT:
                case CMD_IAP_ENTER:
                    BL_LED_SetState(BL_LED_IDLE);
                    break;
                case CMD_IAP_ERASE_FLASH:
                case CMD_IAP_WRITE_FLASH:
                case CMD_IAP_VERIFY_FLASH:
                    BL_LED_SetState(BL_LED_WRITING);
                    break;
                case CMD_IAP_EXIT:
                    BL_LED_SetState(BL_LED_SUCCESS);
                    break;
                default:
                    BL_LED_SetState(BL_LED_ERROR);
                    break;
                }

                bsp_disable_irq();
                u8 cmd = g_upgrade_cmd;
                u16 len = g_upgrade_len;
                u8 data_buf[256];
                if (len > 0 && len <= 256)
                    memcpy(data_buf, g_upgrade_data, len);
                g_cmd_received = 0;
                bsp_enable_irq();

                u8 result = Process_Upgrade_Cmd(cmd, data_buf, len);

                if (cmd == CMD_IAP_EXIT)
                {
                    if (result == FEEDBACK_EXECUTE)
                    {
                        for (int i = 0; i < 200; i++)
                        {
                            bsp_delay(10);
                            BL_LED_Process();
                        }
                        break; // 跳转到 App
                    }
                    else
                    {
                        BL_LED_SetState(BL_LED_ERROR);
                    }
                }
                else if (result == FEEDBACK_FAILURE)
                {
                    BL_LED_SetState(BL_LED_ERROR);
                }
            }
            bsp_delay(1);
        }
    }

    if (!bsp_jump_to_app())
    {
        BL_LED_SetState(BL_LED_ERROR);
    }

    while (1)
    {
        BL_LED_Process();
        bsp_delay(1);
    }
}

void bsp_error_handler(void)
{
    BL_LED_SetState(BL_LED_ERROR);
    bsp_disable_irq();
    while (1)
    {
        BL_LED_Process();
        bsp_delay(10);
    }
}