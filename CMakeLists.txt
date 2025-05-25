cmake_minimum_required(VERSION 3.11)
project(mpack_cpp VERSION 0.1.0 LANGUAGES C CXX)

###############################################################################
# Project configuration
###############################################################################
cmake_policy(SET CMP0135 NEW)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)
endif()

option(MPACK_CPP_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

###############################################################################
# Download and define mpack as a shared library target. 
###############################################################################
include(FetchContent)
fetchcontent_declare(
  mpack
  URL https://github.com/ludocode/mpack/releases/download/v1.1.1/mpack-amalgamation-1.1.1.tar.gz
  URL_HASH MD5=10e7377a58e033f645386ee3ffd0879a
)
fetchcontent_makeavailable(mpack)
add_library(mpack STATIC  ${mpack_SOURCE_DIR}/src/mpack/mpack.c)
set_target_properties(mpack PROPERTIES LANGUAGE CXX)
set_target_properties(mpack PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(mpack PUBLIC
  $<BUILD_INTERFACE:${mpack_SOURCE_DIR}/src/mpack>
  $<INSTALL_INTERFACE:include/mpack>
)
target_compile_definitions(mpack PUBLIC
  MPACK_EXTENSIONS=1
  MPACK_STDIO=1
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(mpack PUBLIC MPACK_DEBUG=1)
endif()

FetchContent_Declare(
  boost_preprocessor
  GIT_REPOSITORY https://github.com/boostorg/preprocessor.git 
)
FetchContent_MakeAvailable(boost_preprocessor)

###############################################################################
# Define mpack_cpp as a header only library. 
###############################################################################
add_library(mpack_cpp INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mpack_cpp INTERFACE mpack Boost::preprocessor)

###############################################################################
# Demos
###############################################################################
add_executable(node_api demos/node_api.cpp)
target_link_libraries(node_api PRIVATE mpack_cpp)

###############################################################################
# Tests 
###############################################################################
if(MPACK_CPP_BUILD_TESTS)
    # https://google.github.io/googletest/quickstart-cmake.html
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)



    include(CTest)
    enable_testing()

    add_executable(
        test_mpack_cpp 
        tests/test_mpack_cpp.cpp
        tests/test_expect_reader.cpp
        tests/test_nested_struct.cpp
        tests/test_custom_allocator.cpp
        tests/test_optional.cpp
        tests/test_ext_type.cpp
    )
    target_link_libraries(
        test_mpack_cpp
        mpack_cpp
        GTest::gtest_main
        GTest::gmock
    )

    include(GoogleTest)
    gtest_discover_tests(test_mpack_cpp)
endif()

