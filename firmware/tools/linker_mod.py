"""修改 CubeMX 生成的链接脚本，根据 bsp/config.h 调整 FLASH 起始地址和长度"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import config_parser


def modify_linker_script(config_h_path, ld_path):
    """读取 config.h，直接修改 ld_path 中的 FLASH 区域定义"""
    cfg = config_parser.parse_bsp_config(config_h_path)

    fw_type = cfg["FIRMWARE_TYPE"]
    bl_start = cfg["BL_START_ADDR"]
    bl_size_kb = cfg["BL_SIZE_KB"]
    app_start = cfg["APP_START_ADDR"]
    flash_end = cfg["FLASH_END_ADDR"]

    if fw_type == "BL":
        origin = bl_start
        length_kb = bl_size_kb
    else:
        app_size_kb = (flash_end - app_start + 1) // 1024 if flash_end >= app_start else 0
        origin = app_start
        length_kb = app_size_kb

    origin_hex = f"0x{origin:08X}"
    length_str = f"{length_kb}K"

    ld_content = ld_path.read_text(encoding="utf-8")

    # 定位 MEMORY 块并替换 FLASH 行
    memory_pattern = r"(MEMORY\s*\{)(.*?)(\})"
    match = re.search(memory_pattern, ld_content, re.DOTALL | re.IGNORECASE)
    if not match:
        sys.exit("错误: 未找到 MEMORY 块，ld 文件无法修改")

    memory_block = match.group(2)
    flash_pattern = r"(FLASH\s*\([^)]*\)\s*:\s*ORIGIN\s*=\s*)(0x[0-9a-fA-F]+)(\s*,\s*LENGTH\s*=\s*)([0-9]+[Kk]?)"
    new_flash_line, count = re.subn(
        flash_pattern,
        lambda m: f"{m.group(1)}{origin_hex}{m.group(3)}{length_str}",
        memory_block,
        flags=re.IGNORECASE,
    )

    if count == 0:
        sys.exit(f"错误: 在 MEMORY 块中未找到 FLASH 定义行")

    new_ld_content = ld_content[: match.start(2)] + new_flash_line + ld_content[match.end(2) :]

    # 备份原文件
    backup_path = ld_path.with_suffix(".ld.bak")
    backup_path.write_text(ld_content, encoding="utf-8")
    print(f"[备份] 原 ld 已备份至 {backup_path}")

    ld_path.write_text(new_ld_content, encoding="utf-8")
    print(f"[OK] 已修改 {ld_path.name}: FLASH ORIGIN={origin_hex}, LENGTH={length_str} ({fw_type}模式)")
