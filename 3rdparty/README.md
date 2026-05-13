# 3rdparty_libs 第三方库依赖模块

本模块用于统一管理和导出项目中使用的第三方 C/C++ 库。通过 CMake 的 `PARENT_SCOPE` 机制，本模块会将各个第三方库的**头文件路径**和**库文件路径**导出为主项目中的 CMake 变量，方便主项目直接调用。

## 📦 包含的第三方库

本模块集成了以下第三方库：

1. **rknpu2** : 瑞芯微 NPU 推理库 (动态库)
2. **librga** : 瑞芯微 2D 图形加速库 (静态库 + 动态库安装)
3. **stb_image** : 图像加载库 (仅头文件)
4. **jpeg_turbo** : 高性能 JPEG 编解码库 (静态库)
5. **libsndfile** : 音频文件读写库 (静态库)
6. **libhikvision** : 海康威视 SDK (动态库目录)
7. **allocator**: 内存分配器库（动态库，需另外包含该目录下的cmakelist来进行编译）
8. **utils**: 语音、文件、图像处理工具库（需另外包含该目录下的cmakelist来进行编译）
9. **libmpp**: 瑞芯微 VPU 视频编码转换库 (动态库)

## 🚀 如何在主项目中使用

### 第一步：引入子目录

将本模块放入主项目的目录下（例如命名为 `3rdparty`），然后在主项目的根 `CMakeLists.txt` 中添加此目录：

**CMake**

```
# 主项目 CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(MyMainProject)

# 1. 添加第三方库子目录
add_subdirectory(3rdparty) 
```

### 第二步：配置头文件与链接库

在主项目中定义了你的目标可执行文件（或库）后，使用本模块导出的变量进行配置：

**CMake**

```
# 假设你的主程序名为 my_app
add_executable(my_app main.cpp)

# 2. 包含需要的头文件路径
target_include_directories(my_app PRIVATE
    ${LIBRKNNRT_INCLUDES}
    ${LIBRGA_INCLUDES}
    ${STB_INCLUDES}
    ${LIBJPEG_INCLUDES}
    ${LIBSNDFILE_INCLUDES}
    ${LIBHIKVISION_INCLUDES}
    ${LIBMPP_INCLUDES}
)

# 3. 链接需要的库文件
target_link_libraries(my_app PRIVATE
    ${LIBRKNNRT}
    ${LIBRGA}
    ${LIBJPEG}
    ${LIBSNDFILE}
    ${HIKVISION_LIBS}
    ${MPP_LIBS}
)
```

## 📋 导出的 CMake 变量参考手册

在执行完 `add_subdirectory()` 后，主项目将可以访问以下变量：

| **第三方库名称** | **导出的头文件变量 (*_INCLUDES)** | **导出的库文件变量** | **库类型**  |
| ---------------------- | --------------------------------------- | -------------------------- | ----------------- |
| **rknpu2**       | `LIBRKNNRT_INCLUDES`                  | `LIBRKNNRT`              | `.so`(动态)     |
| **librga**       | `LIBRGA_INCLUDES`                     | `LIBRGA`                 | `.a`(静态)      |
| **stb_image**    | `STB_INCLUDES`                        | *(无，Header-only)*      | Header-only       |
| **jpeg_turbo**   | `LIBJPEG_INCLUDES`                    | `LIBJPEG`                | `.a`(静态)      |
| **libsndfile**   | `LIBSNDFILE_INCLUDES`                 | `LIBSNDFILE`             | `.a`(静态)      |
| **libhikvision** | `LIBHIKVISION_INCLUDES`               | `HIKVISION_LIBS`         | `.so`(动态集合) |
| **libmpp**       | `LIBMPP_INCLUDES`                     | `MPP_LIBS`               | `.so`(动态集合) |

## 🛠️ 关于安装 (Install)

本模块内置了 `install` 指令。当你在主项目中执行 `make install` 或 `ninja install` 时，相关的动态链接库 (`.so`) 会被自动拷贝到安装目录的 `lib` 文件夹下。

具体行为如下：

* `librknnrt.so` -> `DESTINATION lib`
* `librga.so` -> `DESTINATION lib`
* `libhikvision/lib/*.so` -> `DESTINATION lib`

> **提示：** 请确保在主项目的根 `CMakeLists.txt` 中设置了 `CMAKE_INSTALL_PREFIX`，以指定最终的安装路径。
