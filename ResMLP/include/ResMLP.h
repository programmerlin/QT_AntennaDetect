#ifndef RESMLP_H
#define RESMLP_H

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>

namespace resmlp {

// ============================================================
//  常量定义
// ============================================================

/// 四个阵列中心坐标 (单位: 米)
static const float CENTERS[4][3] = {
    { 0.16f,  0.16f, 0.0f},
    {-0.16f,  0.16f, 0.0f},
    { 0.16f, -0.16f, 0.0f},
    {-0.16f, -0.16f, 0.0f}
};

/// 默认的相位→电压 一元二次方程系数: phase = a*V² + b*V + c
static const float DEFAULT_COEFFS[4][3] = {
    {-0.9998f, 40.8677f, 92.1791f},   // 通道1
    {-1.0230f, 41.0820f, 91.8121f},   // 通道2
    {-1.0731f, 42.3290f, 97.6598f},   // 通道3
    {-1.0958f, 43.0472f, 92.4784f}    // 通道4
};

/// 固定 Z 坐标 (单位: 米, 与训练数据一致)
static const float Z_FIXED = 3.6f;

/// 最大中间特征维度 (用于缓冲区分配)
static const int MAX_HIDDEN = 256;

/// 残差块数量
static const int NUM_BLOCKS = 6;

// ============================================================
//  ResMLP — 残差多层感知机推理引擎 (纯 CPU)
// ============================================================
//
//  实际模型架构 (源自 .pth 权重):
//    Input(20) → BB(20→64)  [无残差]
//              → BB(64→128) [无残差]
//              → BB(128→256)[有残差]
//              → BB(256→128)[有残差]
//              → BB(128→64) [无残差]
//              → BB(64→32)  [无残差]
//              → Dropout → Linear(32→8)
//  每个 BasicBlock: Mish(block(x)) 或 Mish(block(x) + shortcut(x))
//    block: Linear → BN → Mish → Linear → BN
//    shortcut: Linear (use_shortcut=True 且维度不同) 或 None
//
// ============================================================

struct BlockWeights {
    // block.0: Linear(in_dim → out_dim)
    float* w1 = nullptr; float* b1 = nullptr;
    // block.1: BatchNorm1d(out_dim)
    float* bn1_g = nullptr; float* bn1_b = nullptr;
    float* bn1_m = nullptr; float* bn1_v = nullptr;
    // block.3: Linear(out_dim → out_dim)
    float* w2 = nullptr; float* b2 = nullptr;
    // block.4: BatchNorm1d(out_dim)
    float* bn2_g = nullptr; float* bn2_b = nullptr;
    float* bn2_m = nullptr; float* bn2_v = nullptr;
    // shortcut: Linear(in_dim → out_dim) 或 nullptr (identity)
    float* sw = nullptr; float* sb = nullptr;
    // 维度
    int in_dim = 0;
    int out_dim = 0;
    bool has_shortcut = false;  // false = 无残差连接 (shortcut=None)
};

class ResMLP {
public:
    ResMLP();
    ~ResMLP();

    /// 从自定义二进制权重文件加载 (由 export_weights.py 生成)
    bool load(const std::string& weightPath);

    /// 模型是否已加载
    bool isLoaded() const { return loaded_; }

    /// 高阶: 给定俯仰角 θ 和方位角 φ (度数), 输出 4 路电压值 (V)
    std::vector<float> predict(float thetaDeg, float phiDeg,
                               float zFixed = Z_FIXED);

    /// 高阶: 给定物理世界坐标 (x, y, z 米), 输出 4 路电压值 (V)
    std::vector<float> predictFromXYZ(float x, float y, float z);

    /// 相位角 (度) → 电压 (V) — 使用一元二次方程
    static std::vector<float> phasesToVoltages(
        const std::vector<float>& phasesDeg,
        const float coeffs[4][3] = DEFAULT_COEFFS);

private:
    bool loaded_;

    // ======================== 模型参数 ========================
    BlockWeights blocks_[NUM_BLOCKS];
    float* final_w_ = nullptr;   // layers.7.weight (8×32)
    float* final_b_ = nullptr;   // layers.7.bias   (8)

    // ======================== 工作缓冲区 ========================
    float* buf_in_  = nullptr;   // 当前块输入 / max(256)
    float* buf_out_ = nullptr;   // 当前块输出 / max(256)
    float* buf_skip_ = nullptr;  // shortcut 结果暂存 / max(256)
    float* buf_final_ = nullptr; // 最终输出 (8)

    // ======================== 内部运算 ========================
    static void linear(const float* x, int inDim,
                       const float* w, const float* b, int outDim,
                       float* y);
    static void batchnorm(const float* x, int n,
                          const float* g, const float* b,
                          const float* m, const float* v,
                          float eps, float* y);
    static void relu(float* x, int n);
    static void mish(float* x, int n);
    static void add(const float* a, const float* b, int n, float* y);

    // 执行一个 BasicBlock 的前向
    void forwardBlock(const BlockWeights& blk, const float* input, float* output);

    // (x, y, z) → 20 维特征向量
    static void extractFeatures(float x, float y, float z, float* feat);

    // 内存释放
    void freeAll();
    static float* allocF(int n);
};

}  // namespace resmlp

#endif  // RESMLP_H
