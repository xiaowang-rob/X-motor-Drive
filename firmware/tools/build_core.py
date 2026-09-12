"""构建核心逻辑：生成 usr_config.h、CMake 编译"""
import re
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import config_parser
import filter_coeffs
import linker_mod
from utils import get_cmake_project_name, get_firmware_out_dir, clean_ansi


def _fmt_f(val):
    """格式化浮点数：去除尾部零，保留至少一位小数"""
    s = f"{val:.10f}".rstrip("0").rstrip(".")
    return s if "." in s else s + ".0"


def gen_usr_config(cfg, bsp_info, coeffs, output_path):
    """生成 usr_config.h 文件"""
    date_str = datetime.now().strftime("%y%m%d")
    firm_name = f"{cfg['prod_name']}-{bsp_info['PROD_SERIES']}"
    fun_v, firm_v, author = bsp_info["FUN_V"], bsp_info["FIRM_V"], cfg["author"]
    ctrl, hw = cfg["control"], bsp_info
    lpf_w, fc_speed, FS = coeffs["lpf_w"], coeffs["fc_speed"], coeffs["FS"]

    vect_offset = (
        0x0 if bsp_info["FIRMWARE_TYPE"] == "BL"
        else bsp_info["APP_START_ADDR"] - bsp_info["BL_START_ADDR"]
    )

    f = _fmt_f
    f_pwm = hw["F_PWM"]
    f_cur_div = ctrl["f_freq_current"]
    f_spd_div = ctrl["f_freq_speed"]
    f_pos_div = ctrl["f_freq_position"]

    content = f"""\
// 此文件由 build.py 自动生成，请勿手动修改，相关配置在 usr_config.json 中
// 生成时间: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
#ifndef __USR_CONFIG_H
#define __USR_CONFIG_H

// 版本信息
#define FIRM_NAME      "{firm_name}"
#define FIRM_AUTHOR    "{author}"
#define FIRM_V_DATE    "{fun_v}_{firm_v}_{date_str}"
#define FIRM_VERSION   FIRM_NAME " " FIRM_V_DATE

// 启动配置
#define VECT_TABLE_OFFSET  0x{vect_offset:X}U

// 控制参数
#define F_PWM               {f(f_pwm)}
#define T_PWM               {f(hw["T_PWM"])}
#define TIC_PWM             {hw["TIC_PWM"]}
#define T_CON               {f(hw["T_CON"])}

#define FREQ_HIGH_LOOP      {f_cur_div}
#define FREQ_MEDIUM_LOOP    {f_spd_div}
#define FREQ_LOW_LOOP       {f_pos_div}

#define F_CURRENT           {f(f_pwm / f_cur_div)}
#define F_SPEED             {f(f_pwm / f_cur_div / f_spd_div)}
#define F_POSITION          {f(f_pwm / f_cur_div / f_spd_div / f_pos_div)}

#define T_DATA_STREAM       {ctrl["t_data_stream_ms"]}
#define T_STATE_STREAM      {ctrl["t_state_stream_ms"]}
#define TEMP_VBUS_TS_MS     {ctrl["temp_vbus_ts_ms"]}

// 硬件限制参数 (来自 BSP)
#define MAX_CURRENT         {hw["MAX_CURRENT"]}
#define MAX_VOLTAGE         {hw["MAX_VOLTAGE"]}
#define MIN_VOLTAGE         {hw["MIN_VOLTAGE"]}
#define MAX_TEMPERATURE     {hw["MAX_TEMPERATURE"]}
#define T_SAMPLE_us         {f(hw["T_SAMPLE_us"])}
#define T_DEADTIME_us       {f(hw["T_DEADTIME_us"])}
#define T_NOISE_us          {f(hw["T_NOISE_us"])}

// 速度 LPF 滤波器系数 (Butterworth, fc={fc_speed:.1f}Hz, fs={FS}Hz)
#define LPF_W_B0    {f(lpf_w[0])}
#define LPF_W_B1    {f(lpf_w[1])}
#define LPF_W_B2    {f(lpf_w[2])}
#define LPF_W_A1    {f(lpf_w[3])}
#define LPF_W_A2    {f(lpf_w[4])}

// 滤波器截止频率 (Hz)
#define CUR_LPF_HZ    800.0f
#define SPEED_LPF_HZ  50.0f

#endif // __USR_CONFIG_H
"""
    output_path.write_text(content, encoding="utf-8")
    print("[OK] 生成 usr_config.h")


def cmd_gen(cfg, project_root):
    """生成 usr_config.h 和修改链接脚本"""
    config_h = project_root / "bsp" / "config.h"
    bsp_info = config_parser.parse_bsp_config(config_h)
    coeffs = filter_coeffs.compute_filter_coeffs(cfg)

    gen_usr_config(cfg, bsp_info, coeffs, project_root / "usr" / "usr_config.h")

    ld_file = project_root / "STM32F405XX_FLASH.ld"
    if ld_file.exists():
        linker_mod.modify_linker_script(config_h, ld_file)


def _check_and_clean_if_type_changed(project_root, firmware_type):
    """如果编译类型变化，清除 build 目录"""
    build_dir = project_root / "build"
    marker = build_dir / ".last_firmware_type"

    if build_dir.exists():
        if marker.exists():
            last_type = marker.read_text(encoding="utf-8").strip()
            if last_type != firmware_type:
                print(f"检测到编译类型变化 ({last_type} -> {firmware_type})，清理 build 目录...")
                shutil.rmtree(build_dir)
                build_dir.mkdir(parents=True)
        else:
            print("未找到编译类型记录，清理 build 目录以确保一致性...")
            shutil.rmtree(build_dir)
            build_dir.mkdir(parents=True)
    else:
        build_dir.mkdir(parents=True)

    marker.write_text(firmware_type, encoding="utf-8")


def _stream_build_output(proc):
    """流式处理 cmake 编译输出，返回 (error_count, warning_count, elapsed_sec)"""
    import time

    progress_re = re.compile(r"^\s*\[\s*(\d+)([%/])(\d*)\]\s+(Building|Linking)")
    error_re = re.compile(r"(error:|undefined reference|multiple definition|ld returned \d+ exit status)", re.IGNORECASE)
    warning_re = re.compile(r"warning:", re.IGNORECASE)
    file_re = re.compile(r"^(\S+\.(c|cpp|s|ld))\b")

    errors = warnings = 0
    compiled_files = []
    warn_details = []  # (file, msg)
    err_details = []   # (file, msg)
    current_file = ""
    last_progress = False
    start_time = time.time()

    for raw_line in proc.stdout:
        line = raw_line.rstrip("\n\r")
        if not line:
            continue

        plain = clean_ansi(line)
        is_progress = bool(progress_re.match(plain))
        has_error = bool(error_re.search(plain))
        has_warning = bool(warning_re.search(plain))

        # 提取当前编译的文件名
        m = progress_re.match(plain)
        if m:
            # 从 Building C object ... 提取文件名
            parts = plain.split()
            for p in parts:
                if p.endswith(".c") or p.endswith(".s"):
                    fname = Path(p).name
                    if fname not in compiled_files:
                        compiled_files.append(fname)
                    current_file = fname

        if has_error:
            errors += 1
            err_details.append((current_file, plain.strip()))
        if has_warning:
            warnings += 1
            warn_details.append((current_file, plain.strip()))

        # 显示策略：只显示进度、错误、警告、链接
        if is_progress and not has_error and not has_warning:
            # 进度条：覆盖式显示
            elapsed = time.time() - start_time
            pct = m.group(1) if m else "??"
            total = m.group(3) if m and m.group(3) else ""
            suffix = f" [{total}]" if total else ""
            indicator = f"\r  \033[36m编译中...\033[0m {pct}%{suffix}  \033[90m({elapsed:.0f}s)\033[0m"
            print(indicator, end="", flush=True)
            last_progress = True
        elif has_error:
            if last_progress:
                print()
                last_progress = False
            # 错误：红色高亮，简化路径
            short = _shorten_error_path(plain)
            print(f"  \033[31m✗ {short}\033[0m")
        elif has_warning:
            if last_progress:
                print()
                last_progress = False
            # 警告：黄色，只显示简要
            short = _shorten_error_path(plain)
            print(f"  \033[33m⚠ {short}\033[0m")
        elif "Linking" in plain:
            if last_progress:
                print()
                last_progress = False
            print(f"  \033[32m⟶ {plain.strip()}\033[0m")

    proc.wait()
    elapsed = time.time() - start_time

    if last_progress:
        print()

    # 空行分隔
    print()

    return errors, warnings, elapsed


def _shorten_error_path(line):
    """缩短错误/警告中的文件路径，只保留 usr/bsp/Core/ 之后的部分"""
    m = re.search(r"((?:usr|bsp|Core)[/\\]\S+:\d+)", line)
    if m:
        short = m.group(1)
        return f"{short}{line[m.end():]}"
    return line.strip()


def _output_firmware(project_root, build_dir, cfg, bsp_info):
    """编译成功后生成 bin/hex 并输出固件大小"""
    project_name = get_cmake_project_name(project_root)
    elf = build_dir / f"{project_name}.elf"
    if not elf.exists():
        print("  \033[33m⚠ ELF 文件不存在\033[0m")
        return

    # 固件大小 — 解析为表格
    try:
        result = subprocess.run(["arm-none-eabi-size", str(elf)], capture_output=True, text=True, check=True)
        lines = result.stdout.strip().split("\n")
        if len(lines) >= 2:
            headers = lines[0].split()
            values = lines[1].split()
            print("  \033[1m固件大小:\033[0m")
            for h, v in zip(headers, values):
                try:
                    kb = int(v) / 1024
                    print(f"    {h:<6s} {v:>8s}  ({kb:>7.1f} KB)")
                except ValueError:
                    pass  # 最后一列是文件名，跳过
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    # 生成 bin/hex
    firm_name = f"{cfg['prod_name']}-{bsp_info['PROD_SERIES']}"
    fun_v, firm_v = bsp_info["FUN_V"], bsp_info["FIRM_V"]
    date_str = datetime.now().strftime("%y%m%d")
    dist_name = f"{firm_name} {fun_v}_{firm_v}_{date_str}"

    firmware_out = get_firmware_out_dir(project_root)
    firmware_out.mkdir(parents=True, exist_ok=True)

    for ext, fmt in [("bin", "binary"), ("hex", "ihex")]:
        dst = firmware_out / f"{dist_name}.{ext}"
        try:
            subprocess.run(["arm-none-eabi-objcopy", "-O", fmt, str(elf), str(dst)], check=True)
            size_kb = dst.stat().st_size / 1024
            print(f"  \033[32m✓\033[0m {dst.name}  ({size_kb:.1f} KB) → {dst.parent}/")
        except (subprocess.CalledProcessError, FileNotFoundError):
            print(f"  \033[33m⚠ 无法生成 {ext}\033[0m")


def cmd_build(cfg, project_root):
    """完整编译流程：gen + cmake + 输出固件"""
    cmd_gen(cfg, project_root)

    bsp_info = config_parser.parse_bsp_config(project_root / "bsp" / "config.h")
    _check_and_clean_if_type_changed(project_root, bsp_info["FIRMWARE_TYPE"])

    build_dir = project_root / "build"
    toolchain = project_root / "cmake" / "gcc-arm-none-eabi.cmake"
    build_type = cfg.get("build_type", "Debug")

    # cmake 配置（仅首次）
    if not (build_dir / "CMakeCache.txt").exists():
        print("  \033[36mcmake 配置...\033[0m")
        r = subprocess.run([
            "cmake", f"-B{build_dir}", f"-S{project_root}",
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
            f"-DCMAKE_BUILD_TYPE={build_type}",
        ])
        if r.returncode != 0:
            sys.exit(r.returncode)

    # cmake 编译
    fw_type = bsp_info["FIRMWARE_TYPE"]
    print(f"  \033[1m编译 {cfg['prod_name']} ({fw_type})\033[0m")
    proc = subprocess.Popen(
        ["cmake", "--build", str(build_dir), "--", "-k"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1, universal_newlines=True,
    )

    errors, warnings, elapsed = _stream_build_output(proc)

    # 编译统计 — 简洁的一行摘要
    if proc.returncode != 0:
        print(f"  \033[31m✗ 编译失败\033[0m  ({elapsed:.1f}s)")
        sys.exit(proc.returncode)

    status_parts = []
    if errors:
        status_parts.append(f"\033[31m{errors} error\033[0m")
    if warnings:
        status_parts.append(f"\033[33m{warnings} warning\033[0m")
    status_parts.append(f"{elapsed:.1f}s")
    status = "  ".join(status_parts)

    if errors:
        print(f"  \033[31m✗ 编译失败\033[0m  {status}")
        sys.exit(1)
    else:
        print(f"  \033[32m✓ 编译成功\033[0m  {status}")

    _output_firmware(project_root, build_dir, cfg, bsp_info)
