cmake_minimum_required(VERSION 3.16)
project(minioj_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQLCLIENT REQUIRED IMPORTED_TARGET mysqlclient)
find_package(Threads REQUIRED)

add_library(minioj_core
    src/util/config.cpp
    src/util/logger.cpp
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
