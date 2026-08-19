@echo off
echo Building Stock Exchange Matching Engine (C++20)...
g++ -std=c++20 -O3 -Isrc src/demo_main.cpp src/common/time_utils.cpp src/logger/logger.cpp src/threadpool/thread_pool.cpp src/registry/book_registry.cpp src/engine/order_book.cpp src/engine/matching_engine.cpp src/engine/replay_engine.cpp src/engine/risk_checker.cpp src/pipeline/wal.cpp src/pipeline/execution_pipeline.cpp -lws2_32 -lmswsock -o exchange_demo.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo =========================================
    echo   Build Successful! Executing Demo...
    echo =========================================
    echo.
    .\exchange_demo.exe
) else (
    echo Build failed. Please check compiler errors above.
)
