cmake_minimum_required(VERSION 3.10)
project(xtree LANGUAGES CXX)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_GIT "Enable git-related features (git status, author/date)" ON)

if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

set(SOURCES
  ${CMAKE_SOURCE_DIR}/main.cpp
  ${CMAKE_SOURCE_DIR}/src/xtree_tools/printer.cpp
  ${CMAKE_SOURCE_DIR}/src/xtree_tools/utils.cpp
)

if (ENABLE_GIT)
  list(APPEND SOURCES ${CMAKE_SOURCE_DIR}/src/xtree_tools/git.cpp)
  add_compile_definitions(HAVE_GIT=1)
else()
  list(APPEND SOURCES ${CMAKE_SOURCE_DIR}/src/xtree_tools/git_stub.cpp)
endif()

add_executable(xtree ${SOURCES})

install(TARGETS xtree RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)

include(CTest)
enable_testing()
add_test(NAME smoke_test COMMAND $<TARGET_FILE:xtree> --help)

if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

include(CPack)
set(CPACK_PACKAGE_NAME "xtree")
set(CPACK_PACKAGE_VENDOR "xtree")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "xtree - directory tree with git info")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "you@example.com")

