#!/usr/bin/env python3
"""
=====================================================================
 LightResNet / ResMLP — .pth 权重导出脚本
 将 PyTorch 的 state_dict 导出为 C++ 可加载的二进制格式 (.weights)
=====================================================================

用法:
    python tools/export_weights.py                    \\
        --input  model/ResMLP.pth                     \\
        --output model/ResMLP.weights

输出格式:
    [magic:        uint32]   = 0x504D4C52  ("RMLP")
    [num_tensors:  uint32]
    ┌─ 每个张量:
    │  [name_len:  uint32]   N
    │  [name:      char[N]]  无结尾 null
    │  [data_len:  uint32]   M  (元素个数)
    │  [data:      float[M]]  原始 float32 数据
    └─
"""

import os
import sys
import struct
import argparse

import numpy as np
import torch
import torch.nn as nn


# ==============================================================
# 1. 模型结构（必须与训练时保持一致）
# ==============================================================

class BasicBlock(nn.Module):
    def __init__(self, in_channels, out_channels, use_shortcut=True):
        super().__init__()

        self.block = nn.Sequential(
            nn.Linear(in_channels, out_channels),
            nn.BatchNorm1d(out_channels),
            nn.Mish(),
            nn.Linear(out_channels, out_channels),
            nn.BatchNorm1d(out_channels)
        )

        if use_shortcut:
            self.shortcut = (
                nn.Linear(in_channels, out_channels)
                if in_channels != out_channels
                else nn.Identity()
            )
        else:
            self.shortcut = None

        self.mish = nn.Mish()

    def forward(self, x):
        if self.shortcut is None:
            out = self.block(x)
        else:
            out = self.block(x) + self.shortcut(x)

        return self.mish(out)


class LightResNet(nn.Module):
    def __init__(self, input_dim=20, output_dim=8):
        super().__init__()

        self.layers = nn.Sequential(
            # -------- 前段：不加残差 --------
            BasicBlock(input_dim, 64,  use_shortcut=False),
            BasicBlock(64,        128, use_shortcut=False),

            # -------- 中段：加残差 --------
            BasicBlock(128, 256, use_shortcut=True),
            BasicBlock(256, 128, use_shortcut=True),

            # -------- 压缩段：不加残差 --------
            BasicBlock(128, 64, use_shortcut=False),
            BasicBlock(64,  32, use_shortcut=False),

            nn.Dropout(0.2),
            nn.Linear(32, output_dim)
        )

    def forward(self, x):
        return self.layers(x)


# ==============================================================
# 2. 导出函数
# ==============================================================

def export_weights(state_dict, output_path):
    """将 state_dict 导出为自定义二进制格式."""
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)

    with open(output_path, "wb") as f:
        # --- 文件头 ---
        f.write(struct.pack("<I", 0x504D4C52))  # magic "RMLP"
        f.write(struct.pack("<I", len(state_dict)))  # 张量数量

        for name, tensor in state_dict.items():
            # 只导出 float 类型（过滤 int/bool 等）
            if tensor.dtype not in (torch.float32, torch.float64):
                print(f"  跳过非浮点参数: {name}  dtype={tensor.dtype}")
                continue

            data = tensor.cpu().detach().numpy().astype(np.float32).ravel()

            name_bytes = name.encode("utf-8")
            f.write(struct.pack("<I", len(name_bytes)))  # name_len
            f.write(name_bytes)  # name
            f.write(struct.pack("<I", len(data)))  # data_len
            f.write(data.tobytes())  # data

    total = len(state_dict)
    size_kb = os.path.getsize(output_path) / 1024.0
    print(f"\n[OK] 导出完成: {output_path}")
    print(f"  {total} 个张量, {size_kb:.1f} KB")


# ==============================================================
# 3. 主入口
# ==============================================================

def main():
    parser = argparse.ArgumentParser(
        description="将 LightResNet .pth 权重导出为 C++ 可加载的 .weights 格式"
    )
    parser.add_argument(
        "--input", "-i",
        default=os.path.join(
            os.path.dirname(__file__), "..", "model", "ResMLP.pth"
        ),
        help="输入的 .pth 文件路径 (默认: ../model/ResMLP.pth)",
    )
    parser.add_argument(
        "--output", "-o",
        default=os.path.join(
            os.path.dirname(__file__), "..", "model", "ResMLP.weights"
        ),
        help="输出的 .weights 文件路径 (默认: ../model/ResMLP.weights)",
    )
    args = parser.parse_args()

    input_path = os.path.abspath(args.input)
    output_path = os.path.abspath(args.output)

    if not os.path.exists(input_path):
        print(f"错误: 输入文件不存在 — {input_path}", file=sys.stderr)
        sys.exit(1)

    # 加载 state_dict（不依赖模型定义也能加载纯 state_dict）
    print(f"加载 .pth 文件: {input_path}")
    state_dict = torch.load(input_path, map_location="cpu", weights_only=True)

    # 如果是完整模型而不是 state_dict，则提取 state_dict
    if hasattr(state_dict, "state_dict"):
        print("检测到完整模型对象，提取 state_dict...")
        state_dict = state_dict.state_dict()

    # 过滤掉非浮点参数
    float_state_dict = {
        k: v for k, v in state_dict.items()
        if v.dtype in (torch.float32, torch.float64)
    }

    skipped = len(state_dict) - len(float_state_dict)
    if skipped > 0:
        print(f"  跳过了 {skipped} 个非浮点参数")

    export_weights(float_state_dict, output_path)


if __name__ == "__main__":
    main()
