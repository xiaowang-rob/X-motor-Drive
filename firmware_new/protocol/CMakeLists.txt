# 协议只有头文件，供各层显式引用。
add_library(protocol INTERFACE)
target_include_directories(protocol INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
