# CMakeList.txt : CMake project for OpenVinoLibrary, include source and define project specific logic here.
cmake_minimum_required (VERSION 3.8)

project ( "OpenVinoWrapper" )

set (CMAKE_VERBOSE_MAKEFILE ON)
set (TARGET_NAME "OpenVinoWrapper")

include_directories(${PROJECT_BINARY_DIR}/../openvino/include)
include_directories(${PROJECT_BINARY_DIR}/../opencv/include)

link_directories(${LIB_FOLDER})
LINK_DIRECTORIES(${PROJECT_BINARY_DIR}/../opencv/lib)
LINK_DIRECTORIES(${PROJECT_BINARY_DIR}/../openvino/lib/intel64/lib_release)


# Add source to this project's executable.
add_library(${TARGET_NAME} SHARED "OpenVinoWrapper.cpp" "OpenVinoWrapper.h" "OpenVinoData.cpp" "OpenVinoData.h")

if(WIN32)
	set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_FLAGS "/D_UNONICODE /DUNONICODE /DOPEN_VINO_LIBRARY")
endif()

set_target_properties(${TARGET_NAME} PROPERTIES "CMAKE_CXX_FLAGS" "${CMAKE_CXX_FLAGS} -fPIE" COMPILE_PDB_NAME ${TARGET_NAME})

TARGET_LINK_LIBRARIES(${TARGET_NAME} opencv_imgproc454.lib opencv_core454.lib opencv_imgcodecs454.lib opencv_video454.lib opencv_videoio454.lib)
TARGET_LINK_LIBRARIES(${TARGET_NAME} openvino_ir_frontend.lib openvino.lib OpenCL.lib)





#message(STATUS "hello ${OpenCV_INCLUDE_DIRS}")