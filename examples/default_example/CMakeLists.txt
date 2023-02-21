add_executable( default_example main.cpp )
target_link_libraries( default_example appbase ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
