#!/bin/bash

# 构建脚本用于QT天线检测系统

echo "开始构建QT天线检测系统..."

# 创建构建目录
mkdir -p build
cd build

# 生成Makefile
qmake ../QT_AntennaDetect.pro -o Makefile

# 编译项目
make -j4

# 检查编译结果
if [ $? -eq 0 ]; then
    echo "编译成功！"
    echo "可执行文件位置: build/QT_AntennaDetect"
else
    echo "编译失败！请检查错误信息。"
    exit 1
fi