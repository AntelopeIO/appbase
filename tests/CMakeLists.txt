add_executable( basic_test basic_test.cpp )
target_link_libraries( basic_test appbase ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
