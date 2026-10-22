"""本地参数配置存储。

目录约定：
  Software/config/config.json    QFluentWidgets 主题配置（不参与预设列表）
  Software/config/*.json         参数预设（default.json 为默认预设）
"""

import json
from pathlib import Path

from protocol import PARAMETERS


class ConfigStore:
    def __init__(self, directory):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        self.theme_file = "config.json"

    def names(self):
        return sorted(
            path.stem for path in self.directory.glob("*.json")
            if path.name != self.theme_file
        )

    def path(self, name):
        return self.directory / f"{name}.json"

    def load(self, name):
        path = self.path(name)
        if not path.exists():
            raise FileNotFoundError(path)
        raw = json.loads(path.read_text(encoding="utf-8"))
        params = raw.get("params", raw) if isinstance(raw, dict) else raw

        # 兼容旧版按参数索引保存的列表格式
        if isinstance(params, list):
            ordered = sorted(PARAMETERS.values(), key=lambda item: item.id)
            params = {
                meta.id_name: value
                for meta, value in zip(ordered, params)
            }

        if not isinstance(params, dict):
            raise ValueError(f"配置文件格式错误：{path}")
        return params

    def save(self, name, values):
        path = self.path(name)
        path.write_text(
            json.dumps({"params": dict(values)}, indent=4, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        return path

    def delete(self, name):
        if name == "default":
            raise ValueError("default 配置受保护，不能删除")
        self.path(name).unlink(missing_ok=True)
