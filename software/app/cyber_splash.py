"""
PyQt5 cinematic splash animation for the supplied X logo.

Requirements:
    pip install PyQt5

Usage:
    from cyber_splash import show_splash

    # Create your main window first, but do not show it yet.
    main_window = MainWindow()
    show_splash(main_window)  # Shows main_window automatically after the intro.

The splash is purely decorative: it does not pretend to load or initialize anything.
"""

from pathlib import Path
import math
import random
import sys

from PyQt5.QtCore import Qt, QTimer, QElapsedTimer, QRectF, QPointF, QByteArray
from PyQt5.QtGui import QColor, QPainter, QPen, QBrush, QRadialGradient, QFont
from PyQt5.QtSvg import QSvgRenderer
from PyQt5.QtWidgets import QApplication, QWidget
from PyQt5.QtCore import QPropertyAnimation, QEasingCurve
from PyQt5.QtWidgets import QGraphicsOpacityEffect

_APP_DIR = Path(__file__).resolve().parent
_LOGO_PATH = _APP_DIR / "images" / "logo.svg"


class CyberSplash(QWidget):
    """Borderless, full-screen cinematic intro using the original SVG logo."""

    DURATION_MS = 1000

    def __init__(self, main_window, duration_ms=None, parent=None):
        super().__init__(parent)

        self.main_window = main_window
        self.duration_ms = duration_ms or self.DURATION_MS

        self.setWindowFlags(Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.Tool)

        self.setAttribute(Qt.WA_TranslucentBackground, True)
        self.setAttribute(Qt.WA_ShowWithoutActivating, False)

        # ==================================================
        # 核心修改：统一让启动画面和主界面完全对齐
        # ==================================================
        # 1. 获取主窗口的尺寸（此时 1440x960 已经确定）
        win_rect = main_window.frameGeometry()

        # 2. 计算屏幕居中位置（因为你的 MainWindow 代码里没看到 move，这里帮它居中）
        screen = QApplication.primaryScreen().availableGeometry()
        win_rect.moveCenter(screen.center())

        # 3. 把计算好的位置直接赋值给 MainWindow
        main_window.move(win_rect.topLeft())

        # 4. 让启动画面使用一模一样的尺寸和位置
        self.setGeometry(win_rect)

        # === 新增：使用 QGraphicsOpacityEffect 代替 windowOpacity ===
        self.opacity_effect = QGraphicsOpacityEffect(self)
        self.opacity_effect.setOpacity(1.0)
        self.setGraphicsEffect(self.opacity_effect)

        # 淡出动画
        self._fade_animation = None
        self._is_fading = False

        # 定时器
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)

        self._clock = QElapsedTimer()
        self._elapsed = 0
        self.setGeometry(main_window.frameGeometry())

        # ==================================================
        # 窗口位置和大小：跟随主窗口，不再使用全屏尺寸
        # ==================================================
        self.setGeometry(main_window.frameGeometry())

        # ==================================================
        # 加载 SVG
        # ==================================================
        svg_source = _LOGO_PATH.read_text(encoding="utf-8")

        # Preserve the original path geometry; only tint the SVG fill.
        white_svg = svg_source.replace('fill="#000000"', 'fill="#FFF1DF"')

        cyan_svg = svg_source.replace('fill="#000000"', 'fill="#E58A3A"')

        self._renderer = QSvgRenderer(QByteArray(white_svg.encode("utf-8")))

        self._glow_renderer = QSvgRenderer(QByteArray(cyan_svg.encode("utf-8")))

        if not self._renderer.isValid() or not self._glow_renderer.isValid():
            raise RuntimeError(f"Cannot load SVG logo: {_LOGO_PATH}")

        # ==================================================
        # 粒子初始化
        # ==================================================
        rng = random.Random(27)
        self._particles = []

        for _ in range(74):
            self._particles.append(
                (
                    rng.random(),
                    rng.random(),
                    rng.uniform(0.35, 1.0),
                    rng.uniform(0.0, math.tau),
                    rng.uniform(0.7, 2.0),
                )
            )

    @staticmethod
    def _clamp01(value):
        return max(0.0, min(1.0, value))

    @staticmethod
    def _ease_out(value):
        value = max(0.0, min(1.0, value))
        return 1.0 - (1.0 - value) ** 3

    @staticmethod
    def _ease_in_out(value):
        value = max(0.0, min(1.0, value))
        return value * value * (3.0 - 2.0 * value)

    def start(self):
        self._clock.start()
        self._timer.start(16)
        self.show()
        self.raise_()
        self.activateWindow()

    def _tick(self):
        self._elapsed = self._clock.elapsed()

        if self._elapsed >= self.duration_ms:
            self._timer.stop()
            self._is_fading = True
            self.fade_out()
            return

        self.update()

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        p.setRenderHint(QPainter.SmoothPixmapTransform, True)

        w, h = self.width(), self.height()
        t = self._elapsed / 1000.0
        progress = self._clamp01(self._elapsed / float(self.duration_ms))
        cx, cy = w * 0.5, h * 0.465

        # Deep navy background with a restrained central blue bloom.
        p.fillRect(self.rect(), QColor("#0B0907"))
        bloom = QRadialGradient(QPointF(cx, cy), min(w, h) * 0.56)
        bloom.setColorAt(0.0, QColor(174, 91, 25, 62))
        bloom.setColorAt(0.42, QColor(93, 48, 18, 30))
        bloom.setColorAt(1.0, QColor(12, 9, 6, 0))
        p.fillRect(self.rect(), QBrush(bloom))

        # Subtle technical grid, fading toward the edges.
        p.save()
        p.setPen(QPen(QColor(157, 105, 55, 16), 1))
        grid = 42
        for x in range(0, w, grid):
            p.drawLine(x, 0, x, h)
        for y in range(0, h, grid):
            p.drawLine(0, y, w, y)
        p.restore()

        # Floating particles drift gently around the center.
        p.save()
        for nx, ny, alpha, phase, radius in self._particles:
            drift_x = math.sin(t * 0.8 + phase) * 13.0
            drift_y = math.cos(t * 0.65 + phase) * 10.0
            x = nx * w + drift_x
            y = ny * h + drift_y
            pulse = 0.55 + 0.45 * math.sin(t * 2.0 + phase)
            color = QColor(255, 174, 91, int(92 * alpha * pulse))
            p.setPen(Qt.NoPen)
            p.setBrush(color)
            p.drawEllipse(QPointF(x, y), radius, radius)
        p.restore()

        # Expanding HUD rings.
        ring_phase = self._clamp01((progress - 0.18) / 0.67)
        if ring_phase > 0:
            ring_radius = min(w, h) * (0.13 + 0.28 * self._ease_out(ring_phase))
            ring_alpha = int(105 * (1.0 - ring_phase) ** 0.7)
            p.save()
            p.setBrush(Qt.NoBrush)
            p.setPen(QPen(QColor(232, 145, 66, ring_alpha), 1.15))
            p.drawEllipse(QPointF(cx, cy), ring_radius, ring_radius)
            p.setPen(QPen(QColor(191, 111, 48, int(ring_alpha * 0.34)), 1))
            p.drawEllipse(QPointF(cx, cy), ring_radius * 1.12, ring_radius * 1.12)
            # Small angular tick marks around the outer ring.
            p.setPen(QPen(QColor(255, 183, 104, int(ring_alpha * 0.82)), 2))
            for i in range(48):
                a = math.tau * i / 48.0
                r1 = ring_radius * (1.04 if i % 4 else 1.01)
                r2 = ring_radius * (1.09 if i % 4 else 1.15)
                p.drawLine(
                    QPointF(cx + math.cos(a) * r1, cy + math.sin(a) * r1),
                    QPointF(cx + math.cos(a) * r2, cy + math.sin(a) * r2),
                )
            p.restore()

        # Main logo: scale-in + amber energy aura + crisp white foreground.
        logo_in = self._ease_out((progress - 0.08) / 0.48)
        logo_out = 1.0 - self._ease_in_out((progress - 0.82) / 0.18)
        logo_alpha = self._clamp01(logo_in) * self._clamp01(logo_out)
        scale = 0.72 + 0.28 * logo_in
        logo_w = min(w * 0.245, h * 0.34) * scale
        logo_h = logo_w * (1024.0 / 1119.0)
        logo_rect = QRectF(cx - logo_w / 2, cy - logo_h / 2, logo_w, logo_h)

        p.save()
        p.setOpacity(logo_alpha * 0.18)
        p.setPen(Qt.NoPen)
        # Multiple offset passes create a clean neon bloom without external effects.
        for spread, alpha in ((10, 0.18), (6, 0.28), (3, 0.42)):
            p.setOpacity(logo_alpha * alpha)
            glow_rect = logo_rect.adjusted(-spread, -spread, spread, spread)
            # Same original vector silhouette, tinted amber for the aura passes.
            self._glow_renderer.render(p, glow_rect)
        p.setOpacity(logo_alpha)
        self._renderer.render(p, logo_rect)
        p.restore()

        # A fast horizontal energy sweep crosses the mark once.
        sweep_phase = self._clamp01((progress - 0.10) / 0.56)
        if 0.0 < sweep_phase < 1.0:
            sweep_x = (cx - logo_w * 0.85) + (logo_w * 1.7) * sweep_phase
            sweep_alpha = int(190 * math.sin(math.pi * sweep_phase))
            p.save()
            p.setPen(QPen(QColor(255, 207, 151, sweep_alpha), 2))
            p.drawLine(
                QPointF(sweep_x, cy - logo_h * 0.68),
                QPointF(sweep_x, cy + logo_h * 0.68),
            )
            p.setPen(QPen(QColor(225, 133, 58, int(sweep_alpha * 0.20)), 9))
            p.drawLine(
                QPointF(sweep_x, cy - logo_h * 0.68),
                QPointF(sweep_x, cy + logo_h * 0.68),
            )
            p.restore()

        # Minimal wordmark and hairline accents; no fake loading/progress indicator.
        text_alpha = int(
            220
            * self._clamp01((progress - 0.38) / 0.28)
            * (1.0 - self._clamp01((progress - 0.88) / 0.12))
        )
        p.save()
        p.setPen(QColor(238, 218, 194, text_alpha))
        font = QFont("Segoe UI", 10)
        font.setLetterSpacing(QFont.AbsoluteSpacing, 5.0)
        p.setFont(font)
        caption_y = cy + logo_h * 0.82 + 28
        p.drawText(
            QRectF(0, caption_y, w, 24),
            Qt.AlignHCenter | Qt.AlignVCenter,
            "X Motor Drive — Tool",
        )
        p.setPen(QPen(QColor(205, 126, 59, int(text_alpha * 0.48)), 1))
        line_w = min(180.0, w * 0.16)
        p.drawLine(
            QPointF(cx - line_w, caption_y + 34), QPointF(cx - 28, caption_y + 34)
        )
        p.drawLine(
            QPointF(cx + 28, caption_y + 34), QPointF(cx + line_w, caption_y + 34)
        )
        p.restore()

        # Opening flash at the very end, then the splash hands over to the app.
        flash = self._clamp01((progress - 0.91) / 0.09)
        if flash > 0:
            p.fillRect(
                self.rect(), QColor(255, 177, 100, int(28 * math.sin(math.pi * flash)))
            )
        p.end()

    def fade_out(self):
        if self._fade_animation is not None:
            return

        # ==================================================
        # 1. 显示主界面 (初始透明度为 0)
        # ==================================================
        self.main_window.setWindowOpacity(0.0)
        self.main_window.show()

        # 因为 FluentWindow 是自定义绘制，稍微处理一下事件保证第一帧渲染完毕
        QApplication.processEvents()

        self.main_window.raise_()
        self.main_window.activateWindow()

        # ==================================================
        # 2. 主界面淡入 (FluentWindow 支持 windowOpacity)
        # ==================================================
        self.main_win_fade = QPropertyAnimation(
            self.main_window, b"windowOpacity", self
        )
        self.main_win_fade.setDuration(600)  # 稍微放慢一点，更有质感
        self.main_win_fade.setStartValue(0.0)
        self.main_win_fade.setEndValue(1.0)
        self.main_win_fade.setEasingCurve(QEasingCurve.OutCubic)
        self.main_win_fade.start()

        # ==================================================
        # 3. 启动画面淡出
        # ==================================================
        self._fade_animation = QPropertyAnimation(self.opacity_effect, b"opacity", self)
        self._fade_animation.setDuration(600)  # 与主界面同步
        self._fade_animation.setStartValue(1.0)
        self._fade_animation.setEndValue(0.0)
        self._fade_animation.setEasingCurve(QEasingCurve.OutCubic)

        self._fade_animation.finished.connect(self._on_fade_finished)
        self._fade_animation.start()

    def _on_fade_finished(self):
        # 动画结束，隐藏启动画面
        self.close()


def show_splash(main_window, duration_ms=CyberSplash.DURATION_MS):
    """
    Start the decorative intro and show main_window when it ends.
    Returns the splash object; keep a reference if desired.
    """
    splash = CyberSplash(main_window, duration_ms=duration_ms)
    splash.start()
    return splash


if __name__ == "__main__":
    # Standalone preview. Replace DemoWindow with your real main window.
    from PyQt5.QtWidgets import QMainWindow, QLabel

    app = QApplication(sys.argv)
    demo = QMainWindow()
    demo.setWindowTitle("X Motor Drive — Tool")
    demo.resize(1100, 720)
    label = QLabel("MAIN APPLICATION", demo)
    label.setAlignment(Qt.AlignCenter)
    demo.setCentralWidget(label)
    splash = show_splash(demo)
    sys.exit(app.exec_())
