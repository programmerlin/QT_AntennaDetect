#-------------------------------------------------
#
# 第二步：添加 OpenCV 支持的 Qt 项目配置文件
# 集成 OpenCV 图像处理功能
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QT_AntennaDetect_Step2
TEMPLATE = app

# 定义
DEFINES += QT_DEPRECATED_WARNINGS

# OpenCV 配置
# 注意：确保 3rdparty/opencv 目录下包含 include 和 lib 子目录
INCLUDEPATH += ../3rdparty/opencv/include

# OpenCV 库路径
# 根据实际部署环境调整路径
LIBS += -L../3rdparty/opencv/lib \
        -lopencv_core \
        -lopencv_highgui \
        -lopencv_imgproc \
        -lopencv_imgcodecs \
        -lopencv_videoio

# 如果使用动态链接，可能需要添加
# LIBS += -L../3rdparty/opencv/lib -lopencv_core340 -lopencv_highgui340 -lopencv_imgproc340

# RKNN 相关配置（暂时注释，后续启用）
# INCLUDEPATH += ../Antenna_Visioner/include
# LIBS += -L../lib -lrknn_api

# 源文件
SOURCES += \
    main.cpp \
    widget.cpp

# 头文件
HEADERS += \
    widget.h

# 表单文件
FORMS += widget.ui

# 自动生成 moc 文件
QT += core

# 确保包含 widgets 模块
QT += widgets

# 输出信息
message("编译 Qt 界面 + OpenCV 图像处理功能...")
message("OpenCV 路径: ../3rdparty/opencv")
message("OpenCV 库: core, highgui, imgproc, imgcodecs, videoio")