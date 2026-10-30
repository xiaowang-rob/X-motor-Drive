# app_main_init / app_main_start 由板级 main 调用，不在这里定义 main。
add_library(app OBJECT app_main.c)
target_include_directories(app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(app PUBLIC xfoc services bsp)
