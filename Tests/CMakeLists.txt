add_executable(test_config test_config.cpp)
target_include_directories(test_config PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(test_config gtest_main)
target_sources(test_config PRIVATE ${PROJECT_SOURCE_DIR}/Common/Config/ConfigManager.cpp)
add_test(NAME test_config COMMAND test_config)

add_executable(test_db_pool test_db_pool.cpp)
target_link_libraries(test_db_pool gtest_main storage muduo_net muduo_base)
target_link_libraries(test_db_pool spdlog::spdlog)
target_sources(test_db_pool PRIVATE ${PROJECT_SOURCE_DIR}/Common/Logging/Logger.cpp ${PROJECT_SOURCE_DIR}/Common/Logging/LogContext.cpp)
add_test(NAME test_db_pool COMMAND test_db_pool)

# v3.3.0: ReAct Agent 状态机（Issue #12）单元测试
add_executable(test_react_agent test_react_agent.cpp)
target_include_directories(test_react_agent PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/AIEngine/include)
target_link_libraries(test_react_agent gtest_main)
target_sources(test_react_agent PRIVATE
    ${PROJECT_SOURCE_DIR}/AIEngine/src/llm/ReActLoop.cpp
    ${PROJECT_SOURCE_DIR}/AIEngine/src/llm/ReActStateMachine.cpp
    ${PROJECT_SOURCE_DIR}/AIEngine/src/llm/TokenRouter.cpp)
add_test(NAME test_react_agent COMMAND test_react_agent)
