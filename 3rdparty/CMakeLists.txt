cmake_minimum_required(VERSION 3.10)

project(3rdparty_libs)

# for rknpu2
set(RKNN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/rknpu2)
set(LIBRKNNRT ${RKNN_PATH}/lib/librknnrt.so PARENT_SCOPE)
set(LIBRKNNRT_INCLUDES ${RKNN_PATH}/include PARENT_SCOPE)
install(PROGRAMS ${RKNN_PATH}/lib/librknnrt.so DESTINATION lib)

# rga
set(RGA_PATH ${CMAKE_CURRENT_SOURCE_DIR}/librga)
set(LIBRGA ${RGA_PATH}/lib/librga.a PARENT_SCOPE)
set(LIBRGA_INCLUDES ${RGA_PATH}/include PARENT_SCOPE)
install(PROGRAMS ${RGA_PATH}/lib/librga.so DESTINATION lib)

# stb image（将图片转化为像素数组即RGB数据）
set(STB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/stb_image)
set(STB_INCLUDES ${STB_PATH} PARENT_SCOPE)

# jpeg turbo（处理速度更快的将图片转化为像素数组库文件）
set(JPEG_PATH ${CMAKE_CURRENT_SOURCE_DIR}/jpeg_turbo)
set(LIBJPEG ${JPEG_PATH}/lib/libturbojpeg.a PARENT_SCOPE)
set(LIBJPEG_INCLUDES ${JPEG_PATH}/include PARENT_SCOPE)

# libsndfile（处理音频文件的库）
set(LIBSNDFILE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libsndfile)
set(LIBSNDFILE_INCLUDES ${LIBSNDFILE_PATH}/include PARENT_SCOPE)
set(LIBSNDFILE ${LIBSNDFILE_PATH}/lib/libsndfile.a PARENT_SCOPE)

#libhikvision（处理海康威视相关功能的库）
set(LIBHIKVISION_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libhikvision)
file(GLOB HIKVISION_LIBS "${LIBHIKVISION_PATH}/lib/*.so")
set(HIKVISION_LIBS ${HIKVISION_LIBS} PARENT_SCOPE)
set(LIBHIKVISION_INCLUDES ${LIBHIKVISION_PATH}/include PARENT_SCOPE)
install(DIRECTORY ${LIBHIKVISION_PATH}/lib/ DESTINATION lib)

# MPP（处理多媒体处理相关功能的库）
set(MPP_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libmpp)
file(GLOB MPP_LIBS "${MPP_PATH}/lib/*.so")
set(MPP_LIBS ${MPP_LIBS} PARENT_SCOPE)
set(LIBMPP_INCLUDES ${MPP_PATH}/include PARENT_SCOPE)
install(DIRECTORY ${MPP_PATH}/lib/ DESTINATION lib)
