add_library(vecu_core STATIC
    uds.c
    vecu.c
)
target_include_directories(vecu_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_executable(vecu_uds main.c)
target_link_libraries(vecu_uds PRIVATE vecu_core)
