cmake_minimum_required(VERSION 3.16)
project(minioj_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQLCLIENT REQUIRED IMPORTED_TARGET mysqlclient)
pkg_check_modules(JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)
find_package(Threads REQUIRED)

find_library(CRYPT_LIB NAMES crypt libcrypt REQUIRED)

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
    src/db/problem_dao.cpp
    src/db/user_dao.cpp
    src/judge/diff.cpp
    src/judge/worker_pool.cpp
    src/judge/compiler.cpp
    src/judge/runner.cpp
    src/judge/pipeline.cpp
    src/auth/password.cpp
    src/auth/session.cpp
    src/auth/validator.cpp
    src/http/problem_dto.cpp
    src/http/admin_dto.cpp
    src/http/admin_request.cpp
    src/http/submission_dto.cpp
    src/http/submission_request.cpp
    src/http/middleware.cpp
    src/http/handlers_public.cpp
    src/http/handlers_admin.cpp
    src/http/handlers_auth.cpp
    src/http/router.cpp
)

target_include_directories(minioj_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(minioj_core PUBLIC
    PkgConfig::MYSQLCLIENT
    PkgConfig::JSONCPP
    Threads::Threads
    ${CRYPT_LIB}
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

    add_executable(minioj_test_problem_dto tests/test_problem_dto.cpp)
    target_link_libraries(minioj_test_problem_dto PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_problem_dto)

    add_executable(minioj_test_admin_request tests/test_admin_request.cpp)
    target_link_libraries(minioj_test_admin_request PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_admin_request)

    add_executable(minioj_test_diff tests/test_diff.cpp)
    target_link_libraries(minioj_test_diff PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_diff)

    add_executable(minioj_test_worker_pool tests/test_worker_pool.cpp)
    target_link_libraries(minioj_test_worker_pool PRIVATE minioj_core GTest::gtest GTest::gtest_main Threads::Threads)
    gtest_discover_tests(minioj_test_worker_pool)

    add_executable(minioj_test_compiler tests/test_compiler.cpp)
    target_link_libraries(minioj_test_compiler PRIVATE minioj_core GTest::gtest GTest::gtest_main Threads::Threads)
    gtest_discover_tests(minioj_test_compiler)

    add_executable(minioj_test_runner tests/test_runner.cpp)
    target_link_libraries(minioj_test_runner PRIVATE minioj_core GTest::gtest GTest::gtest_main Threads::Threads)
    gtest_discover_tests(minioj_test_runner)

    add_executable(minioj_test_submission_dto tests/test_submission_dto.cpp)
    target_link_libraries(minioj_test_submission_dto PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_submission_dto)

    add_executable(minioj_test_pipeline tests/test_pipeline.cpp)
    target_link_libraries(minioj_test_pipeline PRIVATE minioj_core GTest::gtest GTest::gtest_main Threads::Threads)
    gtest_discover_tests(minioj_test_pipeline)

    add_executable(minioj_test_submission_request tests/test_submission_request.cpp)
    target_link_libraries(minioj_test_submission_request PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_submission_request)

    add_executable(minioj_test_session tests/test_session.cpp)
    target_link_libraries(minioj_test_session PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_session)

    add_executable(minioj_test_user_dao tests/test_user_dao.cpp)
    target_link_libraries(minioj_test_user_dao PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_user_dao)

    add_executable(minioj_test_validator tests/test_validator.cpp)
    target_link_libraries(minioj_test_validator PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_validator)

    add_executable(minioj_test_password tests/test_password.cpp)
    target_link_libraries(minioj_test_password PRIVATE minioj_core GTest::gtest GTest::gtest_main)
    gtest_discover_tests(minioj_test_password)
endif()
