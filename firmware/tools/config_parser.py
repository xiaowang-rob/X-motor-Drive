"""解析 bsp/config.h 中的硬件配置及编译类型"""
import re
import sys


def _extract(text, key, fmt="str"):
    """从 config.h 文本中提取宏定义值"""
    if fmt == "hex":
        m = re.search(rf"#define\s+{key}\s+(0x[0-9a-fA-F]+)U?", text)
        return int(m.group(1), 16) if m else 0
    elif fmt == "num":
        m = re.search(rf"#define\s+{key}\s+(\d+)", text)
        return int(m.group(1)) if m else 0
    elif fmt == "float":
        m = re.search(rf"#define\s+{key}\s+([\d.]+)f?", text)
        return float(m.group(1)) if m else 0.0
    else:
        m = re.search(rf'#define\s+{key}\s+"?([\w.]+)"?', text)
        return m.group(1) if m else ""


def parse_bsp_config(config_h_path):
    """从 config.h 解析所有所需参数，返回字典"""
    if not config_h_path.exists():
        sys.exit(f"错误: config.h 不存在: {config_h_path}")

    text = config_h_path.read_text(encoding="utf-8")
    e = lambda key, fmt="str": _extract(text, key, fmt)

    return {
        "F_PWM":           e("F_PWM", "float"),
        "T_PWM":           e("T_PWM", "float"),
        "TIC_PWM":         e("TIC_PWM", "num"),
        "T_CON":           e("T_CON", "float"),
        "PROD_SERIES":     e("PROD_SERIES"),
        "FUN_V":           e("FUN_V"),
        "FIRM_V":          e("FIRM_V"),
        "MAX_CURRENT":     e("MAX_CURRENT", "num"),
        "MAX_VOLTAGE":     e("MAX_VOLTAGE", "num"),
        "MIN_VOLTAGE":     e("MIN_VOLTAGE", "num"),
        "MAX_TEMPERATURE": e("MAX_TEMPERATURE", "num"),
        "T_SAMPLE_us":     e("T_SAMPLE_us", "float"),
        "T_DEADTIME_us":   e("T_DEADTIME_us", "float"),
        "T_NOISE_us":      e("T_NOISE_us", "float"),
        "BL_START_ADDR":   e("BL_START_ADDR", "hex"),
        "BL_SIZE_KB":      e("BL_SIZE_KB", "num"),
        "APP_START_ADDR":  e("APP_START_ADDR", "hex"),
        "FLASH_END_ADDR":  e("FLASH_END_ADDR", "hex"),
        "FIRMWARE_TYPE":   e("FIRMWARE_TYPE").upper(),
    }
