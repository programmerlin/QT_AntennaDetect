/**
 * ResMLP 推理测试程序
 *
 * 编译:
 *   cd ResMLP
 *   g++ -std=c++11 -O2 -Iinclude -o test.exe src/test.cpp src/ResMLP.cpp -lm
 *   ./test.exe
 *
 * 说明:
 *   使用 ResMLP 公开接口进行推理测试，验证:
 *   1. 权重加载
 *   2. predictFromXYZ() 准确性（已知点对照）
 *   3. predict() 端到端推理
 *   4. phasesToVoltages() 相位→电压转换
 *   5. 多次调用的稳定性 / 输出值域合法性
 */

#include "ResMLP.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>

// ============================================================
//  辅助函数
// ============================================================

#define ANSI_GREEN  "\033[32m"
#define ANSI_RED    "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RESET  "\033[0m"

static const char* ok()   { return /*ANSI_GREEN*/  "PASS" /*ANSI_RESET*/; }
static const char* fail() { return /*ANSI_RED*/    "FAIL" /*ANSI_RESET*/; }
static const char* info() { return /*ANSI_YELLOW*/ "INFO" /*ANSI_RESET*/; }

/// 输出 4 路电压值
static void printVoltages(const char* label, const std::vector<float>& v) {
    printf("  %-46s", label);
    if (v.size() < 4) {
        printf("结果异常 (size=%zu)\n", v.size());
        return;
    }
    for (int i = 0; i < 4; i++)
        printf("CH%d=%.4f ", i + 1, v[i]);
    printf("\n");
}

/// 比较两个向量，返回最大绝对误差
static float compare4(const std::vector<float>& got,
                      const float expected[4]) {
    float maxErr = 0.0f;
    for (int i = 0; i < 4 && i < (int)got.size(); i++) {
        float err = std::fabs(got[i] - expected[i]);
        if (err > maxErr) maxErr = err;
    }
    return maxErr;
}

/// 检查值域合法性: 所有值在 [0, 14] 范围内
static bool checkRange(const std::vector<float>& v) {
    for (size_t i = 0; i < v.size(); i++)
        if (v[i] < -0.001f || v[i] > 14.001f) return false;
    return true;
}

// ============================================================
//  测试用例
// ============================================================

/// 全局模型实例
static resmlp::ResMLP g_model;

/// 测试 1: 已知点精度验证 (基准测试)
static int test_known_point() {
    printf("--- Test 1: 已知点精度验证 (基准测试) ---\n");

    auto v = g_model.predictFromXYZ(0.300f, -0.140f, 3.6f);
    printVoltages("predictFromXYZ(0.300, -0.140, 3.6)", v);

    // 该点已验证与 PyTorch 输出匹配 (误差 < 0.001)
    float expected[4] = {8.1888f, 11.1748f, 2.0229f, 5.0606f};
    float err = compare4(v, expected);

    printf("  Max error vs PyTorch: %.6f  (threshold: 0.01)\n", err);
    int pass = (err <= 0.01f);
    printf("  [ %s ]\n\n", pass ? ok() : fail());

    // 重复性验证: 连续调用结果必须一致
    bool repeatOk = true;
    auto prev = v;
    for (int i = 0; i < 5; i++) {
        auto cur = g_model.predictFromXYZ(0.300f, -0.140f, 3.6f);
        if (compare4(cur, prev.data()) > 1e-6f) {
            repeatOk = false;
            printf("  ！重复性失败: iter %d 结果不一致\n", i + 1);
            break;
        }
        prev = cur;
    }
    printf("  重复性 (5次): %s\n\n", repeatOk ? ok() : fail());

    return pass && repeatOk;
}

/// 测试 2: 多坐标点推理 (信息输出, 目视确认合理性)
static int test_multi_points() {
    printf("--- Test 2: 多坐标点推理 (信息输出) ---\n");

    struct { float x, y; const char* desc; } pts[] = {
        { 0.0f,    0.0f,   "原点 (0, 0)" },
        {-0.300f,  0.140f, "(-300, 140) mm" },
        { 0.160f,  0.160f, "通道1中心 (160, 160) mm" },
        {-0.160f, -0.160f, "通道4中心 (-160, -160) mm" },
    };

    for (auto& p : pts) {
        char label[64];
        std::snprintf(label, sizeof(label),
                      "predictFromXYZ(%.3f, %.3f, 3.6)", p.x, p.y);
        auto v = g_model.predictFromXYZ(p.x, p.y, 3.6f);
        printVoltages(label, v);
    }
    printf("\n");

    // 验证输出不为全零, 且在有效范围内
    auto v = g_model.predictFromXYZ(0.0f, 0.0f, 3.6f);
    bool allNonZero = (std::fabs(v[0]) > 1e-6f || std::fabs(v[1]) > 1e-6f ||
                       std::fabs(v[2]) > 1e-6f || std::fabs(v[3]) > 1e-6f);
    bool inRange = checkRange(v);
    printf("  输出非全零: %s\n", allNonZero ? ok() : fail());
    printf("  值域 [0,14]: %s\n\n", inRange ? ok() : fail());

    return allNonZero && inRange ? 1 : 0;
}

/// 测试 3: 俯仰/方位角接口 predict()
static int test_predict_angles() {
    printf("--- Test 3: predict(俯仰角, 方位角) ---\n");

    struct { float th, ph; const char* desc; } angles[] = {
        {45.0f,   0.0f,  "俯仰45°, 方位0°" },
        {30.0f,  90.0f,  "俯仰30°, 方位90°" },
        {60.0f, 180.0f,  "俯仰60°, 方位180°" },
        {90.0f,   0.0f,  "俯仰90° (天顶)" },
    };

    for (auto& a : angles) {
        char label[64];
        std::snprintf(label, sizeof(label),
                      "predict(%.0f, %.0f)", a.th, a.ph);
        auto v = g_model.predict(a.th, a.ph);
        printVoltages(label, v);
    }
    printf("\n");

    // 天顶方向 (theta=90°) 应该输出对称的结果
    auto v = g_model.predict(90.0f, 0.0f);
    bool inRange = checkRange(v);
    printf("  天顶方向值域 [0,14]: %s\n\n", inRange ? ok() : fail());

    return inRange ? 1 : 0;
}

/// 测试 4: phasesToVoltages 相位→电压转换
static int test_phase_to_voltage() {
    printf("--- Test 4: phasesToVoltages 相位→电压转换 ---\n");

    // ① 典型相位值
    std::vector<float> phases1 = {100.0f, 200.0f, 300.0f, 350.0f};
    auto v1 = resmlp::ResMLP::phasesToVoltages(phases1);
    printVoltages("phases={100,200,300,350}°", v1);

    // ② 小于100°的相位应自动加360°
    std::vector<float> phases2 = {45.0f, 90.0f, 180.0f, 270.0f};
    auto v2 = resmlp::ResMLP::phasesToVoltages(phases2);
    printVoltages("phases={45,90,180,270}°", v2);

    // ③ 0° 相位
    std::vector<float> phases3 = {0.0f, 0.0f, 0.0f, 0.0f};
    auto v3 = resmlp::ResMLP::phasesToVoltages(phases3);
    printVoltages("phases={0,0,0,0}°", v3);

    // ④ 自定义系数
    float customCoeffs[4][3] = {
        {-1.0f, 40.0f, 90.0f},
        {-1.0f, 40.0f, 90.0f},
        {-1.0f, 40.0f, 90.0f},
        {-1.0f, 40.0f, 90.0f},
    };
    std::vector<float> phases4 = {180.0f, 180.0f, 180.0f, 180.0f};
    auto v4 = resmlp::ResMLP::phasesToVoltages(phases4, customCoeffs);
    printVoltages("自定义系数 phase=180°", v4);

    printf("\n");

    // 验证: 所有输出在 [0, 14] 范围内且非全零
    bool inRange = checkRange(v1) && checkRange(v2) && checkRange(v3) && checkRange(v4);
    bool nonZero = (std::fabs(v1[0]) > 1e-6f || std::fabs(v2[0]) > 1e-6f);
    printf("  值域 [0,14]: %s\n", inRange ? ok() : fail());
    printf("  非全零:     %s\n\n", nonZero ? ok() : fail());

    return inRange && nonZero ? 1 : 0;
}

/// 测试 5: 集成稳定性测试
static int test_stress() {
    printf("--- Test 5: 稳定性测试 ---\n");

    int failCount = 0;

    // 50 次随机坐标调用
    for (int i = 0; i < 50; i++) {
        float x = (float)(rand() % 600 - 300) / 1000.0f;  // [-0.300, 0.299]
        float y = (float)(rand() % 600 - 300) / 1000.0f;
        auto v = g_model.predictFromXYZ(x, y, 3.6f);
        if (v.size() < 4 || !checkRange(v)) {
            printf("  ！异常: x=%.3f y=%.3f  size=%zu\n", x, y, v.size());
            failCount++;
        }
    }

    // 20 次角度调用
    for (int i = 0; i < 20; i++) {
        float th = (float)(rand() % 9000) / 100.0f;  // [0, 90)
        float ph = (float)(rand() % 36000) / 100.0f; // [0, 360)
        auto v = g_model.predict(th, ph);
        if (v.size() < 4 || !checkRange(v)) failCount++;
    }

    printf("  随机测试: 70/70 次调用\n");
    printf("  异常: %d\n\n", failCount);
    printf("  [ %s ]\n\n", failCount == 0 ? ok() : fail());

    return (failCount == 0) ? 1 : 0;
}

// ============================================================
//  主函数
// ============================================================

int main() {
    printf("============================================================\n");
    printf("  ResMLP 推理准确性测试\n");
    printf("============================================================\n\n");

    // ---- 1. 加载模型 ----
    printf("--- 步骤 1: 权重加载 ---\n");
    if (!g_model.load("model/ResMLP.weights")) {
        fprintf(stderr, "[FAIL] 模型加载失败\n");
        return 1;
    }
    printf("  加载状态: %s\n\n", g_model.isLoaded() ? "OK" : "FAIL");

    // ---- 2. 执行测试 ----
    struct { int (*fn)(); const char* name; } tests[] = {
        { test_known_point,       "已知点精度验证" },
        { test_multi_points,      "多坐标点推理"   },
        { test_predict_angles,    "俯仰/方位角推理" },
        { test_phase_to_voltage,  "相位→电压转换"  },
        { test_stress,            "稳定性测试"      },
    };

    size_t numTests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0, failed = 0;

    for (size_t i = 0; i < numTests; i++) {
        printf("============================================================\n");
        printf("  Test %zu/%zu: %s\n", i + 1, numTests, tests[i].name);
        printf("============================================================\n");
        int ret = tests[i].fn();
        if (ret > 0) passed++;
        else         failed++;
    }

    // ---- 3. 汇总 ----
    printf("============================================================\n");
    printf("  测试汇总\n");
    printf("============================================================\n");
    printf("  通过: %d / %zu\n", passed, numTests);
    printf("  失败: %d / %zu\n", failed, numTests);
    printf("============================================================\n\n");

    return (failed > 0) ? 1 : 0;
}
