#include "../include/ResMLP.h"

namespace resmlp {

// ============================================================
//  构造 / 析构 / 内存管理
// ============================================================

ResMLP::ResMLP() : loaded_(false) {
    for (int i = 0; i < NUM_BLOCKS; i++) {
        blocks_[i] = BlockWeights();
    }
}

ResMLP::~ResMLP() { freeAll(); }

float* ResMLP::allocF(int n) {
    if (n <= 0) return nullptr;
    float* p = new (std::nothrow) float[n]();
    if (!p) std::cerr << "[ResMLP] allocF(" << n << ") failed" << std::endl;
    return p;
}

void ResMLP::freeAll() {
    loaded_ = false;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        auto& b = blocks_[i];
        delete[] b.w1;      b.w1 = nullptr;
        delete[] b.b1;      b.b1 = nullptr;
        delete[] b.bn1_g;   b.bn1_g = nullptr;
        delete[] b.bn1_b;   b.bn1_b = nullptr;
        delete[] b.bn1_m;   b.bn1_m = nullptr;
        delete[] b.bn1_v;   b.bn1_v = nullptr;
        delete[] b.w2;      b.w2 = nullptr;
        delete[] b.b2;      b.b2 = nullptr;
        delete[] b.bn2_g;   b.bn2_g = nullptr;
        delete[] b.bn2_b;   b.bn2_b = nullptr;
        delete[] b.bn2_m;   b.bn2_m = nullptr;
        delete[] b.bn2_v;   b.bn2_v = nullptr;
        delete[] b.sw;      b.sw = nullptr;
        delete[] b.sb;      b.sb = nullptr;
    }
    delete[] final_w_;   final_w_ = nullptr;
    delete[] final_b_;   final_b_ = nullptr;
    delete[] buf_in_;    buf_in_ = nullptr;
    delete[] buf_out_;   buf_out_ = nullptr;
    delete[] buf_skip_;  buf_skip_ = nullptr;
    delete[] buf_final_; buf_final_ = nullptr;
}

// ============================================================
//  内部运算
// ============================================================

void ResMLP::linear(const float* x, int inDim,
                    const float* w, const float* b, int outDim,
                    float* y) {
    // w: (outDim, inDim) row-major
    for (int i = 0; i < outDim; i++) {
        float sum = b ? b[i] : 0.0f;
        for (int j = 0; j < inDim; j++) {
            sum += x[j] * w[i * inDim + j];
        }
        y[i] = sum;
    }
}

void ResMLP::batchnorm(const float* x, int n,
                       const float* g, const float* beta,
                       const float* m, const float* v,
                       float eps, float* y) {
    for (int i = 0; i < n; i++) {
        y[i] = g[i] * (x[i] - m[i]) / std::sqrt(v[i] + eps) + beta[i];
    }
}

void ResMLP::relu(float* x, int n) {
    for (int i = 0; i < n; i++)
        if (x[i] < 0.0f) x[i] = 0.0f;
}

void ResMLP::mish(float* x, int n) {
    // mish(x) = x * tanh(ln(1 + e^x))
    for (int i = 0; i < n; i++) {
        float e = std::exp(x[i]);
        float softplus = std::log(1.0f + e);
        x[i] *= std::tanh(softplus);
    }
}

void ResMLP::add(const float* a, const float* b, int n, float* y) {
    for (int i = 0; i < n; i++) y[i] = a[i] + b[i];
}

// ============================================================
//  BasicBlock 前向
// ============================================================

void ResMLP::forwardBlock(const BlockWeights& blk,
                          const float* input, float* output) {
    int d = blk.out_dim;

    // 关键约束: input 可能与 output/buf_out_ 指向同一区域.
    // 方案:
    //   L1 → buf_in_   BN1 → buf_skip_   Mish → buf_skip_
    //   L2 → buf_in_   BN2 → buf_in_
    //   组合 + Mish → output

    linear(input, blk.in_dim, blk.w1, blk.b1, d, buf_in_);
    batchnorm(buf_in_, d, blk.bn1_g, blk.bn1_b, blk.bn1_m, blk.bn1_v, 1e-5f, buf_skip_);
    mish(buf_skip_, d);

    linear(buf_skip_, d, blk.w2, blk.b2, d, buf_in_);
    batchnorm(buf_in_, d, blk.bn2_g, blk.bn2_b, blk.bn2_m, blk.bn2_v, 1e-5f, buf_in_);

    // buf_in_ 此时为 block(x) (BN2 之后, 未经过最终 Mish)

    if (blk.has_shortcut) {
        // 有残差: output = Mish(block(x) + shortcut(x))
        linear(input, blk.in_dim, blk.sw, blk.sb, d, buf_skip_);
        add(buf_in_, buf_skip_, d, output);
        mish(output, d);
    } else {
        // 无残差: output = Mish(block(x))
        mish(buf_in_, d);
        std::memcpy(output, buf_in_, d * sizeof(float));
    }
}

// ============================================================
//  坐标与特征变换
// ============================================================

void ResMLP::extractFeatures(float x, float y, float z, float* feat) {
    // 必须匹配 Python 特征排序:
    //   feat[0..7]  = [sin(elev0), sin(azim0), sin(elev1), sin(azim1), ...]
    //   feat[8..15] = [cos(elev0), cos(azim0), cos(elev1), cos(azim1), ...]
    //   feat[16..19]= [dist0, dist1, dist2, dist3]
    float sinVals[8];
    float cosVals[8];
    float dists[4];
    for (int i = 0; i < 4; i++) {
        float dx = x - CENTERS[i][0];
        float dy = y - CENTERS[i][1];
        float dz = z - CENTERS[i][2];
        dists[i] = std::sqrt(dx*dx + dy*dy + dz*dz);
        float elev = std::atan2(dz, std::sqrt(dx*dx + dy*dy));
        float azim = std::atan2(dy, dx);
        sinVals[i*2+0] = std::sin(elev);
        sinVals[i*2+1] = std::sin(azim);
        cosVals[i*2+0] = std::cos(elev);
        cosVals[i*2+1] = std::cos(azim);
    }
    std::memcpy(feat,       sinVals, 8 * sizeof(float));
    std::memcpy(feat + 8,   cosVals, 8 * sizeof(float));
    std::memcpy(feat + 16,  dists,   4 * sizeof(float));
}

// ============================================================
//  相位 → 电压 转换
// ============================================================

std::vector<float> ResMLP::phasesToVoltages(
    const std::vector<float>& phasesDeg,
    const float coeffs[4][3]) {
    std::vector<float> v(4, 0.0f);
    for (int i = 0; i < 4 && i < (int)phasesDeg.size(); i++) {
        float ph = phasesDeg[i];
        if (ph < 100.0f) ph += 360.0f;
        float a = coeffs[i][0], b = coeffs[i][1], c = coeffs[i][2];
        float D = b*b - 4.0f*a*(c - ph);
        float volt = 0.0f;
        if (D < 0.0f) {
            if (std::abs(b) > 1e-6f) volt = (ph - c) / b;
        } else {
            float sD = std::sqrt(D);
            float V1 = (-b + sD) / (2.0f*a);
            float V2 = (-b - sD) / (2.0f*a);
            bool ok1 = (V1 >= 0.0f && V1 <= 14.0f);
            bool ok2 = (V2 >= 0.0f && V2 <= 14.0f);
            if (ok1 && ok2) volt = (std::abs(V1-7.0f) <= std::abs(V2-7.0f)) ? V1 : V2;
            else if (ok1) volt = V1;
            else if (ok2) volt = V2;
            else if (std::abs(b) > 1e-6f) volt = (ph - c) / b;
        }
        if (volt < 0.0f) volt = 0.0f;
        if (volt > 14.0f) volt = 14.0f;
        v[i] = volt;
    }
    return v;
}

// ============================================================
//  权重加载
// ============================================================

static const char* PARAM_NAMES[][2] = {
    // layers.X.block.Y.xxx
    {"layers.0.block.0.weight", "layers.0.block.0.bias"},
    {"layers.0.block.1.weight", "layers.0.block.1.bias"},
    {"", ""}, // running_mean/var loaded differently
    {"layers.0.block.3.weight", "layers.0.block.3.bias"},
    {"layers.0.block.4.weight", "layers.0.block.4.bias"},
    {"layers.0.shortcut.weight", "layers.0.shortcut.bias"},
    // ... done by index in load()
    {nullptr, nullptr}
};

bool ResMLP::load(const std::string& weightPath) {
    freeAll();

    std::ifstream f(weightPath, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[ResMLP] 无法打开: " << weightPath << std::endl;
        return false;
    }

    uint32_t magic = 0;
    f.read((char*)&magic, 4);
    if (magic != 0x504D4C52) {
        std::cerr << "[ResMLP] 无效的 magic: 0x" << std::hex << magic << std::endl;
        return false;
    }

    uint32_t numT = 0;
    f.read((char*)&numT, 4);

    // 读取所有 tensor 到 map
    std::map<std::string, std::vector<float>> tensors;
    for (uint32_t i = 0; i < numT; i++) {
        uint32_t nlen = 0;
        f.read((char*)&nlen, 4);
        if (nlen > 256) { f.close(); return false; }
        std::string name(nlen, '\0');
        f.read(&name[0], nlen);
        uint32_t dlen = 0;
        f.read((char*)&dlen, 4);
        std::vector<float> data(dlen);
        if (dlen > 0) f.read((char*)data.data(), dlen * 4);
        tensors[name] = std::move(data);
    }
    f.close();

    // 辅助: 获取 tensor
    auto T = [&](const std::string& n) -> const std::vector<float>* {
        auto it = tensors.find(n);
        return (it != tensors.end()) ? &it->second : nullptr;
    };

    // 验证并获取 tensor 大小
    auto check = [&](const std::string& n, int expected) -> const std::vector<float>* {
        auto* p = T(n);
        if (!p || (int)p->size() != expected) {
            std::cerr << "[ResMLP] " << n << " 缺失/大小不匹配"
                      << " (期望 " << expected << ", 实际 "
                      << (p ? (int)p->size() : -1) << ")" << std::endl;
            return nullptr;
        }
        return p;
    };

    // ====== BasicBlock 配置: {name_idx, in_dim, out_dim} ======
    int blockCfgs[6][3] = {
        {0, 20, 64},     // layers.0: BB(20→64)
        {1, 64, 128},    // layers.1: BB(64→128)
        {2, 128, 256},   // layers.2: BB(128→256)
        {3, 256, 128},   // layers.3: BB(256→128)
        {4, 128, 64},    // layers.4: BB(128→64)
        {5, 64, 32}      // layers.5: BB(64→32)
    };

    for (int bi = 0; bi < 6; bi++) {
        int idx = blockCfgs[bi][0];
        int inD = blockCfgs[bi][1];
        int outD = blockCfgs[bi][2];
        auto& blk = blocks_[bi];
        blk.in_dim = inD;
        blk.out_dim = outD;

        char prefix[32];
        std::snprintf(prefix, sizeof(prefix), "layers.%d", idx);

        auto fmt = [&](const char* suffix) -> std::string {
            return std::string(prefix) + suffix;
        };

        // block.0: Linear
        {   auto* p = check(fmt(".block.0.weight"), outD * inD); if (!p) return false;
            blk.w1 = allocF(outD * inD); std::memcpy(blk.w1, p->data(), outD * inD * 4); }
        {   auto* p = check(fmt(".block.0.bias"), outD); if (!p) return false;
            blk.b1 = allocF(outD); std::memcpy(blk.b1, p->data(), outD * 4); }

        // block.1: BN
        {   auto* p = check(fmt(".block.1.weight"), outD); if (!p) return false;
            blk.bn1_g = allocF(outD); std::memcpy(blk.bn1_g, p->data(), outD * 4); }
        {   auto* p = check(fmt(".block.1.bias"), outD); if (!p) return false;
            blk.bn1_b = allocF(outD); std::memcpy(blk.bn1_b, p->data(), outD * 4); }
        {   auto* p = check(fmt(".block.1.running_mean"), outD); if (!p) return false;
            blk.bn1_m = allocF(outD); std::memcpy(blk.bn1_m, p->data(), outD * 4); }
        {   auto* p = check(fmt(".block.1.running_var"), outD); if (!p) return false;
            blk.bn1_v = allocF(outD); std::memcpy(blk.bn1_v, p->data(), outD * 4); }

        // block.3: Linear(out→out) — ALL blocks have shortcut, so ALL have block.3 Linear(out→out)
        {   auto* p = check(fmt(".block.3.weight"), outD * outD); if (!p) return false;
            blk.w2 = allocF(outD * outD); std::memcpy(blk.w2, p->data(), outD * outD * 4); }
        {   auto* p = check(fmt(".block.3.bias"), outD); if (!p) return false;
            blk.b2 = allocF(outD); std::memcpy(blk.b2, p->data(), outD * 4); }

        // block.4: BN
        {   auto* p = check(fmt(".block.4.weight"), outD); if (!p) return false;
            blk.bn2_g = allocF(outD); std::memcpy(blk.bn2_g, p->data(), outD * 4); }
        {   auto* p = check(fmt(".block.4.bias"), outD); if (!p) return false;
            blk.bn2_b = allocF(outD); std::memcpy(blk.bn2_b, p->data(), outD * 4); }
        {   auto* p = check(fmt(".block.4.running_mean"), outD); if (!p) return false;
            blk.bn2_m = allocF(outD); std::memcpy(blk.bn2_m, p->data(), outD * 4); }
        {   auto* p = check(fmt(".block.4.running_var"), outD); if (!p) return false;
            blk.bn2_v = allocF(outD); std::memcpy(blk.bn2_v, p->data(), outD * 4); }

        // shortcut
        std::string sk_w = fmt(".shortcut.weight");
        auto* skp = T(sk_w);
        if (skp && (int)skp->size() == outD * inD) {
            blk.has_shortcut = true;
            blk.sw = allocF(outD * inD); std::memcpy(blk.sw, skp->data(), outD * inD * 4);
            auto* skb = check(fmt(".shortcut.bias"), outD); if (!skb) return false;
            blk.sb = allocF(outD); std::memcpy(blk.sb, skb->data(), outD * 4);
        } else {
            // Identity shortcut
            blk.has_shortcut = false;
        }
    }

    // Final Linear: layers.7 (32→8)
    {   auto* p = check("layers.7.weight", 8 * 32); if (!p) return false;
        final_w_ = allocF(8 * 32); std::memcpy(final_w_, p->data(), 8 * 32 * 4); }
    {   auto* p = check("layers.7.bias", 8); if (!p) return false;
        final_b_ = allocF(8); std::memcpy(final_b_, p->data(), 8 * 4); }

    // 分配工作缓冲区 (最大 256)
    buf_in_   = allocF(MAX_HIDDEN);
    buf_out_  = allocF(MAX_HIDDEN);
    buf_skip_ = allocF(MAX_HIDDEN);
    buf_final_ = allocF(8);

    loaded_ = true;
    std::cout << "[ResMLP] 加载成功: " << NUM_BLOCKS << " blocks, final Linear(32→8)" << std::endl;
    return true;
}

// ============================================================
//  前向推理
// ============================================================

std::vector<float> ResMLP::predictFromXYZ(float x, float y, float z) {
    if (!loaded_) {
        std::cerr << "[ResMLP] 模型未加载" << std::endl;
        return {0,0,0,0};
    }

    // 1. 20-d 特征向量
    float feat[20];
    extractFeatures(x, y, z, feat);

    // 2. 逐块推理
    // 第一块的输入是 feat (20-d), 放到 buf_out_
    std::memcpy(buf_out_, feat, 20 * sizeof(float));

    for (int bi = 0; bi < NUM_BLOCKS; bi++) {
        forwardBlock(blocks_[bi], buf_out_, buf_out_);
    }

    // 3. 最终 Linear(32→8)
    linear(buf_out_, 32, final_w_, final_b_, 8, buf_final_);

    // 4. arctan2 → 相位角 (度)
    float phasesDeg[4];
    for (int i = 0; i < 4; i++) {
        float ang = std::atan2(buf_final_[i], buf_final_[i+4]);
        float deg = ang * 180.0f / (float)M_PI;
        deg = std::fmod(deg + 360.0f, 360.0f);
        phasesDeg[i] = deg;
    }

    // 5. 相位 → 电压
    std::vector<float> pv(phasesDeg, phasesDeg + 4);
    return phasesToVoltages(pv);
}

std::vector<float> ResMLP::predict(float thetaDeg, float phiDeg, float zFixed) {
    float thetaRad = thetaDeg * (float)M_PI / 180.0f;
    float phiRad   = phiDeg * (float)M_PI / 180.0f;
    float tanTh = std::tan(thetaRad);
    if (std::abs(tanTh) < 1e-6f) tanTh = (tanTh >= 0.0f) ? 1e-6f : -1e-6f;
    float rho = zFixed / tanTh;
    float x = rho * std::cos(phiRad);
    float y = rho * std::sin(phiRad);
    return predictFromXYZ(x, y, zFixed);
}

} // namespace resmlp
