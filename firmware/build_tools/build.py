#!/usr/bin/env python3
"""XDr 新格式构建入口。

用法（在仓库根执行）：
  python3 firmware_new/build_tools/build.py build --board=xdr_s --type=app
  python3 firmware_new/build_tools/build.py flash --board=xdr_s --debugger=stlink
  python3 firmware_new/build_tools/build.py bf
  python3 firmware_new/build_tools/build.py clean

默认值读取 firmware/project.json。统一从 firmware/CMakeLists.txt 配置，
构建目录为 build/<board>-<type>/<build_type>，与 CMakePresets 共用；
板级 ELF 位于该目录的 board/ 下，bin/hex 输出到 releases/。
当前只有 xdr_s/app 板级工程就绪。编译不会生成或修改业务源码及链接脚本。
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

PROJECT_ROOT = TOOLS_DIR.parent        # firmware_new/
REPO_ROOT = PROJECT_ROOT.parent        # 仓库根
BUILD_ROOT = PROJECT_ROOT / "build"
FIRMWARE_OUT = PROJECT_ROOT / "releases"

# 板卡 -> 产品显示名（对应 firmware_new/board/<board>/）
BOARDS = {
    "xdr_p": "XDr-P",
    "xdr_s": "XDr-S",
}

# 固件类型
FW_TYPES = ("app", "bl")

# 板卡 -> 默认 openocd target（板级 project_config.json 缺失时的兜底）
DEFAULT_TARGET = {
    "xdr_p": "target/stm32f4x.cfg",
    "xdr_s": "target/stm32g4x.cfg",
}

# 板卡 -> 内部 Flash 几何（KB）：固件大小与分配展示、区域占用以此为准。
# 需与板级 fla_mcu.c 的分区表一致：
#   xdr_p：1MB，BL 32 / APP 608 / 用户区 384（LOG + PARAM + IAP 各 128）
#   xdr_s：128KB，BL 32 / APP 80 / USR 16
FLASH_GEOM = {
    "xdr_p": {
        "total_kb": 1024,
        "bl_kb": 32,
        "app_kb": 608,
        "regions": [
            ("BL", 32, 32),
            ("LOG", 128, 33),
            ("PARAM", 128, 35),
            ("IAP", 128, 36),
            ("APP", 608, 34),
        ],
    },
    "xdr_s": {
        "total_kb": 128,
        "bl_kb": 32,
        "app_kb": 80,
        "regions": [
            ("BL", 32, 32),
            ("USR", 16, 33),
            ("APP", 80, 34),
        ],
    },
}


def _load_project_config():
    """读取 firmware/project.json，返回配置字典"""
    cfg_path = PROJECT_ROOT / "project.json"
    if cfg_path.exists():
        return json.loads(cfg_path.read_text())
    return {}


_project_cfg = _load_project_config()  # 缓存


def _board_dir(board, fw_type):
    """板级工程目录：firmware_new/board/<board>/<type>/"""
    return PROJECT_ROOT / "board" / board / fw_type


def _load_board_config(board, fw_type, debugger=None):
    """加载板级配置（调试器信息等）。

    查找顺序：board/<board>/<type>/project_config.json
              → board/<board>/app/project_config.json
    调试器/接口属板级属性、与固件类型无关，故 bl 可复用 app 的配置；
    再找不到则用 DEFAULT_TARGET[board] 兜底。
    """
    for path in (_board_dir(board, fw_type) / "project_config.json",
                 _board_dir(board, "app") / "project_config.json"):
        if path.exists():
            cfg = json.loads(path.read_text())
            break
    else:
        target = DEFAULT_TARGET.get(board, "target/stm32f4x.cfg")
        cfg = {"debugger": {
            "stlink": {"interface": "interface/stlink.cfg", "target": target},
            "jlink": {"interface": "interface/jlink.cfg", "target": target},
            "daplink": {"interface": "interface/cmsis-dap.cfg", "target": target},
        }}

    if debugger:
        cfg["debugger_type"] = debugger
    return cfg


def _build_dir(board, fw_type):
    """构建目录与新 CMakePresets 的 Debug/Release 目录一致。"""
    build_type = _project_cfg.get("build_type", "Debug")
    if build_type not in ("Debug", "Release", "RelWithDebInfo", "MinSizeRel"):
        sys.exit(f"未知 build_type: {build_type}")
    return BUILD_ROOT / f"{board}-{fw_type}" / build_type


def _elf_path(board, fw_type):
    return _build_dir(board, fw_type) / "board" / _elf_name(board, fw_type)


# 板级 CubeMX 工程的 CMake target 名，决定 ELF 文件名
ELF_BASE = {
    "xdr_p": "XDr-P",
    "xdr_s": "XDr-S",
}


def _elf_name(board, fw_type):
    """板级可执行目标的 ELF 文件名（与板级 project()/目标名一致）"""
    base = ELF_BASE[board]
    return f"{base}-BL.elf" if fw_type == "bl" else f"{base}.elf"


def _stream(proc):
    """流式显示编译输出，返回 (errors, warnings, elapsed)"""
    error_re = re.compile(r"(error:|undefined reference|multiple definition)", re.IGNORECASE)
    warning_re = re.compile(r"warning:", re.IGNORECASE)

    errors = warnings = 0
    t0 = time.time()

    for raw in proc.stdout:
        # 原样输出，保留诊断上下文、源码行、插入符和链接器信息。
        print(raw, end="", flush=True)
        if error_re.search(raw):
            errors += 1
        if warning_re.search(raw):
            warnings += 1

    proc.wait()
    elapsed = time.time() - t0
    return errors, warnings, elapsed


def cmd_build(board, fw_type):
    """编译固件"""
    desc = BOARDS[board]
    board_dir = _board_dir(board, fw_type)

    # 编译类型：来自 project.json，默认 Debug
    build_type = _project_cfg.get('build_type', 'Debug')

    print(f"\n  \033[1m{desc} ({fw_type})\033[0m")
    print(f"  板级: board/{board}/{fw_type}")
    print(f"  固件: {'bootload/' if fw_type == 'bl' else 'app/ + services/ + xfoc/ + bsp'}")
    print(f"  编译类型: {build_type}")
    print()

    # 板级工程预检：目录里可能只有 CubeMX 的 .ioc（尚未生成工程）
    if not (board_dir / "CMakeLists.txt").exists():
        print(f"  \033[31m✗ 板级工程不完整: {board_dir}\033[0m")
        print(f"    该目录下没有 CMakeLists.txt（只有 .ioc 时属'未由 CubeMX 生成'状态）。")
        print(f"    请用 STM32CubeMX 打开 {board_dir}/*.ioc 生成板级工程，")
        print(f"    或改用已就绪的组合，例如 --board={board} --type=app。")
        sys.exit(1)

    toolchain = board_dir / "cmake" / "gcc-arm-none-eabi.cmake"
    if not toolchain.exists():
        print(f"  \033[31m✗ 找不到工具链文件: {toolchain}\033[0m")
        sys.exit(1)

    build_dir = _build_dir(board, fw_type)

    # CMake 配置（幂等：每次运行都执行，参数变化即时生效）
    print("  \033[36mCMake 配置...\033[0m")
    r = subprocess.run([
        "cmake", f"-B{build_dir}", f"-S{PROJECT_ROOT}",
        f"-DBOARD={board}", f"-DFW_TYPE={fw_type}",
        "-G", "Unix Makefiles",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(r.stdout, end="", flush=True)
    if r.returncode != 0:
        print(f"  \033[31m✗ CMake 配置失败:\033[0m")
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
    elf = _elf_path(board, fw_type)
    if not elf.exists():
        sys.exit(f"错误: 编译结束但未找到 ELF: {elf}")
    if elf.exists():
        flash_bytes = 0
        dec_total = 0
        r = subprocess.run(["arm-none-eabi-size", str(elf)], capture_output=True, text=True)
        if r.returncode == 0:
            lines = r.stdout.strip().splitlines()
            if len(lines) >= 2:
                vals = lines[1].split()[:4]  # text, data, bss, dec
                dec_total = int(vals[3])
                # data 的初始值存放在 Flash；bss 仅占 RAM，不计入 Flash。
                flash_bytes = int(vals[0]) + int(vals[1])

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

        # Flash 分配进度条（按板几何：xdr_p 1MB / xdr_s 128KB）
        geom = FLASH_GEOM.get(board)
        if geom:
            total_kb = geom["total_kb"]
            regions = geom["regions"]
            print(f"\n  \033[1;36m╭─ Flash 分配 ─────────────────────────────────────╮\033[0m")
            print(f"  \033[1;36m│\033[0m {desc:<8} 总容量: \033[1m{total_kb:>4} KB\033[0m"
                  f"  ·  顺序: BL → USR → APP")
            print(f"  \033[1;36m├───────────────────────────────────────────────────┤\033[0m")
            max_bar = 42
            bar_chars = []
            legend = []
            for name, size_kb, color in regions:
                n = max(1, size_kb * max_bar // total_kb)
                bar_chars.append(f"\033[{color}m{'█' * n}\033[0m")
                legend.append(f"\033[{color}m{name}\033[0m {size_kb:>4}K")
            print(f"  \033[1;36m│\033[0m {''.join(bar_chars)}")
            print(f"  \033[1;36m│\033[0m {'   '.join(legend)}")
            print(f"  \033[1;36m╰───────────────────────────────────────────────────╯\033[0m")

            # 当前固件区域使用进度条（与全片分配分开显示）
            region_name = "BL" if fw_type == "bl" else "APP"
            region_total = geom["bl_kb"] if fw_type == "bl" else geom["app_kb"]
            if flash_bytes > 0 and region_total > 0:
                used = flash_bytes / 1024
                pct = int(used / region_total * 100)
                bar_len = 30
                filled = max(1, int(used * bar_len / region_total))
                empty = bar_len - filled
                # 根据占用百分比变色：<70% 绿  70-90% 黄  >90% 红
                used_color = 32 if pct < 70 else (33 if pct < 90 else 31)
                bar = f"\033[{used_color}m{'█' * filled}\033[0m\033[90m{'░' * empty}\033[0m"
                print(f"\n  \033[1m{region_name} 区域占用:\033[0m")
                print(f"    \033[1m{region_name}\033[0m {bar}  {used:.1f}KB / {region_total}KB (\033[{used_color}m{pct}%\033[0m)")

        # 输出到 releases/
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
            if r.returncode != 0:
                sys.exit(f"错误: 导出 {dst.name} 失败: {r.stderr.decode(errors='replace')}")
            if r.returncode == 0:
                sz = dst.stat().st_size / 1024
                print(f"    \033[32m✓\033[0m {dst.name}  ({sz:.1f} KB)")

        _update_debug_files(board, fw_type, elf, _project_cfg.get("debugger"))

        # 创建统一调试入口符号链接
        current = build_dir / "current.elf"
        try:
            if current.exists() or current.is_symlink():
                current.unlink()
            current.symlink_to(elf.relative_to(build_dir))
        except OSError:
            pass  # Windows 可能不支持符号链接，忽略


def _update_debug_files(board, fw_type, elf, debugger=None):
    """固定调试入口；preLaunchTask 编译后无需重新解析 launch.json。"""
    cfg = _load_board_config(board, fw_type, debugger)
    dbg_type = cfg.get("debugger_type", "stlink")
    dbg = cfg["debugger"][dbg_type]
    lines = [f'source [find {{{dbg["interface"]}}}]',
             f'source [find {{{dbg["target"]}}}]']
    reset = PROJECT_ROOT / "openocd_reset.cfg"
    if reset.exists():
        lines.append(f'source {{{reset.as_posix()}}}')
    BUILD_ROOT.mkdir(parents=True, exist_ok=True)
    current = BUILD_ROOT / "current.elf"
    # 复制代替符号链接，兼容不支持 symlink 的环境。
    shutil.copy2(elf, current)
    (BUILD_ROOT / "debug_openocd.cfg").write_text("\n".join(lines) + "\n", encoding="utf-8")


def _resolve_cli():
    """解析命令行参数，返回 (cmd, board, fw_type, debugger)"""
    board = _project_cfg.get("board", "xdr_s")
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
        print("用法: python3 firmware_new/build_tools/build.py build --board=xdr_s --type=app")
        print("       python3 firmware_new/build_tools/build.py flash --board=xdr_p --debugger=jlink")
        print("命令: build | flash | bf (编译+烧录) | erase | clean")
        print(f"板卡: {', '.join(BOARDS)}")
        print(f"固件类型: {', '.join(FW_TYPES)}")
        print("选项:")
        print("  --board=<board>      板卡（board/<board>/<type>/）")
        print("  --type=<app|bl>      固件类型")
        print("  --debugger=<type>    调试器类型: stlink/jlink/daplink")
        print("默认值取自 project.json，未配置时: board=xdr_s, type=app, debugger=stlink")
        sys.exit(1)

    cmd = sys.argv[1]
    board, fw_type, debugger = _resolve_cli()

    if board not in BOARDS:
        print(f"未知板卡: {board}（可用: {', '.join(BOARDS)}）")
        sys.exit(1)

    if fw_type not in FW_TYPES:
        print(f"未知固件类型: {fw_type}（可用: {', '.join(FW_TYPES)}）")
        sys.exit(1)

    if cmd == "build":
        cmd_build(board, fw_type)
    elif cmd == "flash":
        cfg = _load_board_config(board, fw_type, debugger)
        elf = _elf_path(board, fw_type)
        flash_core.cmd_flash(cfg, elf)
    elif cmd == "bf":
        cmd_build(board, fw_type)
        cfg = _load_board_config(board, fw_type, debugger)
        elf = _elf_path(board, fw_type)
        flash_core.cmd_flash(cfg, elf)
    elif cmd == "erase":
        cfg = _load_board_config(board, fw_type, debugger)
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
