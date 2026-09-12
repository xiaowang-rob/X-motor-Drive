#!/usr/bin/env python3
"""
XDr 统一构建脚本

用法:
  python3 firmware/tools/build.py build --board=xdr_p_app --type=app
  python3 firmware/tools/build.py flash --board=xdr_p_app
  python3 firmware/tools/build.py bf          # 编译 + 烧录
  python3 firmware/tools/build.py clean

项目结构（构建单元在 firmware/ 下）：
  firmware/board/<board>/   板级：HAL + CubeMX Core + USB + drv + startup
  firmware/app/             应用侧：abs / app / ctl / srv / utils
  firmware/bootload/        引导固件（--type=bl）
  firmware/build/<board>-<type>/  CMake 构建目录（与 CMakePresets 的目录一致）
  firmware/firmware_out/    输出 bin/hex
"""
import json
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS_DIR))

import flash_core

PROJECT_ROOT = TOOLS_DIR.parent        # firmware/
REPO_ROOT = PROJECT_ROOT.parent        # 仓库根
BUILD_ROOT = PROJECT_ROOT / "build"
FIRMWARE_OUT = PROJECT_ROOT / "firmware_out"

# 板级目录名 -> 产品显示名（对应 firmware/board/<board>/）
BOARDS = {
    "xdr_p_app": "XDr-P",
}


def _load_project_config():
    """读取仓库根 project.json，返回配置字典"""
    cfg_path = REPO_ROOT / "project.json"
    if cfg_path.exists():
        return json.loads(cfg_path.read_text())
    return {}


_project_cfg = _load_project_config()  # 缓存


def _load_board_config(board, debugger=None):
    """加载板级配置（调试器信息等），debugger 可覆盖配置文件中的 debugger_type"""
    cfg_path = PROJECT_ROOT / "board" / board / "project_config.json"
    if cfg_path.exists():
        cfg = json.loads(cfg_path.read_text())
    else:
        cfg = {"debugger": {"stlink": {"interface": "interface/stlink.cfg", "target": "target/stm32f4x.cfg"}}}
    if debugger:
        cfg["debugger_type"] = debugger
    return cfg


def _build_dir(board, fw_type):
    """构建目录：firmware/build/<board>-<type>（与 CMakePresets 中的同名 preset 共用）"""
    return BUILD_ROOT / f"{board}-{fw_type}"


def _stream(proc):
    """流式显示编译输出，返回 (errors, warnings, elapsed)"""
    progress_re = re.compile(r"^\s*\[\s*(\d+)[%/]\d*\]\s+(Building|Linking)")
    error_re = re.compile(r"(error:|undefined reference)", re.IGNORECASE)
    warning_re = re.compile(r"warning:", re.IGNORECASE)

    errors = warnings = 0
    last_progress = False
    t0 = time.time()

    for raw in proc.stdout:
        line = raw.rstrip("\n\r")
        if not line:
            continue

        is_progress = bool(progress_re.match(line))
        has_err = bool(error_re.search(line))
        has_warn = bool(warning_re.search(line))

        if has_err:
            errors += 1
        if has_warn:
            warnings += 1

        if is_progress and not has_err and not has_warn:
            m = progress_re.match(line)
            pct = m.group(1) if m else "??"
            print(f"\r  \033[36m编译中...\033[0m {pct}%  \033[90m({int(time.time()-t0)}s)\033[0m", end="", flush=True)
            last_progress = True
        else:
            if last_progress:
                print()
                last_progress = False
            if has_err:
                print(f"  \033[31m✗ {line.strip()}\033[0m")
            elif has_warn:
                short = line.strip()
                m = re.search(r"((?:app|board|platform|bl)/\S+:\d+)", short)
                if m:
                    short = f"{m.group(1)}{short[m.end():]}"
                print(f"  \033[33m⚠ {short}\033[0m")
            elif "Linking" in line:
                print(f"  \033[32m⟶ Linking...\033[0m")

    proc.wait()
    elapsed = time.time() - t0
    if last_progress:
        print()
    return errors, warnings, elapsed


def cmd_build(board, fw_type):
    """编译固件"""
    desc = BOARDS[board]
    elf_name = "XDr-BL.elf" if fw_type == "bl" else "XDr.elf"

    # 编译类型：来自 project.json，默认 Debug
    build_type = _project_cfg.get('build_type', 'Debug')

    print(f"\n  \033[1m{desc} ({fw_type})\033[0m")
    print(f"  板级: board/{board}")
    print(f"  固件: {'bootload/' if fw_type == 'bl' else 'app/'}")
    print(f"  编译类型: {build_type}")
    print()

    toolchain = PROJECT_ROOT / "board" / board / "cmake" / "gcc-arm-none-eabi.cmake"
    if not toolchain.exists():
        print(f"  \033[31m✗ 找不到工具链文件: {toolchain}\033[0m")
        sys.exit(1)

    build_dir = _build_dir(board, fw_type)

    # CMake 配置（幂等：每次运行都执行，参数变化即时生效）
    print("  \033[36mCMake 配置...\033[0m")
    r = subprocess.run([
        "cmake", f"-B{build_dir}", f"-S{PROJECT_ROOT}",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        f"-DBOARD={board}", f"-DFW_TYPE={fw_type}",
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"  \033[31m✗ CMake 配置失败:\033[0m")
        for line in (r.stdout + r.stderr).splitlines():
            print(f"    {line}")
        sys.exit(r.returncode)

    # 编译
    proc = subprocess.Popen(
        ["cmake", "--build", str(build_dir), "--", "-k"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1,
    )
    errors, warnings, elapsed = _stream(proc)

    if proc.returncode != 0:
        print(f"\n  \033[31m✗ 编译失败  {errors} error\033[0m")
        sys.exit(proc.returncode)

    status = []
    if errors:
        status.append(f"\033[31m{errors} error\033[0m")
    if warnings:
        status.append(f"\033[33m{warnings} warning\033[0m")
    status.append(f"{elapsed:.1f}s")
    print(f"\n  \033[32m✓ 编译成功\033[0m  {'  '.join(status)}\n")

    # 固件大小 + 输出
    elf = build_dir / elf_name
    if elf.exists():
        r = subprocess.run(["arm-none-eabi-size", str(elf)], capture_output=True, text=True)
        if r.returncode == 0:
            lines = r.stdout.strip().splitlines()
            if len(lines) >= 2:
                vals = lines[1].split()[:4]  # text, data, bss, dec
                dec_total = int(vals[3])

                # 固件大小进度条（每项一行：标签 ████ 大小）
                headers = [("text", 34), ("data", 33), ("bss", 35)]  # name, color
                max_val = max(int(vals[i]) for i in range(3))
                bar_width = 35
                print(f"  \033[1m固件大小:\033[0m")
                for name, color in headers:
                    v = int(vals[0 if name == "text" else (1 if name == "data" else 2)])
                    kb = v / 1024
                    n = max(1, int(v * bar_width / max_val))
                    bar = f"\033[{color}m{'█' * n}\033[0m{' ' * (bar_width - n)}"
                    print(f"    \033[{color}m{name:<5s}\033[0m {bar}  {kb:.1f} KB")
                print(f"    \033[90m{'─' * 48}\033[0m")
                print(f"    \033[1m总计\033[0m  {dec_total/1024:.1f} KB")
            else:
                dec_total = 0

        # Flash 分配进度条（全片）
        print(f"\n  \033[1mFlash 分配:\033[0m")
        regions = [
            ("BL", 32, 32),           # 32: green
            ("APP", 608, 34),         # 34: blue
            ("LOG", 128, 33),         # 33: yellow
            ("PARAM", 128, 35),       # 35: magenta
            ("IAP", 128, 36),         # 36: cyan
        ]
        max_bar = 40
        bar_chars = []
        legend = []
        for name, size_kb, color in regions:
            n = max(1, size_kb * max_bar // 1024)
            bar_chars.append(f"\033[{color}m{'█' * n}\033[0m")
            legend.append(f"\033[{color}m{name}\033[0m")
        print(f"    [{' '.join(legend)}]")
        print(f"    {' ' * 4}{''.join(bar_chars)}  1MB (1024 KB)")

        # 当前固件区域使用进度条（与全片分配分开显示）
        app_size_kb = 1024 - 128 - 128 - 128 - 32  # total - flag - param - log - bl
        region_name = "BL" if fw_type == "bl" else "APP"
        region_total = 32 if fw_type == "bl" else app_size_kb
        if dec_total > 0:
            used = dec_total / 1024
            pct = int(used / region_total * 100) if region_total > 0 else 0
            bar_len = 30
            filled = max(1, int(used * bar_len / region_total)) if region_total > 0 else 1
            empty = bar_len - filled
            # 根据占用百分比变色：<70% 绿  70-90% 黄  >90% 红
            used_color = 32 if pct < 70 else (33 if pct < 90 else 31)
            bar = f"\033[{used_color}m{'█' * filled}\033[0m\033[90m{'░' * empty}\033[0m"
            print(f"\n  \033[1m{region_name} 区域占用:\033[0m")
            print(f"    \033[1m{region_name}\033[0m {bar}  {used:.1f}KB / {region_total}KB (\033[{used_color}m{pct}%\033[0m)")

        # 输出到 firmware_out/
        date_str = datetime.now().strftime("%y%m%d")
        # 固件输出命名：XDr-P_BL260627 或 XDr-P_260627
        fw_tag = f"{fw_type.upper()}" if fw_type == "bl" else ""
        dist = f"{desc.replace(' ', '_')}_{fw_tag}{date_str}"
        FIRMWARE_OUT.mkdir(parents=True, exist_ok=True)
        print(f"\n  \033[1m固件输出:\033[0m")
        for ext, fmt in [("bin", "binary"), ("hex", "ihex")]:
            dst = FIRMWARE_OUT / f"{dist}.{ext}"
            r = subprocess.run(
                ["arm-none-eabi-objcopy", "-O", fmt, str(elf), str(dst)],
                capture_output=True,
            )
            if r.returncode == 0:
                sz = dst.stat().st_size / 1024
                print(f"    \033[32m✓\033[0m {dst.name}  ({sz:.1f} KB)")

        # 创建统一调试入口符号链接
        current = build_dir / "current.elf"
        try:
            if current.exists() or current.is_symlink():
                current.unlink()
            current.symlink_to(elf.name)
        except OSError:
            pass  # Windows 可能不支持符号链接，忽略


def _resolve_cli():
    """解析命令行参数，返回 (cmd, board, fw_type, debugger)"""
    board = _project_cfg.get("board", "xdr_p_app")
    fw_type = _project_cfg.get("fw_type", "app")
    debugger = _project_cfg.get("debugger")

    for a in sys.argv[2:]:
        if a.startswith("--board="):
            board = a.split("=", 1)[1]
        elif a.startswith("--type="):
            fw_type = a.split("=", 1)[1]
        elif a.startswith("--debugger="):
            debugger = a.split("=", 1)[1]

    return board, fw_type, debugger


def main():
    if len(sys.argv) < 2:
        print("XDr 构建脚本")
        print("用法: python3 firmware/tools/build.py build --board=xdr_p_app --type=app")
        print("       python3 firmware/tools/build.py flash --board=xdr_p_app --debugger=jlink")
        print("命令: build | flash | bf (编译+烧录) | erase | clean")
        print(f"板卡: {', '.join(BOARDS.keys())}")
        print("选项:")
        print("  --board=<board>      板卡名称（board/<board>/）")
        print("  --type=<app|bl>      固件类型")
        print("  --debugger=<type>    调试器类型: stlink/jlink/daplink")
        print("默认值取自 project.json，未配置时: board=xdr_p_app, type=app, debugger=stlink")
        sys.exit(1)

    cmd = sys.argv[1]
    board, fw_type, debugger = _resolve_cli()

    if board not in BOARDS:
        print(f"未知板卡: {board}（可用: {', '.join(BOARDS.keys())}）")
        sys.exit(1)

    if cmd == "build":
        cmd_build(board, fw_type)
    elif cmd == "flash":
        cfg = _load_board_config(board, debugger)
        elf = _build_dir(board, fw_type) / flash_core.elf_name(PROJECT_ROOT, fw_type)
        flash_core.cmd_flash(cfg, elf)
    elif cmd == "bf":
        cmd_build(board, fw_type)
        cfg = _load_board_config(board, debugger)
        elf = _build_dir(board, fw_type) / flash_core.elf_name(PROJECT_ROOT, fw_type)
        flash_core.cmd_flash(cfg, elf)
    elif cmd == "erase":
        cfg = _load_board_config(board, debugger)
        flash_core.cmd_erase(cfg)
    elif cmd == "clean":
        if BUILD_ROOT.exists():
            shutil.rmtree(BUILD_ROOT)
            print(f"[OK] 已清除 {BUILD_ROOT}")
        else:
            print("build/ 不存在")
    else:
        print(f"未知命令: {cmd}")
        sys.exit(1)


if __name__ == "__main__":
    main()
