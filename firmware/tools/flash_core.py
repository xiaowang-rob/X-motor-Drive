import os
import subprocess
import sys
from pathlib import Path

from firmware.tools.utils import get_cmake_project_name


def _elf_name(project_root, fw_type):
    """根据固件类型获取 ELF 文件名"""
    name = get_cmake_project_name(project_root)
    if fw_type == "bl":
        return f"{name}-BL.elf"
    return f"{name}.elf"


def _check_debugger(cfg):
    """检查调试器连接并返回 (interface, target) 路径"""
    dbg_type = cfg.get("debugger_type", "stlink")
    debuggers = cfg.get("debugger", {})
    if dbg_type not in debuggers:
        sys.exit(f"错误: 未配置调试器 '{dbg_type}'，可用: {list(debuggers.keys())}")

    dbg = debuggers[dbg_type]
    interface, target = dbg["interface"], dbg["target"]

    print(f"检查 {dbg_type} 仿真器连接...")
    try:
        r = subprocess.run(
            ["openocd", "-f", interface, "-f", target, "-c", "init; exit"],
            capture_output=True, text=True, timeout=10,
        )
        if r.returncode != 0:
            sys.exit(f"错误: 无法连接到 {dbg_type}\n{r.stderr}")
    except FileNotFoundError:
        sys.exit("错误: 未找到 openocd")
    except subprocess.TimeoutExpired:
        sys.exit(f"错误: {dbg_type} openocd 连接超时")

    print(f"{dbg_type} 仿真器连接正常")
    return interface, target


def _get_nrst_args(project_root):
    """获取软件复位参数（如果配置了的话）"""
    cfg = project_root / "openocd_reset.cfg"
    if cfg.exists():
        print("已启用软件复位（无硬件 NRST）")
        return ["-f", str(cfg)]
    return []


def cmd_flash(cfg, project_root, fw_type="app"):
    """烧录固件"""
    interface, target = _check_debugger(cfg)
    elf = project_root / "build" / _elf_name(project_root, fw_type)
    if not elf.exists():
        sys.exit(f"错误: ELF 文件不存在: {elf}，请先编译")

    print("烧录中... (使用 ELF 内部地址)")
    # 使用 flash write_image erase 代替 program 命令，
    # program 依赖 reset halt 来验证烧录，但芯片上若有旧程序
    # 跑着会导致 halt 超时。flash write_image 直接擦除+写入，
    # 不受已有程序影响。
    r = subprocess.run([
        "openocd", "-f", interface, "-f", target,
        "-c", "init", "-c", "reset halt",
        "-c", f"flash write_image erase {elf}",
        "-c", f"verify_image {elf}",
        "-c", "resume", "-c", "exit",
    ])
    if r.returncode != 0:
        sys.exit("烧录失败!")
    print("[OK] 烧录完成")


def cmd_erase(cfg, project_root, fw_type="app"):
    """擦除芯片"""
    interface, target = _check_debugger(cfg)
    nrst_args = _get_nrst_args(project_root)
    print("擦除芯片...")
    subprocess.run([
        "openocd", "-f", interface, "-f", target, *nrst_args,
        "-c", "init; reset halt; flash erase_sector 0 0 last; exit",
    ])
