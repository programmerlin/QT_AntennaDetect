@echo off
echo 开始构建QT天线检测系统...

rem 创建构建目录
if not exist build mkdir build
cd build

rem 生成Makefile
qmake ..\QT_AntennaDetect.pro -o Makefile

rem 编译项目
nmake

rem 检查编译结果
if %errorlevel% equ 0 (
    echo 编译成功！
    echo 可执行文件位置: build\QT_AntennaDetect.exe
) else (
    echo 编译失败！请检查错误信息。
    pause
    exit /b 1
)