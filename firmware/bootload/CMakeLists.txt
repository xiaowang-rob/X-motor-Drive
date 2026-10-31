set(SERVICES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../services")
# BL 只包含升级所需服务，不链接电机控制或 APP 通信业务。
set(BOOTLOAD_SERVICE_SOURCES
    "${SERVICES_DIR}/parameters.c"
    "${SERVICES_DIR}/utils/crc.c"
    "${SERVICES_DIR}/utils/queue.c"
    "${SERVICES_DIR}/abs/led.c"
    "${SERVICES_DIR}/abs/storage.c"
    "${SERVICES_DIR}/abs/uart_com.c"
    "${SERVICES_DIR}/abs/usb_com.c"
)
add_library(bootload OBJECT bl_main.c bl_comm.c bl_iap.c bl_status.c ${BOOTLOAD_SERVICE_SOURCES})
target_include_directories(bootload PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SERVICES_DIR}"
    "${SERVICES_DIR}/abs"
    "${SERVICES_DIR}/utils"
)
target_link_libraries(bootload PUBLIC protocol PRIVATE board_headers)
