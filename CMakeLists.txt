cmake_minimum_required(VERSION 3.28)
project(TurkeyDBProject)

set(CMAKE_CXX_STANDARD 20)

# 定义静态库 TurkeyDB
add_library(TurkeyDB STATIC
        src/
        src/db_impl.cpp
        src/db_impl.h
)

# 设置 TurkeyDB 库的头文件搜索路径
target_include_directories(TurkeyDB PUBLIC ${CMAKE_SOURCE_DIR}/include)

# 创建可执行文件
add_executable(TurkeyDBExecutable app/main.cpp)

# 将 TurkeyDB 静态库链接到可执行文件
target_link_libraries(TurkeyDBExecutable PRIVATE TurkeyDB)
