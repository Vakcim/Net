cmake_minimum_required(VERSION 3.16)
project(hybrid_temporal_index VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(temporal_index src/temporal_index.cpp src/synthetic.cpp)
target_include_directories(temporal_index PUBLIC include)
target_compile_options(temporal_index PRIVATE -Wall -Wextra -Wpedantic)

if(ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(temporal_index PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(temporal_index PUBLIC -fsanitize=address,undefined)
endif()

add_executable(temporal_index_cli apps/temporal_index_cli.cpp)
target_link_libraries(temporal_index_cli PRIVATE temporal_index)
target_compile_options(temporal_index_cli PRIVATE -Wall -Wextra -Wpedantic)

add_executable(temporal_benchmark benchmarks/benchmark.cpp)
target_link_libraries(temporal_benchmark PRIVATE temporal_index)
target_compile_options(temporal_benchmark PRIVATE -O3 -Wall -Wextra -Wpedantic)

add_executable(temporal_profile benchmarks/profile.cpp)
target_link_libraries(temporal_profile PRIVATE temporal_index)
target_compile_options(temporal_profile PRIVATE -O3 -Wall -Wextra -Wpedantic)

enable_testing()
add_executable(temporal_index_tests tests/test_temporal_index.cpp)
target_link_libraries(temporal_index_tests PRIVATE temporal_index)
target_compile_options(temporal_index_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME temporal_index_tests COMMAND temporal_index_tests)
