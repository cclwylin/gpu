add_executable(scene_runner scene_runner.cpp)
target_link_libraries(scene_runner PRIVATE gpu_sw_ref)

# Walk the tests/scenes/ tree and add one CTest per .scene file.
file(GLOB SCENE_FILES "${CMAKE_SOURCE_DIR}/tests/scenes/*.scene")
foreach(scene_path ${SCENE_FILES})
    get_filename_component(scene_name ${scene_path} NAME_WE)
    add_test(NAME conformance.${scene_name}
             COMMAND scene_runner ${scene_path})
endforeach()
