cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(x_pilot)

# 设置C++标准
set(CMAKE_CXX_STANDARD 14)

# 头文件检索路径
include_directories(${CMAKE_SOURCE_DIR}/include)

# 1. 守护进程
# 需要加载配置，所以加上了 config.cpp
add_executable(manager 
    src/manager_main.cpp 
    src/utils.cpp
    src/config.cpp      
)

# 2. 通信进程
# 需要读取 IP/端口/心跳配置，所以加上 config.cpp
add_executable(comm_process 
    src/comm_main.cpp 
    src/utils.cpp
    src/config.cpp      
)

# 3. 控制进程
# 需要读取共享内存路径等配置，所以加上 config.cpp
add_executable(control_process 
    src/control_main.cpp 
    src/utils.cpp
    src/config.cpp      
)

# 4. 数据可视进程
# 如果它也需要读配置，就加上；如果只是纯查看共享内存，可以不加。建议先加上。
add_executable(view_process 
    src/view_main.cpp 
    src/utils.cpp
    src/config.cpp   
)
