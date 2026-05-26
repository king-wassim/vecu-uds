# Feature test macros for POSIX + getrandom (Linux glibc 2.25+).
add_compile_definitions(_XOPEN_SOURCE=700 _DEFAULT_SOURCE)

find_package(Threads REQUIRED)

add_library(vecu_core STATIC
    uds.c
    vecu.c
    can_io.c
    isotp.c
    isotp_kernel.c
    did_table.c
    app_engine.c
    dtc_mem.c
    security.c
    queue.c
    threads.c
)
target_include_directories(vecu_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(vecu_core PUBLIC Threads::Threads m)

add_executable(vecu_uds main.c)
target_link_libraries(vecu_uds PRIVATE vecu_core)
