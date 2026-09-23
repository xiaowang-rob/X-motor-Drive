#include "IF_config.h"

#define BOARD_NAME "XDr-P"
#define BOARD_VERSION "V1.2"
#define FIRM_VERSION "260626" // 这个由编译脚本更新

#define XDR_VERSION_STR (BOARD_NAME "_" BOARD_VERSION "_" FIRM_VERSION)

const char XDR_VERSION[] = XDR_VERSION_STR;

const float MAX_CURRENT = 100;    // MOS最大瞬间电流
const float MAX_VOLTAGE = 34;     // 最大输入电压
const float MIN_VOLTAGE = 14;     // 最小输入电压
const float MAX_TEMPERATURE = 80; // 最大温度