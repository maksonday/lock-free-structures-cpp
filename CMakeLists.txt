cmake_minimum_required(VERSION 3.14)
project(LockFree VERSION 1.0.0 LANGUAGES CXX)

add_library(lock_free INTERFACE)

target_include_directories(lock_free INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(lock_free INTERFACE cxx_std_20)

option(BUILD_TESTS "Build lock-free library tests" ON)

if(BUILD_TESTS)
    enable_testing()

    add_executable(lock_free_tests
        tests/main.cpp
        tests/unit/queue_test.cpp
        tests/unit/stack_test.cpp
    )

    target_include_directories(lock_free_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    target_link_libraries(lock_free_tests PRIVATE lock_free)
    set_target_properties(lock_free_tests PROPERTIES CXX_STANDARD 20)

    add_test(NAME lock_free_tests COMMAND lock_free_tests)
endif()

option(ENABLE_ASAN "Build tests with AddressSanitizer" OFF)

if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lock_free_tests PRIVATE -fsanitize=address -fno-omit-frame-pointer -O1 -g)
        target_link_options(lock_free_tests PRIVATE -fsanitize=address)
    else()
        message(WARNING "AddressSanitizer enabled, but compiler does not support it.")
    endif()
endif()
