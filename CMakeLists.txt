cmake_minimum_required(VERSION 3.15)
project(StockExchangeMatchingEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O3 -pthread)
endif()

# Find Threads library
find_package(Threads REQUIRED)

# Common Source files
set(COMMON_SOURCES
    src/common/time_utils.cpp
    src/logger/logger.cpp
    src/threadpool/thread_pool.cpp
    src/registry/book_registry.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/engine/replay_engine.cpp
    src/engine/risk_checker.cpp
    src/pipeline/wal.cpp
    src/pipeline/execution_pipeline.cpp
)

add_compile_definitions(ASIO_STANDALONE _WIN32_WINNT=0x0A00)

# Target: exchange_demo (Primary Interview Demo & Correctness Tests)
add_executable(exchange_demo src/demo_main.cpp ${COMMON_SOURCES})
target_include_directories(exchange_demo PRIVATE src)

# Link threads and winsock libraries
if(WIN32)
    target_link_libraries(exchange_demo PRIVATE Threads::Threads ws2_32 mswsock)
else()
    target_link_libraries(exchange_demo PRIVATE Threads::Threads)
endif()
