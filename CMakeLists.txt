cmake_minimum_required(VERSION 3.6)
project(redbase)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -Wno-deprecated-register")

find_package(FLEX)
find_package(BISON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

FLEX_TARGET(Lexer src/scan.l ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)
BISON_TARGET(Parser src/parse.y ${CMAKE_CURRENT_BINARY_DIR}/y.tab.cc COMPILE_FLAGS -vd)
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

# Kiprey: Wrong CMake config maybe.

# file(GLOB C_SOURCE_FILES
#         "src/*.h"
#         "src/*.c"
#         )
# file(GLOB CPP_SOURCE_FILES
#         "src/*.h"
#         "src/*.hh"
#         "src/*.cc"
#         "src/*.c"
#         )
# set(SOURCE_FILES
#         ${C_SOURCE_FILES}
#         ${CPP_SOURCE_FILES}
#         ${FLEX_Lexer_OUTPUTS}
#         ${BISON_Parser_OUTPUTS}
#         )
# add_executable(redbase ${SOURCE_FILES})


# add_definitions("-DPF_STATS")     # 统计 PF 状态
# add_definitions("-DPF_LOG")       # 启动 PF 日志 功能

IF (NOT ASAN_ENABLE MATCHES "False")
    MESSAGE(STATUS "ASAN is enabled")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
ENDIF ()

file(GLOB PF_SOURCE_FILES
    "src/pf_buffermgr.*"
    "src/pf_error.cc"
    "src/pf_filehandle.cc"
    "src/pf_hashtable.*"
    "src/pf_internal.h"
    "src/pf_manager.cc"
    "src/pf_pagehandle.cc"
    "src/pf_statistics.cc"
    "src/pf.h"
    )

file(GLOB STATS_SOURCE_FILES "src/statistics.*")

add_executable(pf_test1 src/pf_test1.cpp ${PF_SOURCE_FILES})

add_executable(pf_test2 "src/pf_test2.cpp" "src/statistics.cc" ${PF_SOURCE_FILES})
target_compile_definitions(pf_test2 PUBLIC "-DPF_STATS" "-DPF_")

add_executable(pf_test3 src/pf_test3.cpp ${PF_SOURCE_FILES})