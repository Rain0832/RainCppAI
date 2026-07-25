add_executable(test_config test_config.cpp)
target_include_directories(test_config PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(test_config gtest_main)
target_sources(test_config PRIVATE ${PROJECT_SOURCE_DIR}/Common/Config/ConfigManager.cpp)
add_test(NAME test_config COMMAND test_config)

add_executable(test_db_pool test_db_pool.cpp)
target_link_libraries(test_db_pool gtest_main storage muduo_net muduo_base)
add_test(NAME test_db_pool COMMAND test_db_pool)
