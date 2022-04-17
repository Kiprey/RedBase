cmake_minimum_required(VERSION 3.6)
project(redbase)

################ Origin Flags ################

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -Wno-deprecated-register")

################ Parser ################

find_package(FLEX)
find_package(BISON)

FLEX_TARGET(Lexer src/scan.l ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)
BISON_TARGET(Parser src/parse.y ${CMAKE_CURRENT_BINARY_DIR}/y.tab.cc COMPILE_FLAGS -vd)
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

################ Definitions ################

# add_definitions("-DPF_STATS")     # 统计 PF 状态
# add_definitions("-DPF_LOG")       # 启动 PF 日志 功能

################ Address Sanitizer ################

IF (NOT ASAN_ENABLE MATCHES "False")
    MESSAGE(STATUS "ASAN is enabled")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
ENDIF ()

################ Include Directories & Source Files ################

include_directories("src/")
include_directories("src/pf")
include_directories("src/rm")
include_directories("src/utils")

file(GLOB PF_SOURCE_FILES       "src/pf/*" )
file(GLOB UTILS_SOURCE_FILES    "src/utils/*")
file(GLOB RM_SOURCE_FILES       "src/rm/*")
file(GLOB IX_SOURCE_FILES       "src/ix/*")

################ Page File Test ################

add_executable(pf_test1 "src/test/pf_test1.cpp" ${PF_SOURCE_FILES})

add_executable(pf_test2 "src/test/pf_test2.cpp" ${UTILS_SOURCE_FILES} ${PF_SOURCE_FILES})
target_compile_definitions(pf_test2 PUBLIC "-DPF_STATS" "-DPF_")

add_executable(pf_test3 "src/test/pf_test3.cpp" ${PF_SOURCE_FILES})

################ Record Management Test ################

add_executable(rm_test "src/test/rm_test.cpp" ${PF_SOURCE_FILES} ${RM_SOURCE_FILES})

################ Idexing Test ################

add_executable(ix_test "src/test/ix_test.cpp" ${PF_SOURCE_FILES} ${IX_SOURCE_FILES})

