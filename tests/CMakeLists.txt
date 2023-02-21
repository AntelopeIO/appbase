file(GLOB UNIT_TESTS "*.cpp")
add_executable( appbase_test ${UNIT_TESTS} )
target_link_libraries( appbase_test appbase ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

add_test( appbase_test appbase_test )
