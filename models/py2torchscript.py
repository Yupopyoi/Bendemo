# pip uninstall -y torch torchvision torchaudio
# pip install --index-url https://download.pytorch.org/whl/cu126 torch==2.8.0 torchvision==0.23.0+cu126 torchaudio==2.8.0+cu126
# pip install ultralytics

from ultralytics import YOLO

m = YOLO("yolov10l.pt")

# 重要：GPU上でエクスポート（定数もCUDAで焼かれる）
m.export(format="torchscript", device=0, imgsz=640)   # device=0=CUDA:0
# 出力パスは通常 runs/export/weights/best.torchscript など

import torch
ts = torch.jit.load("yolov10l.torchscript", map_location="cuda:0")  # ← CUDAでロード
ts.eval()
x = torch.randn(1,3,640,640, device="cuda:0")
with torch.inference_mode():
    y = ts(x)
print("OK:", isinstance(y, (tuple, list, torch.Tensor)))
# ここで例外が出なければ .torchscript は CUDA 定数で固まっています