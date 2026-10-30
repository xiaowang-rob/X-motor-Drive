file(GLOB SERVICE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/abs/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.c"
)
add_library(services OBJECT ${SERVICE_SOURCES})
target_include_directories(services PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/abs"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils"
)
target_link_libraries(services PUBLIC protocol xfoc PRIVATE board_headers)
