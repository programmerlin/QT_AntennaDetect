#-------------------------------------------------
#
# 简化的 Qt 项目配置文件
# 只包含基本 Qt 功能，用于测试编译
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QT_AntennaDetect
TEMPLATE = app

# 定义
DEFINES += QT_DEPRECATED_WARNINGS

# 添加 RKNN 包含路径（暂时注释掉，避免编译错误）
# INCLUDEPATH += ../3rdparty/rknpu2/include

# 源文件 - 只编译基本界面
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
message("编译基本 Qt 界面...")