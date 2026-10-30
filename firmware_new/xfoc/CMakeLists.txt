# xfoc 只归档算法源码；不编入 BSP、services 或应用启动逻辑。
file(GLOB XFOC_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/abs/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ctl/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.c"
)
add_library(xfoc STATIC ${XFOC_SOURCES})
target_include_directories(xfoc PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/abs"
    "${CMAKE_CURRENT_SOURCE_DIR}/ctl"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils"
)
target_link_libraries(xfoc PUBLIC protocol xmath)
set_target_properties(xfoc PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
