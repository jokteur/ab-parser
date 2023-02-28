cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 17)

# Set project name
project(AB-PARSER)

include_directories(src)
file(GLOB source_list RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    src/*.cpp
)

# Making a library without the main, for the tests and the program
add_library(${PROJECT_NAME} ${source_list})

# ~~~~~~~
# Testing
# ~~~~~~~
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

# -------
# DocTest
# -------
add_subdirectory(external/doctest)
add_subdirectory(tests)
endif()