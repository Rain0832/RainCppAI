add_executable(test_router test_router.cpp)
target_link_libraries(test_router gtest_main httpserver)
add_test(NAME test_router COMMAND test_router)

add_executable(test_config test_config.cpp)
target_link_libraries(test_config gtest_main)
add_test(NAME test_config COMMAND test_config)

add_executable(test_db_pool test_db_pool.cpp)
target_link_libraries(test_db_pool gtest_main storage)
add_test(NAME test_db_pool COMMAND test_db_pool)
