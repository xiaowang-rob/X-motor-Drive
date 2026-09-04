#ifndef __ABS_FLASH_H
#define __ABS_FLASH_H

#include <stdint.h>
#include <stdbool.h>

#include "usr/abs/device.h"

// ============================================================
// flash.h — 串行/并行 Flash 介质驱动契约（usr/abs）
//
// 本契约描述"一块可寻址、扇区擦除的 NOR Flash 介质"的最小能力，
// 供 abs 层做单元磨损 / IAP 编排时调用，也约束驱动实现（w25qxx 等）。
// 地址为芯片内绝对地址（从 0 起），几何参数由驱动提供。
//
// 上层保证：write/erase 操作区在已擦除的扇区上；erase 的 addr/len
// 为扇区大小对齐（驱动内部也会做扇区粒度扩展）。
// ============================================================

typedef void *FlashChipHandle;

typedef struct
{
    // 芯片初始化（含连接校验，如 JEDEC ID）
    bool (*init)(FlashChipHandle h);

    // 连续读取 len 字节
    bool (*read)(FlashChipHandle h, uint32_t addr, uint8_t *data, uint32_t len);

    // 连续写入（自动处理页边界），调用方保证目标扇区已擦除
    bool (*write)(FlashChipHandle h, uint32_t addr, const uint8_t *data, uint32_t len);

    // 擦除从 addr 起覆盖 len 的区域（按扇区粒度向上取整）
    bool (*erase)(FlashChipHandle h, uint32_t addr, uint32_t len);

    // ---- 几何 ----
    uint32_t (*get_capacity)(FlashChipHandle h); // 总字节
    uint32_t (*get_page_size)(FlashChipHandle h);  // 页(编程单元)字节
    uint32_t (*get_sector_size)(FlashChipHandle h); // 扇区(擦除单元)字节

    // 设备状态（eDeviceStatus 值）
    uint8_t (*get_state)(FlashChipHandle h);
} tFlashDriverOps;

// ==================== 业务对象：日志式存储单元 ====================

// 一个存储单元 = 介质上的一个擦除单元（扇区）区间。
// 采用"顺序追加记录"的日志式写法：写满才擦除，减少擦除次数（磨损友好）。
// 记录格式由上层定义；本层保证：写入字节后该区域不再为全 0xFF，
// 空闲边界由二分探测"首个未写位置"得到（重启后可恢复）。
typedef struct
{
    uint32_t base_addr; // 单元在介质中的基地址（须擦除单元对齐）
    uint32_t size;      // 单元大小（= 一个擦除单元）
    uint32_t free_addr; // 下一条记录的写入偏移（相对 base，0=空）
} tFlashUnit;

typedef struct
{
    const tFlashDriverOps *ops; // 绑定的介质驱动 ops
    FlashChipHandle handle;     // 介质句柄
    tFlashUnit unit;            // 当前管理单元
} tFlashStore;

// 绑定介质并初始化单元：单元 = 从 base_addr 起的第一个擦除单元；
// 自动探测空闲边界（重启恢复续写位置）
bool flash_unit_init(tFlashStore *s, const tFlashDriverOps *ops, FlashChipHandle h,
                     uint32_t base_addr);

// 追加写入一条记录（自动落在 free_addr）；空间不足返回 false 且不写入
bool flash_unit_append(tFlashStore *s, const uint8_t *data, uint32_t len);

// 读已写区数据（offset/len 必须落在 free 以内）
bool flash_unit_read(tFlashStore *s, uint32_t offset, uint8_t *data, uint32_t len);

// 擦除整个单元并复位写位置
bool flash_unit_erase(tFlashStore *s);

// 剩余可写字节
uint32_t flash_unit_free(tFlashStore *s);
bool flash_unit_is_full(tFlashStore *s);

// ==================== 业务对象：IAP 固件升级编排 ====================

// 分区表与固件行为回调由上层（组装层）注入，abs 不依赖任何板头。
typedef struct
{
    uint32_t bl_addr;  // Bootloader 区起始
    uint32_t bl_size;  // Bootloader 区大小（字节）
    uint32_t app_addr; // App 区起始
    uint32_t app_size; // App 区大小（字节）

    void (*jump)(uint32_t addr); // 跳转到指定固件地址（如 platform 包装）
    void (*reset)(void);         // 系统复位
} tFlashIAP;

// 擦除分区（边界按介质擦除单元向上取整，由介质驱动保证）
bool flash_iap_erase(const tFlashDriverOps *ops, FlashChipHandle h,
                     uint32_t addr, uint32_t size);

// 写分区数据（介质驱动处理页/字粒度）
bool flash_iap_write(const tFlashDriverOps *ops, FlashChipHandle h,
                     uint32_t addr, const uint8_t *data, uint32_t size);

// 校验分区数据与内存一致
bool flash_iap_verify(const tFlashDriverOps *ops, FlashChipHandle h,
                      uint32_t addr, const uint8_t *data, uint32_t size);

// 便捷：擦写/校验整个 App / BL 区
bool flash_iap_erase_app(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap);
bool flash_iap_write_app(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap,
                         uint32_t offset, const uint8_t *data, uint32_t size);
bool flash_iap_verify_app(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap,
                          uint32_t offset, const uint8_t *data, uint32_t size);
bool flash_iap_erase_bl(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap);
bool flash_iap_write_bl(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap,
                        uint32_t offset, const uint8_t *data, uint32_t size);
bool flash_iap_verify_bl(const tFlashDriverOps *ops, FlashChipHandle h, const tFlashIAP *iap,
                         uint32_t offset, const uint8_t *data, uint32_t size);

// 跳转 / 复位
void flash_iap_jump_app(const tFlashIAP *iap);
void flash_iap_jump_bl(const tFlashIAP *iap);
void flash_iap_reset(const tFlashIAP *iap);

#endif // __ABS_FLASH_H
