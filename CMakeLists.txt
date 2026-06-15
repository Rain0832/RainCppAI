cmake_minimum_required(VERSION 3.16)

project(RainCppAI LANGUAGES CXX)

# C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------
# 查找依赖
# -----------------------------

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenCV REQUIRED)

# MySQL Connector
find_library(MYSQLCPPCONN_LIBRARY
    NAMES mysqlcppconn mysqlcppconn8
    PATHS
        /usr/local/lib64
        /usr/local/lib
        /usr/lib64
        /usr/lib
)

# ONNX Runtime
find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    PATHS
        /usr/local/onnxruntime/lib
        /usr/local/lib
        /usr/lib
)

if(NOT ONNXRUNTIME_LIBRARY)
    message(FATAL_ERROR "ONNX Runtime library not found")
endif()

# mysqlclient
find_library(MYSQLCLIENT_LIBRARY
    NAMES mysqlclient
    PATHS
        /usr/lib64/mysql
        /usr/lib64
        /usr/local/lib64
        /usr/local/lib
)

# -----------------------------
# 源文件
# -----------------------------

file(GLOB_RECURSE HTTP_SERVER_SRC
    ${PROJECT_SOURCE_DIR}/HttpServer/src/*.cpp
)
# 排除已迁移至 Storage/ 模块的旧 DB 文件
list(FILTER HTTP_SERVER_SRC EXCLUDE REGEX ".*/utils/db/.*")

file(GLOB_RECURSE STORAGE_SRC
    ${PROJECT_SOURCE_DIR}/Storage/src/*.cpp
)

file(GLOB_RECURSE AI_ENGINE_SRC
    ${PROJECT_SOURCE_DIR}/AIEngine/src/*.cpp
)
# 移除已废弃的 MQ 模块（RabbitMQ 链路已拆除）
list(FILTER AI_ENGINE_SRC EXCLUDE REGEX ".*/common/MQManager.cpp")

file(GLOB_RECURSE AI_SERVER_CORE_SRC
    ${PROJECT_SOURCE_DIR}/AIServerCore/src/*.cpp
)
list(REMOVE_ITEM AI_SERVER_CORE_SRC ${PROJECT_SOURCE_DIR}/AIServerCore/src/main.cpp)

# 主程序
set(MAIN_SRC
    ${PROJECT_SOURCE_DIR}/AIServerCore/src/main.cpp
)

# -----------------------------
# 可执行文件
# -----------------------------

add_executable(http_server
    ${MAIN_SRC}
    ${HTTP_SERVER_SRC}
    ${STORAGE_SRC}
    ${AI_ENGINE_SRC}
    ${AI_SERVER_CORE_SRC}
)

# -----------------------------
# 头文件目录
# -----------------------------

target_include_directories(http_server
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/HttpServer/include
        ${PROJECT_SOURCE_DIR}/Storage/include
        ${PROJECT_SOURCE_DIR}/AIEngine/include
        ${PROJECT_SOURCE_DIR}/AIServerCore/include
        ${PROJECT_SOURCE_DIR}/3rdparty
        /usr/include/mysql-cppconn-8
        /usr/include/mysql
        /usr/local/include
        /usr/local/include/jdbc
        /usr/local/onnxruntime/include
)

# -----------------------------
# 链接库
# -----------------------------

target_link_libraries(http_server
    PRIVATE
        pthread
        muduo_net
        muduo_base

        ${MYSQLCPPCONN_LIBRARY}
        ${MYSQLCLIENT_LIBRARY}

        OpenSSL::SSL
        OpenSSL::Crypto
        CURL::libcurl

        ${OpenCV_LIBS}

        ${ONNXRUNTIME_LIBRARY}

)

# -----------------------------
# 调试信息
# -----------------------------

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "ONNX Runtime library: ${ONNXRUNTIME_LIBRARY}")