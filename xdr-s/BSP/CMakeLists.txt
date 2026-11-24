set(TINYUSB_SRC "${PROJECT_SOURCE_DIR}/Middlewares/Third_Party/tinyusb/src")

add_library(bsp STATIC
        math/bsp_math.c
        flash/bsp_mcu_flash.c
        bus/bap_can.c
        enc/bsp_enc_adc.c
        enc/bsp_enc_spi.c
        enc/bsp_enc_tim.c
        enc/abi/enc_abi.c
        enc/hall/enc_hall.c
        enc/sincos/enc_sincos.c
        enc/spi/enc_as5047.c
        enc/spi/enc_mt6816.c
        enc/spi/enc_mt6835.c
        mcu/bsp_mcu.c
        led/bsp_gpio_led.c
        pwm/bsp_mcu_pwm.c
        sample/bsp_mcu_adc.c
        uart/bsp_mcu_uart.c
        usb/bsp_usb.c
        usb/bsp_usb_vendor.c
        dfu/bsp_dfu.c
        "${TINYUSB_SRC}/tusb.c"
        "${TINYUSB_SRC}/common/tusb_fifo.c"
        "${TINYUSB_SRC}/device/usbd.c"
        "${TINYUSB_SRC}/class/vendor/vendor_device.c"
        "${TINYUSB_SRC}/class/dfu/dfu_device.c"
        "${TINYUSB_SRC}/portable/st/stm32_fsdev/dcd_stm32_fsdev.c"
        "${TINYUSB_SRC}/portable/st/stm32_fsdev/fsdev_common.c"
        bsp_config.h
)

target_include_directories(bsp PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/math"
        "${CMAKE_CURRENT_SOURCE_DIR}/flash"
        "${CMAKE_CURRENT_SOURCE_DIR}/usb"
        "${CMAKE_CURRENT_SOURCE_DIR}/dfu"
        "${CMAKE_CURRENT_SOURCE_DIR}/bus"
        "${CMAKE_CURRENT_SOURCE_DIR}/enc"
        "${CMAKE_CURRENT_SOURCE_DIR}/enc/abi"
        "${CMAKE_CURRENT_SOURCE_DIR}/enc/hall"
        "${CMAKE_CURRENT_SOURCE_DIR}/enc/sincos"
        "${CMAKE_CURRENT_SOURCE_DIR}/enc/spi"
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu"
        "${CMAKE_CURRENT_SOURCE_DIR}/led"
        "${CMAKE_CURRENT_SOURCE_DIR}/pwm"
        "${CMAKE_CURRENT_SOURCE_DIR}/sample"
        "${CMAKE_CURRENT_SOURCE_DIR}/uart"
)

target_include_directories(bsp PRIVATE "${TINYUSB_SRC}")

target_link_libraries(bsp PUBLIC
        stm32cubemx
        motor_interfaces
        "${PROJECT_SOURCE_DIR}/Middlewares/ST/ARM/DSP/Lib/libarm_cortexM4lf_math.a"
        m
)
