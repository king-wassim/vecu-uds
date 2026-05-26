add_executable(test_uds_smoke test_uds_smoke.c)
target_link_libraries(test_uds_smoke PRIVATE vecu_core)
add_test(NAME uds_smoke COMMAND test_uds_smoke)

add_executable(test_can_io_smoke test_can_io_smoke.c)
target_link_libraries(test_can_io_smoke PRIVATE vecu_core)
add_test(NAME can_io_smoke COMMAND test_can_io_smoke)

add_executable(test_isotp_smoke test_isotp_smoke.c)
target_link_libraries(test_isotp_smoke PRIVATE vecu_core)
add_test(NAME isotp_smoke COMMAND test_isotp_smoke)
