# CMakeList.txt : CMake project for OpenVinoLibrary, include source and define project specific logic here.
cmake_minimum_required (VERSION 3.8)

project ( "OpenVinoWrapper" )

set (CMAKE_VERBOSE_MAKEFILE ON)
set (TARGET_NAME "OpenVinoWrapper")

find_package(InferenceEngine REQUIRED)
find_package(OpenCV COMPONENTS imgcodecs REQUIRED)

include_directories(${InferenceEngine_INCLUDE_DIRS})
include_directories(${OpenCV_INCLUDE_DIRS})

link_directories(${LIB_FOLDER})

# Add source to this project's executable.
add_library(${TARGET_NAME} SHARED "OpenVinoWrapper.cpp" "OpenVinoWrapper.h" "OpenVinoData.cpp" "OpenVinoData.h" "ClassificationResult.h")

if(WIN32)
	set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_FLAGS "/D_UNONICODE /DUNONICODE /DOPEN_VINO_LIBRARY")
endif()

set_target_properties(${TARGET_NAME} PROPERTIES "CMAKE_CXX_FLAGS" "${CMAKE_CXX_FLAGS} -fPIE" COMPILE_PDB_NAME ${TARGET_NAME})

target_link_libraries(${TARGET_NAME} ${InferenceEngine_LIBRARIES} ${OpenCV_LIBS})


get_filename_component(IE_BIN_PATH "${InferenceEngine_INCLUDE_DIRS}/../bin/intel64/Release" ABSOLUTE)
file(GLOB_RECURSE IE_BIN_FILES ${IE_BIN_PATH}/*.*)
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/inference_engine)
file(COPY ${IE_BIN_FILES} DESTINATION ${PROJECT_BINARY_DIR}/inference_engine)

get_filename_component(TBB_BIN_PATH "${InferenceEngine_INCLUDE_DIRS}/../external/tbb/bin" ABSOLUTE)
file(GLOB_RECURSE TBB_BIN_FILES ${TBB_BIN_PATH}/tbb.dll)
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/inference_engine)
file(COPY ${TBB_BIN_FILES} DESTINATION ${PROJECT_BINARY_DIR}/inference_engine)

get_filename_component(NGRAPH_BIN_PATH "${InferenceEngine_INCLUDE_DIRS}/../../ngraph/lib" ABSOLUTE)
file(GLOB_RECURSE NGRAPH_BIN_FILES ${NGRAPH_BIN_PATH}/ngraph.dll)
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/inference_engine)
file(COPY ${NGRAPH_BIN_FILES} DESTINATION ${PROJECT_BINARY_DIR}/inference_engine)

get_filename_component(OpenCV_BIN_PATH "${OpenCV_INCLUDE_DIRS}/../bin" ABSOLUTE)
file(GLOB_RECURSE OpenCV_BIN_FILES ${OpenCV_BIN_PATH}/*.*)
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/opencv)
file(COPY ${OpenCV_BIN_FILES} DESTINATION ${PROJECT_BINARY_DIR}/opencv)



#message(STATUS "hello ${OpenCV_INCLUDE_DIRS}")

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/intelc)
file(COPY "C:/Program Files (x86)/Common Files/Intel/Shared Libraries/redist/intel64/compiler/svml_dispmd.dll" DESTINATION ${PROJECT_BINARY_DIR}/intelc)
