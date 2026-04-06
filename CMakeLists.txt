cmake_minimum_required(VERSION 3.20)
project(market_data_feed CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Release flags — required to hit latency targets
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# Sanitizer presets (use with -DCMAKE_BUILD_TYPE=Sanitize)
set(CMAKE_CXX_FLAGS_SANITIZE "-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS_SANITIZE   "-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer")

# ThreadSanitizer preset (use with -DCMAKE_BUILD_TYPE=TSan)
set(CMAKE_CXX_FLAGS_TSAN "-O1 -g -fsanitize=thread -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS_TSAN   "-O1 -g -fsanitize=thread -fno-omit-frame-pointer")

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(bench)
add_subdirectory(tools)
