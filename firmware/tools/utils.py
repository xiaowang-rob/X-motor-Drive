"""共享工具函数：项目名称解析、路径常量、错误处理"""
import re
import sys
from pathlib import Path


def get_cmake_project_name(project_root):
    """从顶层 CMakeLists.txt 解析 CMAKE_PROJECT_NAME"""
    cmake_file = project_root / "CMakeLists.txt"
    if not cmake_file.exists():
        sys.exit("错误: 找不到顶层 CMakeLists.txt")
    text = cmake_file.read_text(encoding="utf-8")
    m = re.search(r"set\s*\(\s*CMAKE_PROJECT_NAME\s+(\w+)\s*\)", text)
    if not m:
        sys.exit("错误: 在 CMakeLists.txt 中未找到 CMAKE_PROJECT_NAME")
    return m.group(1)


def get_firmware_out_dir(project_root):
    """获取固件输出目录（项目根目录的兄弟目录 firmware_out/）"""
    # 项目结构: X-motor-Drive/Firmware/series_P/O_V1.2/
    # 固件输出: X-motor-Drive/firmware_out/
    return project_root.parent.parent.parent / "firmware_out"


def clean_ansi(text):
    """移除 ANSI 转义序列"""
    return re.sub(r"\x1b\[[0-9;]*m", "", text)
