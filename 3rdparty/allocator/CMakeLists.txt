add_library(allocator_obj STATIC)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/ 3rdparty.out)

target_sources(allocator_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/dma_alloc.cpp
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include/dma_alloc.h
)

target_link_libraries(allocator_obj PUBLIC 
    ${LIBRKNNRT}
)

target_include_directories(allocator_obj PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../libdrm/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libdrm/include/libdrm
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBRGA_INCLUDES}
)
