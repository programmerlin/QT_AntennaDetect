#-------------------------------------------------
#
# Qt 天线检测系统集成项目
#-------------------------------------------------

QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QT_AntennaDetect
TEMPLATE = app

# 基础定义
DEFINES += QT_DEPRECATED_WARNINGS

# OpenCV 支持
INCLUDEPATH += ../3rdparty/opencv/include
LIBS += -L../3rdparty/opencv/lib \
        -lopencv_core \
        -lopencv_highgui \
        -lopencv_imgproc \
        -lopencv_imgcodecs

# RKNN 和 RGA 库
INCLUDEPATH += ../Antenna_Visioner/include \
               ../USB_HikCamera/include \
               ../dma_buffer_pool/include \
               ../3rdparty/libhikvision/include \
               ../3rdparty/allocator/include \
               ../3rdparty/utils \
               ../参考代码/librga-main/samples/utils/allocator/include \
               ../3rdparty/rga \
               ../3rdparty/librga/include \
               ../参考代码/librga-main/include \
               ../3rdparty/rknpu2/include

# 静态库文件
LIBS += -L./lib \
        -lUSB_HikCamera_lib \
        -ldma_buffer_pool_lib \
        -lrknn_api \
        -lrga \
        -lisp \
        -lrockchip_rga \
        -lpthread \
        -ldl

# 源文件
SOURCES += src/main.cpp \
           src/widget.cpp \
           src/camera_utils.cpp

# 头文件
HEADERS += include/widget.h \
           include/camera_utils.h

# 表单文件
FORMS += ui/widget.ui

# 链接模式
CONFIG += link_pkgconfig