"""巴特沃斯速度回路低通滤波器系数计算"""

import sys


def compute_filter_coeffs(cfg):
    try:
        from scipy import signal
    except ImportError:
        print("错误: 需要 scipy 库。请运行: pip install scipy numpy")
        sys.exit(1)

    FS = 20000
    ORDER = 2
    filt = cfg["filter"]

    fc_speed = filt["pll_bandwidth_hz"] * filt["speed_lpf_factor"]
    sos = signal.butter(ORDER, fc_speed, btype="low", output="sos", fs=FS)
    b0, b1, b2, a0, a1, a2 = sos[0]
    lpf_w = [b0 / a0, b1 / a0, b2 / a0, -a1 / a0, -a2 / a0]

    return {
        "lpf_w": lpf_w,
        "fc_speed": fc_speed,
        "FS": FS,
    }
