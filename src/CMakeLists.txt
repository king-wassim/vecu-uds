# Spécifie les fonctionnalités du système (POSIX) pour le compilateur
add_compile_definitions(_XOPEN_SOURCE=700 _DEFAULT_SOURCE)

add_library(vecu_core STATIC
    uds.c
    vecu.c
    can_io.c
)
target_include_directories(vecu_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_executable(vecu_uds main.c)
target_link_libraries(vecu_uds PRIVATE vecu_core)
