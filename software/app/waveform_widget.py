"""实时波形控件：固定容量 numpy 缓冲 + 定时批量刷新。

写入路径是 1kHz 的数据流热路径，因此每次只写一个槽位、不做全量拷贝；
绘制路径按 33ms 批量刷新，只把有变化的通道整理成快照后交给 pyqtgraph。
"""

import logging

import numpy as np
import pyqtgraph as pg
from PyQt5.QtCore import QEvent, Qt, QTimer, pyqtSignal
from PyQt5.QtGui import QFont
from PyQt5.QtWidgets import QVBoxLayout, QWidget

logger = logging.getLogger("Wave")

CHANNEL_COUNT = 3
# 通道配色唯一来源：控制页的色条与曲线笔色都引用这里。
CHANNEL_COLORS = ("#58cfc2", "#f5ae50", "#e77fa3", "#9cce70", "#b59af7")
DEFAULT_CHANNEL_COLORS = CHANNEL_COLORS

# 固件 app_comm.c 的 STREAM_INTERVAL_MS：数据流每 1ms 主动上报一帧。
STREAM_INTERVAL_MS = 1.0
# 自动 X 轴默认显示的时长（秒），跨度由采样率换算成点数。
DEFAULT_VIEW_SECONDS = 3.0


class WaveformWidget(QWidget):
    """波形显示控件 - 支持 3 通道实时波形绘制。"""

    auto_x_state_changed = pyqtSignal(bool)
    auto_y_state_changed = pyqtSignal(bool)

    def __init__(
        self,
        parent=None,
        colors=DEFAULT_CHANNEL_COLORS,
        max_points=4000,
        min_view_points=1000,
        max_view_points=3000,
        sample_interval_ms=STREAM_INTERVAL_MS,
        view_seconds=DEFAULT_VIEW_SECONDS,
    ):
        super().__init__(parent)

        # 核心配置
        self.max_points = int(max_points)      # 每通道缓冲容量（环形覆盖）
        self.min_view_points = int(min_view_points)  # 数据不足时 X 轴最小跨度
        self.max_view_points = int(max_view_points)  # 自动 X 轴最大显示跨度
        self.sample_interval_ms = sample_interval_ms  # 采样间隔（毫秒），None 表示未知
        self.view_seconds = float(view_seconds)      # 自动 X 轴显示的时长
        self.channel_colors = tuple(colors)

        # 每通道固定容量缓冲：写指针 + 已填充数量，写入为 O(1)
        self.wave_data = [np.zeros(self.max_points) for _ in range(CHANNEL_COUNT)]
        self._heads = [0] * CHANNEL_COUNT
        self._counts = [0] * CHANNEL_COUNT
        self._x_axis = np.arange(self.max_points, dtype=np.float64)
        # 最近一次绘制用的 y 视图，供悬停与自动缩放复用
        self._display = [None] * CHANNEL_COUNT

        self.is_running = True
        self.auto_x = True
        self.auto_y = True

        # 批量更新优化：标记有变化的通道，定时刷新
        self._dirty_channels = set()

        self.init_ui()

        self.curves = [
            self.plot_widget.plot(
                pen=pg.mkPen(color=self.channel_colors[index], width=2)
            )
            for index in range(CHANNEL_COUNT)
        ]

        # 末端值标签 - 共用字体
        label_font = QFont()
        label_font.setPointSize(11)
        self.value_labels = []
        for index in range(CHANNEL_COUNT):
            label = pg.TextItem(
                text="N/A",
                color=self.channel_colors[index],
                border=pg.mkPen(80, 80, 80, 200),
                fill=pg.mkBrush(35, 35, 38, 220),
                anchor=(0.5, 0.5),
            )
            label.setFont(label_font)
            label.setZValue(10)
            self.plot_widget.addItem(label)
            self.value_labels.append(label)

        # ── 批量更新定时器（33ms ≈ 30fps，替代逐点更新） ──
        self._batch_timer = QTimer()
        self._batch_timer.timeout.connect(self._batch_update_plot)
        self._batch_timer.start(33)

        # ── 自动缩放定时器（200ms，与刷新解耦） ──
        self.auto_scale_timer = QTimer()
        self.auto_scale_timer.timeout.connect(self.auto_scale_axes)
        self.auto_scale_timer.start(200)

        # 交互
        self.plot_widget.setMouseEnabled(x=True, y=True)
        self.plot_widget.getViewBox().setMouseMode(pg.ViewBox.PanMode)
        self.plot_widget.scene().sigMouseClicked.connect(self._on_mouse_clicked)

        # 滚轮事件拦截（viewport级）
        self.plot_widget.viewport().installEventFilter(self)

        # 悬停标签
        self.hover_label = pg.TextItem(
            text="",
            color="#ffffff",
            border=pg.mkPen(80, 80, 80, 200),
            fill=pg.mkBrush(35, 35, 38, 220),
            anchor=(0.5, 1.5),
        )
        self.hover_label.setFont(QFont("Arial", 10, QFont.Bold))
        self.plot_widget.addItem(self.hover_label)
        self.hover_label.hide()

        self._mouse_proxy = pg.SignalProxy(
            self.plot_widget.scene().sigMouseMoved,
            rateLimit=60,
            slot=self._on_mouse_hover,
        )

        # 手动拖动时同步关闭自动缩放
        self.plot_widget.getViewBox().sigRangeChangedManually.connect(
            self._on_manual_range_change
        )

    # ---------- UI ----------
    def init_ui(self):
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)

        self.plot_widget = pg.PlotWidget(background="#39373F")
        self.plot_widget.setLabel("left", "Y轴", color="#ffffff", size="10pt")
        self.plot_widget.setLabel("bottom", "X轴", color="#ffffff", size="10pt")
        self.plot_widget.getAxis("left").setTextPen("#aaaaaa")
        self.plot_widget.getAxis("bottom").setTextPen("#aaaaaa")
        self.plot_widget.getAxis("left").setPen("#777777")
        self.plot_widget.getAxis("bottom").setPen("#777777")
        self.plot_widget.showGrid(x=True, y=True, alpha=0.3)

        layout.addWidget(self.plot_widget)
        self.setLayout(layout)

    # ---------- 事件处理 ----------
    def eventFilter(self, obj, event):
        if event.type() == QEvent.Wheel:
            if obj is self.plot_widget.viewport():
                self.handle_wheel_event(event)
                return True
        return super().eventFilter(obj, event)

    def handle_wheel_event(self, event):
        scale = 0.9 if event.angleDelta().y() > 0 else 1.1
        if event.modifiers() & Qt.ShiftModifier:
            self.plot_widget.getViewBox().scaleBy(x=scale)
            self.set_auto_x_scale(False)
        else:
            self.plot_widget.getViewBox().scaleBy(y=scale)
            self.set_auto_y_scale(False)
        event.accept()

    def _on_manual_range_change(self, _):
        """手动平移/缩放时，同时关闭XY自动缩放"""
        self.set_auto_x_scale(False)
        self.set_auto_y_scale(False)

    def _on_mouse_clicked(self, event):
        if event.double():
            self.plot_widget.enableAutoRange()
            self.set_auto_x_scale(True)
            self.set_auto_y_scale(True)

    # ---------- 自动缩放逻辑 ----------
    def auto_scale_axes(self):
        if self.auto_y:
            self.auto_scale_y_axis()
        if self.auto_x:
            self.auto_scale_x_axis()

    def auto_scale_y_axis(self):
        """Y 轴自动缩放 — 复用绘制缓存，避免每次重新组装数组。"""
        min_val = float("inf")
        max_val = float("-inf")

        for wave in self._display:
            if wave is None or len(wave) == 0:
                continue
            finite = wave[np.isfinite(wave)]
            if finite.size == 0:
                continue
            min_val = min(min_val, float(finite.min()))
            max_val = max(max_val, float(finite.max()))

        if min_val > max_val:
            return

        margin = 1.0 if max_val == min_val else (max_val - min_val) * 0.1
        # padding=0：自动缩放不留空白边，否则范围会外扩 2% 并随刷新抖动。
        self.plot_widget.setYRange(min_val - margin, max_val + margin, padding=0)

    def set_sample_interval(self, interval_ms):
        """告知采样间隔，自动 X 轴据此把“显示时长”换算成点数。"""
        self.sample_interval_ms = interval_ms

    def window_points(self):
        """自动 X 轴要显示的采样点数。

        已知采样间隔时由 view_seconds 换算，跨度只随采样率变化，不会因为
        波形幅度起伏而抖动；未知时退回 min/max_view_points。
        """
        interval = self.sample_interval_ms
        if interval and interval > 0:
            points = int(round(self.view_seconds * 1000.0 / interval))
            return max(1, min(points, self.max_points))
        return max(self.min_view_points, self.max_view_points)

    def auto_scale_x_axis(self):
        """X 轴自动缩放 — 固定跨度跟随最新数据。

        数据不足一个窗口时左对齐，超过后始终贴住最新点。
        """
        total_points = max(self._counts)
        if total_points <= 0:
            return

        span = self.window_points()
        if total_points <= span:
            self.plot_widget.setXRange(0, span, padding=0)
        else:
            self.plot_widget.setXRange(total_points - span, total_points, padding=0)

    # ---------- 数据写入 ----------
    def _append_value(self, row, value):
        """写入单个采样点；缓冲满后按环形覆盖最旧数据。"""
        count = self._counts[row]
        if count < self.max_points:
            self.wave_data[row][count] = value
            self._counts[row] = count + 1
            return
        head = self._heads[row]
        self.wave_data[row][head] = value
        self._heads[row] = (head + 1) % self.max_points

    def align_row(self, row):
        """把该通道对齐到当前最长通道的时间轴，空缺段用 NaN 占位。

        中途开启的通道因此不会从 X=0 重新起跑；旧数据紧贴最新位置，
        前面的空白由绘制时的 connect="finite" 跳过，不画成假线。
        """
        if not 0 <= row < CHANNEL_COUNT:
            return
        target = max(self._counts)
        if target <= self._counts[row]:
            return
        existing = self._channel_view(row)
        old_length = 0 if existing is None else len(existing)
        self.wave_data[row].fill(np.nan)
        if old_length:
            self.wave_data[row][target - old_length:target] = existing
        self._heads[row] = 0
        self._counts[row] = target
        self._dirty_channels.add(row)

    def _channel_view(self, row):
        """返回该通道按时间顺序排列的缓冲快照（最旧 → 最新）。

        必须返回独立数组：环形写入会覆盖底层缓冲，视图会让已绘制数据错位。
        """
        count = self._counts[row]
        if count == 0:
            return None
        head = self._heads[row]
        if head == 0:
            return self.wave_data[row][:count].copy()
        tail = self.wave_data[row][head:]
        return np.concatenate((tail, self.wave_data[row][: count - len(tail)]))

    def add_waveform_data(self, channel, data):
        """写入采样值。channel 可以是单行，也可以是行号集合（同一值上屏）。"""
        if not self.is_running:
            return

        rows = (channel,) if isinstance(channel, (int, np.integer)) else tuple(channel)
        rows = [int(row) for row in rows if 0 <= int(row) < CHANNEL_COUNT]
        if not rows:
            return

        if isinstance(data, (list, tuple, np.ndarray)):
            values = data
        else:
            values = (data,)

        for value in values:
            value = float(value)
            for row in rows:
                self._append_value(row, value)

        # 标记该通道有更新（批量定时器会统一刷新）
        self._dirty_channels.update(rows)

    # ---------- 绘制 ----------
    def _redraw(self, rows=None):
        targets = range(CHANNEL_COUNT) if rows is None else rows
        for row in targets:
            y_data = self._channel_view(row)
            self._display[row] = y_data
            if y_data is None or len(y_data) == 0:
                self.curves[row].setData([], [])
                self.value_labels[row].setVisible(False)
                continue

            x_data = self._x_axis[: len(y_data)]
            # connect="finite"：对齐用的 NaN 前导不画线，曲线从首个有效点开始。
            self.curves[row].setData(x_data, y_data, connect="finite")
            valid = np.nonzero(np.isfinite(y_data))[0]
            if valid.size == 0:
                self.value_labels[row].setVisible(False)
                continue

            last_x = int(valid[-1])
            last_y = float(y_data[last_x])
            self.value_labels[row].setText(f"{last_y:.3f}")
            self.value_labels[row].setPos(last_x + 8, last_y)
            self.value_labels[row].setVisible(True)

    def _batch_update_plot(self):
        """批量刷新：仅更新有数据变化的通道（由定时器触发，约30fps）"""
        if not self.is_running or not self._dirty_channels:
            return
        rows = list(self._dirty_channels)
        self._dirty_channels.clear()
        self._redraw(rows)

    def update_plot(self):
        """立即强制刷新全部通道（保留对外接口，用于clear等场景）"""
        if not self.is_running:
            return
        self._redraw()
        self._dirty_channels.clear()

    def clear_row(self, row):
        """只清空一个通道的缓冲与曲线，其他通道不受影响。"""
        if not 0 <= row < CHANNEL_COUNT:
            return
        self.wave_data[row].fill(0.0)
        self._heads[row] = 0
        self._counts[row] = 0
        self._display[row] = None
        self._dirty_channels.discard(row)
        self.curves[row].setData([], [])
        self.value_labels[row].setVisible(False)

    def clear_waveforms(self):
        self._counts = [0] * CHANNEL_COUNT
        self._heads = [0] * CHANNEL_COUNT
        self._display = [None] * CHANNEL_COUNT
        self._dirty_channels.clear()
        for index in range(CHANNEL_COUNT):
            self.curves[index].setData([], [])
            self.value_labels[index].setVisible(False)
        logger.debug("波形已清除")

    def set_auto_x_scale(self, enable: bool):
        if self.auto_x != enable:
            self.auto_x = enable
            self.auto_x_state_changed.emit(enable)
            logger.debug("X自动缩放 -> %s", enable)

    def set_auto_y_scale(self, enable: bool):
        if self.auto_y != enable:
            self.auto_y = enable
            self.auto_y_state_changed.emit(enable)
            logger.debug("Y自动缩放 -> %s", enable)

    def start(self):
        self.is_running = True
        logger.debug("波形启动")

    def pause(self):
        self.is_running = False
        logger.debug("波形暂停")

    # ---------- 悬停显示 ----------
    def _on_mouse_hover(self, evt):
        pos = evt[0] if isinstance(evt, tuple) else evt
        view_box = self.plot_widget.getViewBox()

        if not self.plot_widget.sceneBoundingRect().contains(pos):
            self.hover_label.hide()
            return

        mouse_view = view_box.mapSceneToView(pos)
        x_range, y_range = view_box.viewRange()
        x_span = x_range[1] - x_range[0] + 1e-6
        y_span = y_range[1] - y_range[0] + 1e-6

        min_dist = float("inf")
        best_y = None

        for y_data in self._display:
            if y_data is None:
                continue
            start = max(0, int(x_range[0]))
            end = min(len(y_data) - 1, int(x_range[1]))
            if end < start:
                continue

            y_slice = y_data[start : end + 1]
            finite = np.isfinite(y_slice)
            if not finite.any():
                continue

            x_slice = self._x_axis[start : end + 1]
            dx = (x_slice - mouse_view.x()) / x_span
            dy = (y_slice - mouse_view.y()) / y_span
            dist = np.where(finite, dx * dx + dy * dy, np.inf)
            index = int(np.argmin(dist))
            if dist[index] < min_dist:
                min_dist = float(dist[index])
                best_y = float(y_slice[index])

        # 阈值沿用原距离 0.05（这里用平方距离，省一次开方）
        if min_dist < 0.0025 and best_y is not None:
            self.hover_label.setHtml(
                f'<span style="color:#ffffff; font-weight:bold;">{best_y:.3f}</span>'
            )
            self.hover_label.setPos(mouse_view.x(), best_y)
            self.hover_label.show()
        else:
            self.hover_label.hide()

    def leaveEvent(self, event):
        super().leaveEvent(event)
        self.hover_label.hide()
