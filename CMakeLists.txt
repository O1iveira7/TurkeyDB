cmake_minimum_required(VERSION 3.28)
project(TurkeyDBProject)

set(CMAKE_CXX_STANDARD 20)

# 定义静态库 TurkeyDB
add_library(TurkeyDB STATIC
        src/
        src/db_impl.cpp
        src/db_impl.h
        include/mem_table.h
        include/util.h
        include/iterator.h
        src/mem_table_impl.cpp
        src/mem_table_impl.h
)

# 设置 TurkeyDB 库的头文件搜索路径
target_include_directories(TurkeyDB PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

# 创建可执行文件
add_executable(TurkeyDBExecutable app/main.cpp)

# 将 TurkeyDB 静态库链接到可执行文件
target_link_libraries(TurkeyDBExecutable PRIVATE TurkeyDB)


include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
        TEST_DB
        test/util_test.cpp
        test/mem_table_test.cpp
)
target_link_libraries(
        TEST_DB
        GTest::gtest_main
        TurkeyDB
)

include(GoogleTest)
gtest_discover_tests(TEST_DB)
