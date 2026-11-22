#include "xmath.h"

#include <assert.h>
#include <float.h>
#include <stdio.h>

#define NEAR(actual, expected, tolerance) \
    assert(fabs((double)(actual) - (double)(expected)) <= (tolerance))

static void test_precision_and_domains(void) {
    /* f64 轨迹不能因统一 API 而截断为 f32。 */
    double x = 1.0 + 0x1p-40;
    assert(XMAX(x, 1.0) == x);
    assert(XMIN(x, 2.0) == x);
    assert(XFABS(-x) == x);
    assert(XCLAMPD(x, 0, 2) == x);
    NEAR(XSQRT(2), sqrt(2), 1e-15);
    NEAR(XCBRT(-27), -3, 1e-14);
    assert(signbit(XCOPYSIGN(0, -1)));
    assert(signbit(XCOPYSIGNF(0, -1)));
    assert(XFROUNDF(1.5f) == 2);
    assert(XFSIGN(-0.0f) == 0);
    assert(XFSIGN(-3.0f) == -1);
    NEAR(XEXPF(-2), expf(-2), 1e-7);
    NEAR(XTANHF(-2), tanhf(-2), 1e-7);
    for (unsigned i = 1; i < 1000; ++i) {
        float value = i * 0.001f;
        NEAR(XSQRTF(value), sqrtf(value), 2e-6);
    }
}

static void test_wrap_and_trigonometry(void) {
    const float angles[] = {0, -0.0f, XMATH_PI, -XMATH_PI,
        XMATH_2PI, -XMATH_2PI, 100, -100,
        FLT_TRUE_MIN, -FLT_TRUE_MIN};
    for (unsigned i = 0; i < sizeof(angles) / sizeof(angles[0]); ++i) {
        float wrapped = XANGLE_2PI(angles[i]);
        assert(wrapped >= 0 && wrapped < XMATH_2PI);
        float centered = XANGLE_PI(angles[i]);
        assert(centered >= -XMATH_PI && centered < XMATH_PI);
        double reference = fmod((double)angles[i], (double)XMATH_2PI);
        if (reference < 0)
            reference += XMATH_2PI;
        double distance = fabs(wrapped - reference);
        if (distance > XMATH_PI)
            distance = XMATH_2PI - distance;
        assert(distance <= 8e-6);
    }
    assert(XANGLE_PI(XMATH_PI) == -XMATH_PI);
    for (int i = -720; i <= 720; ++i) {
        float angle = i * XMATH_PI / 180.0f;
        float sine, cosine;
        XSIN_COS(angle, &sine, &cosine);
        NEAR(sine, sinf(angle), 3e-5);
        NEAR(cosine, cosf(angle), 3e-5);
        NEAR(XSIN(angle), sinf(angle), 3e-5);
        NEAR(XCOS(angle), cosf(angle), 3e-5);
        NEAR(XATAN2(sine, cosine), atan2f(sine, cosine), 3e-5);
    }
}

static void test_hypot(void) {
    const float values[][2] = {{3, 4}, {0.1f, -0.2f}, {0, 0}, {-3, -4}};
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        NEAR(XHYPOT(values[i][0], values[i][1]), hypotf(values[i][0], values[i][1]), 1e-6);
    NEAR(XNORM2(3, 4), 25, 1e-6);
    assert(XCLAMP(3, 0, 1) == 1);
}

int main(void) {
    test_precision_and_domains();
    test_wrap_and_trigonometry();
    test_hypot();
    puts("xmath regression tests passed");
    return 0;
}
