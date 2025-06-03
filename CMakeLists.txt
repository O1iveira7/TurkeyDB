cmake_minimum_required(VERSION 3.28)
project(TurkeyDBProject)

set(CMAKE_CXX_STANDARD 20)

# 定义静态库 TurkeyDB
add_library(TurkeyDB STATIC
        include/block.h
        include/block_cache.h
        include/block_iterator.h
        include/blockmeta.h
        include/bloom_filter.h
        include/compact.h
        include/concact_iterator.h
        include/consts.h
        include/db.h
        include/engine.h
        include/files.h
        include/iterator.h
        include/mem_table.h
        include/memtable.h
        include/mmap_file.h
        include/record.h
        include/skiplist.h
        include/sst.h
        include/sst_iterator.h
        include/std_file.h
        include/transaction.h
        include/two_merge_iterator.h
        include/util.h
        include/wal.h
        src/block.cpp
        src/block_cache.cpp
        src/block_iterator.cpp
        src/blockmeta.cpp
        src/bloom_filter.cpp
        src/concact_iterator.cpp
        src/db_impl.h
        src/db_impl.cpp
        src/engine.cpp
        src/file.h
        src/file.cpp
        src/files.cpp
        src/iterator.cpp
        src/mem_table_impl.h
        src/mem_table_impl.cpp
        src/memtable.cpp
        src/mmap_file.cpp
        src/record.cpp
        src/skipList.cpp
        src/sst.cpp
        src/sst_iterator.cpp
        src/std_file.cpp
        src/transation.cpp
        src/two_merge_iterator.cpp
        src/wal.cpp
)

# 设置 TurkeyDB 库的头文件搜索路径
target_include_directories(TurkeyDB PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

# 创建可执行文件
add_executable(TurkeyDBExecutable app/main.cpp
        )

# 将 TurkeyDB 静态库链接到可执行文件
target_link_libraries(TurkeyDBExecutable PRIVATE TurkeyDB)

# 设置bench
add_executable(DBBench
        bench/bench.cpp)
# 将 TurkeyDB 静态库链接到可执行文件
target_link_libraries(DBBench PRIVATE TurkeyDB)


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
        test/test_skiplist.cpp
        test/test_block.cpp
        test/test_sstable.cpp
        test/test_memtable.cpp
)

target_link_libraries(
        TEST_DB
        GTest::gtest_main
        TurkeyDB
)

include(GoogleTest)
gtest_discover_tests(TEST_DB)
