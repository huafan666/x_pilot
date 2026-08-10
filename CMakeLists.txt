cmake_minimum_required(VERSION 3.10)

# 头文件检索路径
include_directories(${CMAKE_SOURCE_DIR}/include)

# 设置项目名称
project(x_pilot)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)

# 添加可执行文件
# 语法：add_executable(生成可执行的文件 源代码文件路径)
add_executable(x_pilot src/main.cpp)