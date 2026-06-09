#-------------------------------------------------
# Step3: YOLO检测功能集成
#-------------------------------------------------

QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QT_AntennaDetect_Step3
TEMPLATE = app

# 基础定义
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += ZERO_COPY

#--- 输出目录配置 ---
# 中间编译产物(.o/.moc/ui_*.h等)放入 build 目录
BUILD_DIR = $$PWD/build
OBJECTS_DIR = $$BUILD_DIR/obj
MOC_DIR     = $$BUILD_DIR/moc
RCC_DIR     = $$BUILD_DIR/rcc
UI_DIR      = $$BUILD_DIR/ui

# 可执行文件输出到 install 目录
DESTDIR = $$PWD/install

# C++标准与链接标志
CONFIG += c++14
QMAKE_LFLAGS += -Wl,--allow-shlib-undefined

# 编译后自动复制依赖库和模型文件到 install 目录
QMAKE_POST_LINK += mkdir -p $$DESTDIR/lib && \
    cp -d $$PWD/../3rdparty/rknpu2/lib/*.so*     $$DESTDIR/lib/ 2>/dev/null; \
    cp -d $$PWD/../3rdparty/librga/lib/*.so*     $$DESTDIR/lib/ 2>/dev/null; \
    cp -d $$PWD/../3rdparty/jpeg_turbo/lib/*.so* $$DESTDIR/lib/ 2>/dev/null; \
    cp -d $$PWD/../3rdparty/libhikvision/lib/*.so* $$DESTDIR/lib/ 2>/dev/null; \
    cp -d $$PWD/../3rdparty/libmpp/lib/*.so*     $$DESTDIR/lib/ 2>/dev/null; \
    cp -d $$PWD/../3rdparty/opencv/lib/*.so*     $$DESTDIR/lib/ 2>/dev/null; \
    cp -r $$PWD/../ResMLP/model                  $$DESTDIR/     2>/dev/null; \
    echo "==> Output files in: $$DESTDIR"

# 包含路径
INCLUDEPATH += $$PWD/../Antenna_Visioner/include \
               $$PWD/../USB_HikCamera/include \
               $$PWD/../dma_buffer_pool/include \
               $$PWD/../3rdparty/opencv/include \
               $$PWD/../3rdparty/utils \
               $$PWD/../3rdparty/allocator/include \
               $$PWD/../3rdparty/libhikvision/include \
               $$PWD/../3rdparty/librga/include \
               $$PWD/../3rdparty/jpeg_turbo/include \
               $$PWD/../3rdparty/stb_image \
               $$PWD/../3rdparty/libdrm/include \
               $$PWD/../ResMLP/include

# 源码集成 (注意：确保路径下文件确实存在)
SOURCES += src/main.cpp \
           src/widget.cpp \
           ../Antenna_Visioner/src/Antenna_visioner.cpp \
           ../Antenna_Visioner/src/postprocess.cc \
           ../Antenna_Visioner/src/yolov8_zero_copy.cc \
           ../USB_HikCamera/src/HikCamera.cpp \
           ../dma_buffer_pool/src/dma_buffer_pool.cpp \
           ../3rdparty/utils/file_utils.c \
           ../3rdparty/utils/image_utils.c \
           ../3rdparty/utils/image_drawing.c \
           ../3rdparty/allocator/dma_alloc.cpp \
           ../3rdparty/allocator/drm_alloc.cpp \
           ../ResMLP/src/ResMLP.cpp

HEADERS += include/widget.h \
           ../Antenna_Visioner/include/Antenna_Visioner.h \
           ../USB_HikCamera/include/HikCamera.h \
           ../dma_buffer_pool/include/dma_buffer_pool.h

FORMS += ui/widget.ui

# 外部库链接
LIBS += -L$$PWD/../3rdparty/rknpu2/lib -lrknnrt \
        -L$$PWD/../3rdparty/librga/lib -lrga \
        -L$$PWD/../3rdparty/jpeg_turbo/lib -lturbojpeg \
        -L$$PWD/../3rdparty/libhikvision/lib -lMvCameraControl -lMediaProcess -lFormatConversion \
        -L$$PWD/../3rdparty/libmpp/lib -lrockchip_mpp \
        -ldrm -lpthread -ldl -lm

# OpenCV 链接 (根据你的 install/lib 目录 image_a5b62f.png 确定)
LIBS += -L$$PWD/../3rdparty/opencv/lib \
        -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs -lopencv_videoio