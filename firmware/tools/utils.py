"""共享工具函数：项目名称解析、路径常量、错误处理"""
import re
import sys
from pathlib import Path


def get_cmake_project_name(project_root):
    """从顶层 CMakeLists.txt 的 project() 解析工程名"""
    cmake_file = project_root / "CMakeLists.txt"
    if not cmake_file.exists():
        sys.exit(f"错误: 找不到顶层 CMakeLists.txt: {cmake_file}")
    text = cmake_file.read_text(encoding="utf-8")
    m = re.search(r"^\s*project\s*\(\s*([\w.+-]+)", text, re.MULTILINE)
    if not m:
        sys.exit("错误: 在 CMakeLists.txt 中未找到 project(...)")
    return m.group(1)


def get_firmware_out_dir(project_root):
    """固件输出目录

    项目结构: X-motor-Drive/firmware/{app,bootload,board,tools}/ 与 firmware_out/
    故 project_root 即 firmware/，输出目录为其下的 firmware_out/。
    """
    return project_root / "firmware_out"


def clean_ansi(text):
    """移除 ANSI 转义序列"""
    return re.sub(r"\x1b\[[0-9;]*m", "", text)
