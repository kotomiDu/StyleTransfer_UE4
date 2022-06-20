# CMakeList.txt : CMake project for OpenVinoLibrary, include source and define project specific logic here.
cmake_minimum_required (VERSION 3.8)

project ( "OpenVinoWrapper" )

set (CMAKE_VERBOSE_MAKEFILE ON)
set (TARGET_NAME "OpenVinoWrapper")

include_directories(${PROJECT_BINARY_DIR}/../openvino/include)
include_directories(${PROJECT_BINARY_DIR}/../openvino/include/ie)
include_directories(${PROJECT_BINARY_DIR}/../opencv/include)
include_directories(${PROJECT_BINARY_DIR}/../ocl/cl_headers)
include_directories(${PROJECT_BINARY_DIR}/../ocl/clhpp_headers/include)

link_directories(${LIB_FOLDER})
LINK_DIRECTORIES(${PROJECT_BINARY_DIR}/../opencv/lib)
LINK_DIRECTORIES(${PROJECT_BINARY_DIR}/../openvino/lib/intel64/lib_release)
LINK_DIRECTORIES(${PROJECT_BINARY_DIR}/../openvino/lib/intel64/lib_debug)


# Add source to this project's executable.
add_library(${TARGET_NAME} SHARED "OpenVinoWrapper.cpp" "OpenVinoWrapper.h" "OpenVinoData.cpp" "OpenVinoData.h"  "OpenCLUtil.cpp" "OpenCLUtil.h")

if(WIN32)
	set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_FLAGS "/D_UNONICODE /DUNONICODE /DOPEN_VINO_LIBRARY")
endif()

set_target_properties(${TARGET_NAME} PROPERTIES "CMAKE_CXX_FLAGS" "${CMAKE_CXX_FLAGS} -fPIE" COMPILE_PDB_NAME ${TARGET_NAME})

TARGET_LINK_LIBRARIES(${TARGET_NAME} 
debug opencv_imgproc454d.lib opencv_core454d.lib opencv_imgcodecs454d.lib opencv_video454d.lib opencv_videoio454d.lib
optimized opencv_imgproc454.lib opencv_core454.lib opencv_imgcodecs454.lib opencv_video454.lib opencv_videoio454.lib)
TARGET_LINK_LIBRARIES(${TARGET_NAME} 
debug openvino_ir_frontendd.lib openvinod.lib OpenCL.lib
optimized openvino_ir_frontend.lib openvino.lib OpenCL.lib)


# # Copy dll to target folder
add_custom_command(
        TARGET ${PROJECT_NAME}  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${PROJECT_BINARY_DIR}/../opencv/bin/dll_release
                $<TARGET_FILE_DIR:${TARGET_NAME}>)

add_custom_command(
        TARGET ${PROJECT_NAME}  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${PROJECT_BINARY_DIR}/../openvino/bin/intel64/dll_release
                $<TARGET_FILE_DIR:${TARGET_NAME}>)

add_custom_command(
        TARGET ${PROJECT_NAME}  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${PROJECT_BINARY_DIR}/../opencv/bin/dll_debug
                $<TARGET_FILE_DIR:${TARGET_NAME}>)

add_custom_command(
        TARGET ${PROJECT_NAME}  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${PROJECT_BINARY_DIR}/../openvino/bin/intel64/dll_debug
                $<TARGET_FILE_DIR:${TARGET_NAME}>)
#message(STATUS "hello ${OpenCV_INCLUDE_DIRS}")