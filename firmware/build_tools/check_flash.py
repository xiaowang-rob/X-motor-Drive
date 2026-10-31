#!/usr/bin/env python3
"""检查固件是否需要重新烧录（基于 ELF 时间戳）"""
import json
import sys
import subprocess
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS_DIR))

import build

PROJECT_ROOT = TOOLS_DIR.parent        # firmware_new/
REPO_ROOT = PROJECT_ROOT.parent        # 仓库根


def _default_target():
    """从仓库根 project.json 读取默认板卡与固件类型"""
    cfg_path = PROJECT_ROOT / "project.json"
    if cfg_path.exists():
        cfg = json.loads(cfg_path.read_text())
        return cfg.get("board", "xdr_s"), cfg.get("fw_type", "app")
    return "xdr_s", "app"


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "skip":
        print("跳过烧录（参数指定）")
        return

    board, fw_type = _default_target()
    elf = build._elf_path(board, fw_type)
    if not elf.exists():
        print(f"ELF 不存在（{elf}），需要先编译")
        sys.exit(1)

    # 简单判断：ELF 存在即烧录
    print("烧录固件...")
    subprocess.run(
        [sys.executable, str(TOOLS_DIR / "build.py"), "flash",
         f"--board={board}", f"--type={fw_type}"],
        cwd=PROJECT_ROOT, check=True,
    )
    print("烧录完成")


if __name__ == "__main__":
    main()
