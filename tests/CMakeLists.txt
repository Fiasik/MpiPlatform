add_subdirectory(unit)
add_test(NAME ArchitectureLayerIncludes
    COMMAND ${CMAKE_COMMAND} -DPROJECT_ROOT=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/architecture/CheckLayerIncludes.cmake)
