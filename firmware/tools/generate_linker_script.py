#!/usr/bin/env python3
"""
修改链接脚本的 FLASH 地址 / 大小
根据 bsp/config.h 中的 FIRMWARE_TYPE 修改 CubeMX 生成的 ld 文件
用法: python3 generate_linker_script.py <config.h> <ld_file>
"""

import re
import sys
from pathlib import Path

def parse_config(config_path):
    text = config_path.read_text(encoding="utf-8")

    def extract_hex(key):
        m = re.search(rf'#define\s+{key}\s+(0x[0-9a-fA-F]+)U?', text)
        return int(m.group(1), 16) if m else 0

    def extract_num(key):
        m = re.search(rf'#define\s+{key}\s+(\d+)', text)
        return int(m.group(1)) if m else 0

    def extract_str(key):
        m = re.search(rf'#define\s+{key}\s+(\w+)', text)
        return m.group(1) if m else "APP"

    config = {
        "FIRMWARE_TYPE": extract_str("FIRMWARE_TYPE").upper(),
        "BL_START_ADDR": extract_hex("BL_START_ADDR"),
        "BL_SIZE_KB": extract_num("BL_SIZE_KB"),
        "APP_START_ADDR": extract_hex("APP_START_ADDR"),
        "FLASH_END_ADDR": extract_hex("FLASH_END_ADDR"),
    }

    if config["FIRMWARE_TYPE"] not in ("APP", "BL"):
        print(f"警告: 未知 FIRMWARE_TYPE '{config['FIRMWARE_TYPE']}'，默认 APP")
        config["FIRMWARE_TYPE"] = "APP"

    # 计算 APP 可用 Flash 大小
    if config["FLASH_END_ADDR"] >= config["APP_START_ADDR"]:
        config["APP_FLASH_SIZE_KB"] = (config["FLASH_END_ADDR"] - config["APP_START_ADDR"] + 1) // 1024
    else:
        config["APP_FLASH_SIZE_KB"] = 0

    return config

def modify_ld(ld_path, config):
    content = ld_path.read_text(encoding="utf-8")

    if config["FIRMWARE_TYPE"] == "BL":
        origin = config["BL_START_ADDR"]
        length_kb = config["BL_SIZE_KB"]
    else:
        origin = config["APP_START_ADDR"]
        length_kb = config["APP_FLASH_SIZE_KB"]

    origin_hex = f"0x{origin:08X}"
    length_str = f"{length_kb}K"

    # 匹配 MEMORY 块中 FLASH 行，如： FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K
    pattern = r'(FLASH\s*\([^)]*\)\s*:\s*ORIGIN\s*=\s*)(0x[0-9a-fA-F]+)(\s*,\s*LENGTH\s*=\s*)([0-9]+K)'
    new_content = re.sub(pattern, rf'\1{origin_hex}\3{length_str}', content)

    if new_content == content:
        print("错误: 未找到 FLASH 定义行，脚本未修改任何内容")
        sys.exit(1)

    # 可选：备份原始文件
    backup = ld_path.with_suffix(".ld.bak")
    backup.write_text(content, encoding="utf-8")
    print(f"[备份] 原 ld 文件已备份至 {backup}")

    ld_path.write_text(new_content, encoding="utf-8")
    print(f"[OK] 已修改 {ld_path.name}: FLASH ORIGIN={origin_hex}, LENGTH={length_str}")
    print(f"      固件类型: {config['FIRMWARE_TYPE']}")

def main():
    if len(sys.argv) != 3:
        print("用法: python3 generate_linker_script.py <config.h> <ld_file>")
        sys.exit(1)

    config_path = Path(sys.argv[1])
    ld_path = Path(sys.argv[2])

    if not config_path.exists():
        print(f"错误: config.h 不存在: {config_path}")
        sys.exit(1)
    if not ld_path.exists():
        print(f"错误: ld 文件不存在: {ld_path}")
        sys.exit(1)

    config = parse_config(config_path)
    modify_ld(ld_path, config)

if __name__ == "__main__":
    main()