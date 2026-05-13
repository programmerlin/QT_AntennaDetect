cmake_minimum_required(VERSION 3.10)

project(HikCamera)

# 设置编译器选项，允许共享库中未定义的符号
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--allow-shlib-undefined")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-shlib-undefined")
#指定C++标准并且不要降级编译
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#设置安装路径
#如果当前目录是顶层项目，则设置安装前缀为当前目录下的install文件夹，否则使用父项目的安装前缀
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(STATUS "Running as STANDALONE project. Setting local install prefix.")
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/install)
else()
    #作为子项目运行，使用父项目的安装前缀
    message(STATUS "Running as SUB-PROJECT. Using parent's install prefix: ${CMAKE_INSTALL_PREFIX}")
endif()

# RPATH 设置 (解决运行找不到库的问题)
set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# OpenCV 配置
set(OpenCV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/opencv/share/OpenCV)
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV libraries found: ${OpenCV_LIBS}")
set(OPENCV_LIB_SRC_DIR "${CMAKE_SOURCE_DIR}/../3rdparty/opencv/lib")

# 添加第三方库中其他库文件，嵌套添加第三方库的CMakeLists.txt
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/ 3rdparty.out)
if(NOT TARGET dma_buffer_pool_lib)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../dma_buffer_pool ${CMAKE_CURRENT_BINARY_DIR}/dma_build)
endif()

#添加自己的静态库文件
add_library(USB_HikCamera_lib STATIC src/HikCamera.cpp)

#设置库文件的包含目录，PUBLIC表示这个库的使用者也需要包含这些目录
target_include_directories(USB_HikCamera_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/libhikvision/include
    ${LIBHIKVISION_INCLUDES}
    ${LIBRGA_INCLUDES}
)

# 查找线程库
find_package(Threads REQUIRED)

target_link_libraries(USB_HikCamera_lib PUBLIC
    ${OpenCV_LIBS}
    ${HIKVISION_LIBS}
    ${LIBRGA}
    Threads::Threads
    dma_buffer_pool_lib
)

# 编译可执行程序
add_executable(usb_hikvisioner_demo
    src/test_demo.cpp
)

target_link_libraries(usb_hikvisioner_demo
    USB_HikCamera_lib
)

# 安装规则
install(TARGETS usb_hikvisioner_demo DESTINATION ./)
install(DIRECTORY ${OPENCV_LIB_SRC_DIR}/ DESTINATION lib)
