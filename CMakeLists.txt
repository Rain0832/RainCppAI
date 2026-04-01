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

# SimpleAmqpClient
find_library(SIMPLEAMQPCLIENT_LIBRARY
    NAMES SimpleAmqpClient
    PATHS
        /usr/local/lib64
        /usr/local/lib
        /usr/lib64
        /usr/lib
)

# rabbitmq-c
find_library(RABBITMQ_LIBRARY
    NAMES rabbitmq
    PATHS
        /usr/local/lib64
        /usr/local/lib
        /usr/lib64
        /usr/lib
)

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

file(GLOB_RECURSE CHAT_SERVER_SRC
    ${PROJECT_SOURCE_DIR}/AIApps/ChatServer/src/*.cpp
)

# 主程序
set(MAIN_SRC
    ${PROJECT_SOURCE_DIR}/AIApps/ChatServer/src/main.cpp
)

# -----------------------------
# 可执行文件
# -----------------------------

add_executable(http_server
    ${MAIN_SRC}
    ${HTTP_SERVER_SRC}
    ${CHAT_SERVER_SRC}
)

# -----------------------------
# 头文件目录
# -----------------------------

target_include_directories(http_server
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/HttpServer/include
        ${PROJECT_SOURCE_DIR}/AIApps/ChatServer/include
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

        ${SIMPLEAMQPCLIENT_LIBRARY}
        ${RABBITMQ_LIBRARY}
)

# -----------------------------
# 调试信息
# -----------------------------

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "ONNX Runtime library: ${ONNXRUNTIME_LIBRARY}")