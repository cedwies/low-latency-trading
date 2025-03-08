# Create simulator executable
add_executable(simulator simulator.cpp)
target_link_libraries(simulator PRIVATE trading_system)

# If pthreads is required
find_package(Threads REQUIRED)
target_link_libraries(simulator PRIVATE Threads::Threads)