cmake_minimum_required(VERSION 3.20)
project(low_latency_trading VERSION 1.0.0 LANGUAGES CXX)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # High optimization level with LTO
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto -march=native")
    # Extra warnings and strict compilation
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
elseif(MSVC)
    # High optimization level
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG /GL")
    # Warning level 4
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Library sources
file(GLOB_RECURSE SOURCES 
    "src/core/*.cpp"
    "src/utils/*.cpp"
    "src/support/*.cpp"
)

# Create library
add_library(trading_system ${SOURCES})

# Add examples subdirectory
add_subdirectory(examples)

# Add benchmark subdirectory
add_subdirectory(benchmark)