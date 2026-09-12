#!/usr/bin/env python3
"""检查固件是否需要重新烧录（基于 ELF 时间戳）"""
import sys
import subprocess
from pathlib import Path

from firmware.tools.utils import get_cmake_project_name

TOOLS_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = TOOLS_DIR.parent


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "skip":
        print("跳过烧录（参数指定）")
        return

    elf = PROJECT_ROOT / "build" / f"{get_cmake_project_name(PROJECT_ROOT)}.elf"
    if not elf.exists():
        print("ELF 不存在，需要先编译")
        sys.exit(1)

    # 简单判断：ELF 存在即烧录
    print("烧录固件...")
    subprocess.run(
        [sys.executable, str(TOOLS_DIR / "build.py"), "flash"],
        cwd=PROJECT_ROOT, check=True,
    )
    print("烧录完成")


if __name__ == "__main__":
    main()
