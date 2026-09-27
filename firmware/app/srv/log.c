#include "log.h"
#include "flash.h"
#include "bsp_cfg.h"
#include "bsp_time.h"

tLog g_log;
tFlashUnit log_unit; // 日志存储单元

// ============================================================
// ★ 唯一字段定义处
//   X(枚举 id, 结构体字段, 类型, 默认值)
//   - id 直接用 protocol.h 的 eLogList 名 → 与协议强绑定
// ============================================================
#define LOG_FIELDS(X)                  \
    X(LOG_HOURS, hours, U8, 0u)        \
    X(LOG_MINUTES, minutes, U8, 0u)    \
    X(LOG_FAULT, fault, U8, 0u)        \
    X(LOG_WARNING, warning, U8, 0u)    \
    X(LOG_VBUS, vbus, F32, 0.0f)       \
    X(LOG_TEMP, temp, F32, 0.0f)       \
    X(LOG_IU, iu, F32, 0.0f)           \
    X(LOG_IV, iv, F32, 0.0f)           \
    X(LOG_IW, iw, F32, 0.0f)           \
    X(LOG_ID, id, F32, 0.0f)           \
    X(LOG_ID_TAG, id_tag, F32, 0.0f)   \
    X(LOG_IQ, iq, F32, 0.0f)           \
    X(LOG_IQ_TAG, iq_tag, F32, 0.0f)   \
    X(LOG_VEL, vel, F32, 0.0f)         \
    X(LOG_VEL_TAG, vel_tag, F32, 0.0f) \
    X(LOG_POS, position, F32, 0.0f)    \
    X(LOG_POS_TAG, position_tag, F32, 0.0f)

// ---- 描述符表（协议读写用；字段名错 → 编译期报错）----
const tField g_log_fields[LOG_NUM] = {
#define L_DESC(id, field, type, def) \
    [id] = {&g_log.field, FLD_##type, (def)},
    LOG_FIELDS(L_DESC)
#undef L_DESC
};

// 日志服务初始化：注册存储单元，回读最后一条有效日志
bool log_init(void)
{
    if (!flash_unit_register(&g_flash, &log_unit))
        return false;

    // 无文件头：每条就是 sizeof(tLog) 字节，单元为空时读失败即保持默认值
    flash_unit_read(&g_flash, &log_unit, (uint8_t *)&g_log, (uint32_t)sizeof(g_log));
    return true;
}

// 采集当前运行量填入 g_log
void log_data_save(tProtectionManager *pro_manager)
{
    if (pro_manager)
    {
        g_log.fault = (uint8_t)pro_manager->fault;
        g_log.warning = (uint8_t)pro_manager->warning;
    }

    // 时间戳：由系统运行时间（ms）换算成 hh:mm
    uint32_t sec = bsp_time_ms() / 1000U;
    g_log.hours = (uint8_t)((sec / 3600U) % 100U);
    g_log.minutes = (uint8_t)((sec / 60U) % 60U);

    // TODO: 电气量（vbus / temp / iu..iw / id / iq / vel / position 及其目标值）
    //       由调用方或 foc/core 侧数据源采集后填入 g_log 对应字段
}

// 把当前 g_log 作为一条记录追加写入存储单元
void log_data_write(void)
{
    flash_unit_append(&g_flash, &log_unit, (const uint8_t *)&g_log, (uint32_t)sizeof(g_log)));
}

// 读指定log
void log_get(eLogList id, uint8_t *value, uint32_t *len)
{
    if (id >= LOG_NUM)
    {
        *len = 0;
        return;
    }
    uint8_t n = field_read_raw(&g_log_fields[id], value);
    if (len)
        *len = n;
}