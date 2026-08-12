cmake_minimum_required(VERSION 3.10)

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
)

# 2. 通信进程 (重要修改：增加了 tcp_client.cpp 和 ipc.cpp)
add_executable(comm_process 
    src/comm_main.cpp 
    src/utils.cpp
    src/config.cpp
    src/ipc.cpp           # 新增：Comm 需要连接 IPC
    src/tcp_client.cpp    # 新增：Comm 需要连接 TCP
)

# 3. 控制进程
add_executable(control_process 
    src/control_main.cpp 
    src/utils.cpp
    src/config.cpp
    src/ipc.cpp      
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
