import os
import subprocess
import sys
from pathlib import Path

# 本目录在 sys.path 上时（build.py 已插入）可直接导入同目录模块
sys.path.insert(0, str(Path(__file__).resolve().parent))

from utils import get_cmake_project_name

# firmware/tools/ -> firmware/ -> 仓库根
REPO_ROOT = Path(__file__).resolve().parent.parent.parent


def elf_name(project_root, fw_type):
    """根据固件类型获取 ELF 文件名（与 CMake 目标名一致）"""
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


def _get_nrst_args():
    """获取软件复位参数（如果配置了的话；openocd_reset.cfg 位于仓库根）"""
    cfg = REPO_ROOT / "openocd_reset.cfg"
    if cfg.exists():
        print("已启用软件复位（无硬件 NRST）")
        return ["-f", str(cfg)]
    return []


def cmd_flash(cfg, elf_path):
    """烧录固件（elf_path 为 build 目录下的 ELF）"""
    elf = Path(elf_path)
    if not elf.exists():
        sys.exit(f"错误: ELF 文件不存在: {elf}，请先编译")

    interface, target = _check_debugger(cfg)

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


def cmd_erase(cfg):
    """擦除芯片"""
    interface, target = _check_debugger(cfg)
    nrst_args = _get_nrst_args()
    print("擦除芯片...")
    subprocess.run([
        "openocd", "-f", interface, "-f", target, *nrst_args,
        "-c", "init; reset halt; flash erase_sector 0 0 last; exit",
    ])
