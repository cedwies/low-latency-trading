# Create simulator executable
add_executable(simulator simulator.cpp)
target_link_libraries(simulator PRIVATE trading_system)

find_package(Threads REQUIRED)
target_link_libraries(simulator PRIVATE Threads::Threads)