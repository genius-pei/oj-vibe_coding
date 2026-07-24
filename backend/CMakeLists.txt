cmake_minimum_required(VERSION 3.16)
project(minioj_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQLCLIENT REQUIRED IMPORTED_TARGET mysqlclient)
find_package(Threads REQUIRED)

option(MINIOJ_BUILD_TESTS "Build unit tests" ON)
if(MINIOJ_BUILD_TESTS)
    find_package(GTest REQUIRED)
    include(GoogleTest)
    enable_testing()
endif()

add_library(minioj_core
    src/util/config.cpp
    src/util/logger.cpp
    src/util/types.cpp
    src/db/pool.cpp
)

target_include_directories(minioj_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(minioj_core PUBLIC
    PkgConfig::MYSQLCLIENT
    Threads::Threads
)

target_compile_options(minioj_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(minioj-backend src/main.cpp)
target_link_libraries(minioj-backend PRIVATE minioj_core)

if(MINIOJ_BUILD_TESTS)
    add_executable(minioj_test_config tests/test_config.cpp)
    target_link_libraries(minioj_test_config PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_config)

    add_executable(minioj_test_logger tests/test_logger.cpp)
    target_link_libraries(minioj_test_logger PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_logger)

    add_executable(minioj_test_types tests/test_types.cpp)
    target_link_libraries(minioj_test_types PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_types)
endif()
