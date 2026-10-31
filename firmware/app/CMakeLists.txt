# app_main_init / app_main_start 由板级 main 调用，不在这里定义 main。
set(SERVICES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../services")
file(GLOB APP_SERVICE_SOURCES CONFIGURE_DEPENDS
    "${SERVICES_DIR}/*.c"
    "${SERVICES_DIR}/abs/*.c"
    "${SERVICES_DIR}/utils/*.c"
)
add_library(app OBJECT app_main.c ${APP_SERVICE_SOURCES})
target_include_directories(app PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SERVICES_DIR}"
    "${SERVICES_DIR}/abs"
    "${SERVICES_DIR}/utils"
)
target_link_libraries(app PUBLIC protocol xfoc bsp PRIVATE board_headers)
