# Defines AppBase library target.
cmake_minimum_required( VERSION 3.5 )
project( AppBase )

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

include( GNUInstallDirs )

file(GLOB HEADERS "include/appbase/*.hpp")

set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

if(CMAKE_CXX_STANDARD EQUAL 98 OR CMAKE_CXX_STANDARD LESS 17)
   message(FATAL_ERROR "appbase requires c++17 or newer")
elseif(NOT CMAKE_CXX_STANDARD)
   set(CMAKE_CXX_STANDARD 17)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)

if(NOT TARGET Boost::program_options)
   find_package(Boost 1.67 REQUIRED COMPONENTS program_options system)
endif()

if( APPLE )
  # Apple Specific Options Here
  message( STATUS "Configuring AppBase on OS X" )
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-conversion -Wno-deprecated-declarations" )
else( APPLE )
  # Linux Specific Options Here
  message( STATUS "Configuring AppBase on Linux" )
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall" )
  if ( FULL_STATIC_BUILD )
    set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
  endif ( FULL_STATIC_BUILD )
endif( APPLE )


if(ENABLE_COVERAGE_TESTING)
    SET(CMAKE_CXX_FLAGS "--coverage ${CMAKE_CXX_FLAGS}")
endif()

add_library( appbase
             application_base.cpp
             ${HEADERS}
           )

target_link_libraries( appbase PUBLIC Boost::program_options Boost::system Threads::Threads)

if(TARGET Boost::asio)
   target_link_libraries( appbase PUBLIC Boost::asio Boost::signals2 )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries( appbase PUBLIC stdc++fs )
  endif()
endif()

target_include_directories( appbase
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

set_target_properties( appbase PROPERTIES PUBLIC_HEADER "${HEADERS}" )

option(APPBASE_ENABLE_AUTO_VERSION "enable automatic discovery of version via 'git describe'" ON)

find_package(Git)
if(APPBASE_ENABLE_AUTO_VERSION AND EXISTS ${CMAKE_SOURCE_DIR}/.git AND GIT_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/version.cmake @ONLY)
  add_custom_target(appbase_version ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/version.cmake BYPRODUCTS version.cpp)
  add_dependencies(appbase appbase_version)
else()
  set(VERSION_STRING "Unknown")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/version.cpp @ONLY ESCAPE_QUOTES)
endif()
target_sources(appbase PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)

set(CPACK_PACKAGING_INSTALL_PREFIX /)

if(APPBASE_INSTALL_COMPONENT)
   set(INSTALL_COMPONENT_ARGS COMPONENT ${APPBASE_INSTALL_COMPONENT} EXCLUDE_FROM_ALL)
endif()

install( TARGETS
   appbase

   RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR} ${INSTALL_COMPONENT_ARGS}
   LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR} ${INSTALL_COMPONENT_ARGS}
   ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR} ${INSTALL_COMPONENT_ARGS}
   PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}/appbase ${INSTALL_COMPONENT_ARGS}
)

add_subdirectory( examples )
enable_testing()
add_subdirectory( tests )
