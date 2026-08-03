#!/bin/bash
# ==========================================================================
# download_models.sh — 下载 ONNX 推理模型文件
# ==========================================================================
# Usage: bash scripts/download_models.sh
# 将 MobileNetV2 模型下载到 /root/models/mobilenetv2/ 目录

set -euo pipefail

MODEL_DIR="/root/models/mobilenetv2"
MODEL_FILE="mobilenetv2-7.onnx"
MODEL_URL="https://github.com/onnx/models/raw/main/validated/vision/classification/mobilenet/model/mobilenetv2-7.onnx"

LABEL_DIR="/root"
LABEL_FILE="imagenet_classes.txt"
LABEL_URL="https://raw.githubusercontent.com/pytorch/hub/master/imagenet_classes.txt"

echo "=== RainCppAI Model Downloader ==="
echo ""

# Create model directory
mkdir -p "${MODEL_DIR}"

# Download ONNX model
MODEL_PATH="${MODEL_DIR}/${MODEL_FILE}"
if [ -f "${MODEL_PATH}" ]; then
    echo "[SKIP] Model already exists: ${MODEL_PATH}"
else
    echo "[INFO] Downloading MobileNetV2 ONNX model..."
    wget -q --show-progress -O "${MODEL_PATH}" "${MODEL_URL}"
    echo "[OK] Model saved to: ${MODEL_PATH}"
fi

# Download imagenet class labels
LABEL_PATH="${LABEL_DIR}/${LABEL_FILE}"
if [ -f "${LABEL_PATH}" ]; then
    echo "[SKIP] Labels already exist: ${LABEL_PATH}"
else
    echo "[INFO] Downloading ImageNet class labels..."
    wget -q --show-progress -O "${LABEL_PATH}" "${LABEL_URL}"
    echo "[OK] Labels saved to: ${LABEL_PATH}"
fi

echo ""
echo "=== Done! ==="
echo "Model:  ${MODEL_PATH}"
echo "Labels: ${LABEL_PATH}"
echo ""
echo "You can now restart the server to enable vision recognition."
