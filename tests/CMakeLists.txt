add_executable( appbase_basic_test basic_test.cpp )
target_link_libraries( appbase_basic_test appbase ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
add_test( appbase_basic_test appbase_basic_test )

# Different executors must run in different executibles
add_executable( appbase_executor_test executor_test.cpp )
target_link_libraries( appbase_executor_test appbase ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
add_test( appbase_executor_test appbase_executor_test )
