#!/bin/bash
set -e

# ============================================================
#  QT_AntennaDetect 一键编译脚本
#  用法:
#    ./build.sh          - 完整编译 (qmake + make)
#    ./build.sh clean    - 清理编译产物
#    ./build.sh rebuild  - 清理后重新编译
# ============================================================

# ---- 路径配置 ----
PROJECT_DIR=$(cd "$(dirname "$0")" && pwd)
PRO_FILE="QT_AntennaDetect_Step3.pro"
TARGET_NAME="QT_AntennaDetect_Step3"

BUILD_DIR="${PROJECT_DIR}/build"
INSTALL_DIR="${PROJECT_DIR}/install"

QMAKE="/home/topeet/Linux/rk356x_linux/buildroot/output/rockchip_rk3568/host/bin/qmake"

# ---- 颜色输出 ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_info()  { echo -e "${GREEN}[INFO]${NC}  $1"; }
print_warn()  { echo -e "${YELLOW}[WARN]${NC}  $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# ---- 检查 qmake 是否存在 ----
check_qmake() {
    if [ ! -f "${QMAKE}" ]; then
        print_error "qmake 未找到: ${QMAKE}"
        print_error "请确认 SDK 路径是否正确"
        exit 1
    fi
    print_info "qmake: ${QMAKE}"
}

# ---- 清理 ----
do_clean() {
    print_info "清理编译产物..."
    if [ -d "${BUILD_DIR}" ]; then
        rm -rf "${BUILD_DIR}"
        print_info "已删除: ${BUILD_DIR}"
    fi
    if [ -d "${INSTALL_DIR}" ]; then
        rm -rf "${INSTALL_DIR}"
        print_info "已删除: ${INSTALL_DIR}"
    fi
    print_info "清理完成"
}

# ---- 编译 ----
do_build() {
    check_qmake

    # 1. 创建目录
    print_info "创建输出目录..."
    mkdir -p "${BUILD_DIR}"
    mkdir -p "${INSTALL_DIR}"
    mkdir -p "${INSTALL_DIR}/lib"

    # 2. qmake (在 build 目录中生成 Makefile)
    print_info "运行 qmake..."
    cd "${BUILD_DIR}"
    "${QMAKE}" -o Makefile "${PROJECT_DIR}/${PRO_FILE}"
    cd "${PROJECT_DIR}"

    # 3. make
    print_info "开始编译 (make -j4)..."
    cd "${BUILD_DIR}"
    make -j4
    cd "${PROJECT_DIR}"

    # 4. 验证输出
    if [ -f "${INSTALL_DIR}/${TARGET_NAME}" ]; then
        print_info "============================================"
        print_info "编译成功!"
        print_info "可执行文件: ${INSTALL_DIR}/${TARGET_NAME}"
        if [ -d "${INSTALL_DIR}/lib" ]; then
            print_info "依赖库目录: ${INSTALL_DIR}/lib/"
        fi
        if [ -d "${INSTALL_DIR}/model" ]; then
            print_info "模型文件:   ${INSTALL_DIR}/model/"
        fi
        print_info "============================================"
    else
        print_error "编译失败: 未生成可执行文件"
        exit 1
    fi
}

# ---- 主入口 ----
case "$1" in
    clean)
        do_clean
        ;;
    rebuild)
        do_clean
        do_build
        ;;
    *)
        do_build
        ;;
esac
