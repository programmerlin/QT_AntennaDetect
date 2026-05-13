#-------------------------------------------------
#
# 完整的 Qt 项目配置文件
# 包含所有必要的依赖
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QT_AntennaDetect
TEMPLATE = app

# 定义
DEFINES += QT_DEPRECATED_WARNINGS

# 添加包含路径
INCLUDEPATH += \
    ./ \
    ./src \
    ./src/3rdparty/utils \
    ./src/3rdparty/allocator/include \
    ./src/3rdparty/libhikvision/include \
    ../Antenna_Visioner/include \
    ../USB_HikCamera/include \
    ../3rdparty/opencv/include \
    ../参考代码/librga-main/samples/utils/allocator/include

# 库路径
LIBS += \
    -L../lib \
    -L../3rdparty/opencv/lib \
    -L../Antenna_Visioner/build \
    -L../USB_HikCamera/build/build_USB_HikCamera_lib \
    -L../dma_buffer_pool/src \

# 链接的库
LIBS += \
    -lrknn_api \
    -lrga \
    -lisp \
    -lrockchip_rga \
    -lAntenna_Visioner_lib \
    -lUSB_HikCamera_lib \
    -ldma_buffer_pool \
    -lopencv_core \
    -lopencv_highgui \
    -lopencv_imgproc \
    -lopencv_imgcodecs \
    -lpthread \
    -ldl \

# 源文件
SOURCES += \
    main.cpp \
    widget.cpp \
    \
    # Antenna_Visioner 源文件
    ../Antenna_Visioner/src/Antenna_visioner.cpp \
    ../Antenna_Visioner/src/postprocess.cc \
    ../Antenna_Visioner/src/yolov8.cc \
    \
    # USB_HikCamera 源文件
    ../USB_HikCamera/src/HikCamera.cpp \
    \
    # 工具库源文件
    ../3rdparty/utils/image_drawing.c \
    ../3rdparty/utils/image_utils.c \
    ../3rdparty/utils/file_utils.c \
    ../3rdparty/utils/audio_utils.c \
    \
    # DMA 缓冲池源文件
    ../dma_buffer_pool/src/dma_buffer_pool.cpp \
    ../3rdparty/allocator/dma_alloc.cpp \
    ../3rdparty/allocator/drm_alloc.cpp

# 头文件
HEADERS += \
    widget.h \
    \
    # Antenna_Visioner 头文件
    ../Antenna_Visioner/include/Antenna_Visioner.h \
    ../Antenna_Visioner/include/yolov8.h \
    ../Antenna_Visioner/include/postprocess.h \
    \
    # USB_HikCamera 头文件
    ../USB_HikCamera/include/HikCamera.h \
    ../USB_HikCamera/include/ThreadSafeQueue.h \
    \
    # 工具库头文件
    ../3rdparty/utils/common.h \
    ../3rdparty/utils/image_drawing.h \
    ../3rdparty/utils/image_utils.h \
    ../3rdparty/utils/file_utils.h \
    ../3rdparty/utils/audio_utils.h \
    ../3rdparty/utils/font.h \
    \
    # DMA 相关头文件
    ../3rdparty/allocator/include/dma_alloc.h \
    ../3rdparty/allocator/include/drm_alloc.h \
    ../dma_buffer_pool/include/dma_buffer_pool.h \
    \
    # 相机 SDK 头文件
    ../3rdparty/libhikvision/include/MvCameraControl.h \
    ../3rdparty/libhikvision/include/CameraParams.h \
    ../3rdparty/libhikvision/include/MvErrorDefine.h \
    ../3rdparty/libhikvision/include/MvISPErrorDefine.h \
    ../3rdparty/libhikvision/include/ObsoleteCamParams.h \
    ../3rdparty/libhikvision/include/PixelType.h

# 表单文件
FORMS += widget.ui

# 自动生成 moc 文件
QT += core

# 确保包含 widgets 模块
QT += widgets

# 输出信息
message("编译天线检测系统...")
message("包含路径: $$INCLUDEPATH")
message("库文件: $$LIBS")