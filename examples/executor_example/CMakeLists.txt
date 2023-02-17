add_executable( executor_example main.cpp )
target_link_libraries( executor_example appbase ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
