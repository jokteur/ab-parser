add_executable(${PROJECT_NAME}_tests main.cpp t_testcases.cpp)
target_link_libraries(${PROJECT_NAME}_tests doctest::doctest ${PROJECT_NAME})

add_custom_command(TARGET ${PROJECT_NAME}_tests PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/tests/files/ $<TARGET_FILE_DIR:${PROJECT_NAME}_tests>)

# Benchmarking
add_executable(${PROJECT_NAME}_bench benchmark.cpp)
target_link_libraries(${PROJECT_NAME}_bench PUBLIC doctest::doctest ${PROJECT_NAME})

add_custom_command(TARGET ${PROJECT_NAME}_bench PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/tests/files/ $<TARGET_FILE_DIR:${PROJECT_NAME}_tests>)

# Profiling
add_executable(${PROJECT_NAME}_prof profiling.cpp)
target_link_libraries(${PROJECT_NAME}_prof PUBLIC doctest::doctest ${PROJECT_NAME})

add_custom_command(TARGET ${PROJECT_NAME}_bench PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/tests/files/ $<TARGET_FILE_DIR:${PROJECT_NAME}_tests>)