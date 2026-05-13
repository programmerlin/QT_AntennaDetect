cmake_minimum_required(VERSION 3.4.1)

project(Antenna_Visioner)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--allow-shlib-undefined")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-shlib-undefined") 
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置安装路径
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(STATUS "Running as STANDALONE project. Setting local install prefix.")
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/install)
else()
    message(STATUS "Running as SUB-PROJECT. Using parent's install prefix: ${CMAKE_INSTALL_PREFIX}")
endif()

# RPATH 设置 (解决运行找不到库的问题)
set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# OpenCV 配置
set(OpenCV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/opencv/share/OpenCV)
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV libraries found: ${OpenCV_LIBS}")
set(OPENCV_LIB_SRC_DIR "${CMAKE_SOURCE_DIR}/../3rdparty/opencv/lib")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/ 3rdparty.out)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/utils/ utils.out)
# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/allocator/ allocator.out)
if(NOT TARGET USB_HikCamera_lib)
    add_subdirectory(../USB_HikCamera build_USB_HikCamera_lib)
endif()

# 编译库
add_library(Antenna_Visioner_lib STATIC
    src/Antenna_visioner.cpp
    src/postprocess.cc
    src/yolov8_zero_copy.cc
)

target_compile_definitions(Antenna_Visioner_lib PUBLIC ZERO_COPY) #向编译器传递一个预处理宏(ZERO_COPY)定义

target_include_directories(Antenna_Visioner_lib PUBLIC
    ${LIBRKNNRT_INCLUDES}
    ${LIBRGA_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty
    ${CMAKE_CURRENT_SOURCE_DIR}/../USB_HikCamera/include
)

target_link_libraries(Antenna_Visioner_lib PUBLIC
    imageutils
    fileutils
    imagedrawing    
    ${LIBRKNNRT}
    ${OpenCV_LIBS}
    dl
    allocator_obj
    USB_HikCamera_lib
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(Antenna_Visioner_lib PUBLIC Threads::Threads)

# 编译可执行文件
add_executable(Antenna_Visioner_demo src/test_demo.cpp)

target_link_libraries(Antenna_Visioner_demo Antenna_Visioner_lib)

# 安装规则
install(TARGETS Antenna_Visioner_demo DESTINATION ./)
install(DIRECTORY ${OPENCV_LIB_SRC_DIR}/ DESTINATION lib)
install(DIRECTORY model/yolov8/ DESTINATION ./yolov8_model)

