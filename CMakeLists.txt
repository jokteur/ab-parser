cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 17)

# Set project name
project(AB-PARSER)

include_directories(src)
file(GLOB source_list RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    src/*.cpp
)

# ~~~~~~~
# Testing
# ~~~~~~~
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    # Making a library without the main, for the tests and the program
    add_library(${PROJECT_NAME} ${source_list})

    target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}/include")

    # -------
    # DocTest
    # -------
    message("Testing activated")
    add_subdirectory(external/doctest)
    option(TRACY_ENABLE "" ON)

    option(TRACY_ON_DEMAND "" OFF)
    add_subdirectory(external/tracy)
    add_subdirectory(tests)

    target_link_libraries(${PROJECT_NAME} PUBLIC TracyClient)
else()
    # Making a library without the main, for the tests and the program
    add_library(${PROJECT_NAME} ${source_list})

    target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}/include")
endif()
