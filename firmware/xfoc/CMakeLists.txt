# motor 只归档电机控制算法源码；不编入 BSP、services 或应用启动逻辑。
file(GLOB MC_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/abs/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ctl/*.c"
)
add_library(motor STATIC ${MC_SOURCES})
target_include_directories(motor PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/abs"
    "${CMAKE_CURRENT_SOURCE_DIR}/ctl"
)
target_link_libraries(motor PUBLIC protocol xmath)
set_target_properties(motor PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
