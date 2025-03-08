# Create benchmark executable
add_executable(latency_benchmark latency_benchmark.cpp)
target_link_libraries(latency_benchmark PRIVATE trading_system)

# If pthreads is required
find_package(Threads REQUIRED)
target_link_libraries(latency_benchmark PRIVATE Threads::Threads)