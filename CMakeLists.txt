cmake_minimum_required(VERSION 3.10)

# 引入 gtest（自动下载，不用手动安装）
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Windows 编译器要这个
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 设置项目名称
project(x_pilot)

# 设置C++标准
set(CMAKE_CXX_STANDARD 14)

# 头文件检索路径
include_directories(${CMAKE_SOURCE_DIR}/include)

# 1. 守护进程
add_executable(manager 
    src/manager_main.cpp 
    src/utils.cpp
    src/config.cpp
    src/metrics.cpp 
)

# 2. 通信进程 (重要修改：增加了 tcp_client.cpp 和 ipc.cpp)
add_executable(comm_process 
    src/comm_main.cpp 
    src/utils.cpp
    src/config.cpp
    src/ipc.cpp           # 新增：Comm 需要连接 IPC
    src/tcp_client.cpp    # 新增：Comm 需要连接 TCP
    src/protocol.cpp
    src/metrics.cpp
)

# 3. 控制进程
add_executable(control_process 
    src/control_main.cpp 
    src/utils.cpp
    src/config.cpp
    src/ipc.cpp
    src/metrics.cpp   
)

# 4. 数据可视进程
add_executable(view_process 
    src/view_main.cpp 
    src/utils.cpp
    src/config.cpp
)

# 5. 测试客户端
add_executable(ipc_test_client
    test/ipc_test_client.cpp
    src/ipc.cpp
    src/utils.cpp
)

# ==================== 单元测试 ====================
enable_testing()

# 测试辅助库：把要测的源码打包成一个静态库，给各测试复用
add_library(xpilot_common STATIC
    src/utils.cpp
    src/config.cpp
    src/protocol.cpp
    src/metrics.cpp
)
target_include_directories(xpilot_common PUBLIC ${CMAKE_SOURCE_DIR}/include)

# 测试1：日志模块
add_executable(test_logger test/test_logger.cpp)
target_link_libraries(test_logger PRIVATE xpilot_common gtest_main)

# 测试2：配置加载
add_executable(test_config test/test_config.cpp)
target_link_libraries(test_config PRIVATE xpilot_common gtest_main)

# 测试3：协议拆包
add_executable(test_protocol test/test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE xpilot_common gtest_main)

# 注册到 ctest
include(GoogleTest)
gtest_discover_tests(test_logger)
gtest_discover_tests(test_config)
gtest_discover_tests(test_protocol)