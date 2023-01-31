add_executable(${PROJECT_NAME}_tests main.cpp)
target_link_libraries(${PROJECT_NAME}_tests doctest::doctest ${PROJECT_NAME})

add_custom_command(TARGET ${PROJECT_NAME}_tests PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/tests/files/ $<TARGET_FILE_DIR:${PROJECT_NAME}_tests>)