"""
SMART HELMET DETECTION + ESP32 ALCOHOL CONTROL SYSTEM
------------------------------------------------------
Requirements:
pip install torch torchvision timm opencv-python pillow numpy matplotlib pyserial
Usage:
python smart_helmet.py --mode webcam
"""

import torch
import torch.nn as nn
import cv2
import timm
from torchvision import transforms
import time
import os
import argparse
import serial
import numpy as np
from pathlib import Path

# =====================================================
# CONFIGURATION
# =====================================================
CONFIG = {
    'img_size': 224,
    'num_classes': 2,
    'classes': ['Helmet', 'No_helmet'],
    'model_path': r'C:\Users\DELL\Downloads\best_helmet_model.pth',
    'confidence_threshold': 0.7,
    'capture_interval': 5,  # seconds
    'serial_port': 'COM3', # change this to your ESP32 port
    'baud_rate': 115200
}

# =====================================================
# SETUP DEVICE & SERIAL
# =====================================================
device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
print(f"✅ Using device: {device}")

# Try to connect to ESP32
esp32 = None
try:
    esp32 = serial.Serial(CONFIG['serial_port'], CONFIG['baud_rate'], timeout=1)
    print(f"🔗 Connected to ESP32 on {CONFIG['serial_port']}")
except Exception as e:
    print(f"⚠️ Could not connect to ESP32: {e}")
    print("Helmet detection will still run without sending data.")

# =====================================================
# HYBRID ViT MODEL
# =====================================================
class HybridViTModel(nn.Module):
    def __init__(self, num_classes=2, pretrained=False):
        super(HybridViTModel, self).__init__()
        self.vit = timm.create_model('vit_base_patch16_224', pretrained=pretrained)
        vit_embed_dim = self.vit.embed_dim
        
        self.cnn_branch = nn.Sequential(
            nn.Conv2d(3, 64, 7, stride=2, padding=3),
            nn.BatchNorm2d(64),
            nn.ReLU(True),
            nn.MaxPool2d(3, stride=2, padding=1),
            nn.Conv2d(64, 128, 3, padding=1),
            nn.BatchNorm2d(128),
            nn.ReLU(True),
            nn.MaxPool2d(2, 2),
            nn.Conv2d(128, 256, 3, padding=1),
            nn.BatchNorm2d(256),
            nn.ReLU(True),
            nn.AdaptiveAvgPool2d((1, 1))
        )

        self.vit.head = nn.Identity()
        fusion_dim = vit_embed_dim + 256
        self.fusion = nn.Sequential(
            nn.Linear(fusion_dim, 512),
            nn.ReLU(True),
            nn.Dropout(0.3),
            nn.Linear(512, 256),
            nn.ReLU(True),
            nn.Dropout(0.2),
            nn.Linear(256, num_classes)
        )

    def forward(self, x):
        vit_features = self.vit(x)
        cnn_features = self.cnn_branch(x).flatten(1)
        combined = torch.cat([vit_features, cnn_features], dim=1)
        return self.fusion(combined)

# =====================================================
# LOAD MODEL
# =====================================================
def load_model():
    print("📦 Loading model...")
    model = HybridViTModel(num_classes=CONFIG['num_classes'], pretrained=False)
    if os.path.exists(CONFIG['model_path']):
        checkpoint = torch.load(CONFIG['model_path'], map_location=device)
        model.load_state_dict(checkpoint['model_state_dict'])
        print("✅ Model loaded successfully!")
    else:
        print("❌ Model file not found!")
        exit()
    model.to(device)
    model.eval()
    return model

# =====================================================
# IMAGE TRANSFORM
# =====================================================
transform = transforms.Compose([
    transforms.ToPILImage(),
    transforms.Resize((CONFIG['img_size'], CONFIG['img_size'])),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406],
                         std=[0.229, 0.224, 0.225])
])

# =====================================================
# PREDICT FUNCTION
# =====================================================
def predict_frame(model, frame):
    frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    img_tensor = transform(frame_rgb).unsqueeze(0).to(device)
    with torch.no_grad():
        outputs = model(img_tensor)
        probs = torch.softmax(outputs, dim=1)
        confidence, predicted = torch.max(probs, 1)
    label = CONFIG['classes'][predicted.item()]
    conf = confidence.item()
    return label, conf

# =====================================================
# SEND RESULT TO ESP32
# =====================================================
def send_to_esp32(label):
    if esp32 is None:
        return
    try:
        if label == "Helmet":
            esp32.write(b"HELMET:1\n")
        else:
            esp32.write(b"HELMET:0\n")
    except Exception as e:
        print(f"⚠️ Error sending data to ESP32: {e}")

# =====================================================
# DRAW RESULTS ON FRAME
# =====================================================
def draw_prediction(frame, label, confidence, fps=None):
    color = (0, 255, 0) if label == 'Helmet' else (0, 0, 255)
    status = "SAFE ✓" if label == 'Helmet' else "WARNING ⚠"
    cv2.rectangle(frame, (10, 10), (frame.shape[1]-10, frame.shape[0]-10), color, 3)
    cv2.putText(frame, f"{status}: {label}", (30, 40),
                cv2.FONT_HERSHEY_SIMPLEX, 1.0, color, 3)
    cv2.putText(frame, f"Conf: {confidence*100:.1f}%", (30, 75),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255,255,255), 2)
    if fps:
        cv2.putText(frame, f"FPS: {fps}", (frame.shape[1]-150, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255,255,255), 2)
    return frame

# =====================================================
# WEBCAM MODE (FAST)
# =====================================================
def webcam_mode(model):
    print("\n🎥 Starting Smart Helmet Detection...")
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("❌ Could not access camera")
        return

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    prev_time = time.time()
    last_send_time = time.time()

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        label, conf = predict_frame(model, frame)

        # Send to ESP32 every 1 second
        if time.time() - last_send_time > 1:
            send_to_esp32(label)
            last_send_time = time.time()

        # Calculate FPS
        curr_time = time.time()
        fps = int(1 / (curr_time - prev_time))
        prev_time = curr_time

        frame = draw_prediction(frame, label, conf, fps)
        cv2.imshow("Smart Helmet Detection", frame)

        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()
    if esp32:
        esp32.close()
    print("✅ System stopped safely.")

# =====================================================
# MAIN ENTRY POINT
# =====================================================
def main():
    parser = argparse.ArgumentParser(description='Smart Helmet Detection System')
    parser.add_argument('--mode', type=str, default='webcam', choices=['webcam'])
    args = parser.parse_args()

    model = load_model()

    if args.mode == 'webcam':
        webcam_mode(model)

if __name__ == "__main__":
    print("="*55)
    print("    SMART HELMET DETECTION + ESP32 ALCOHOL SAFETY SYSTEM")
    print("="*55)
    main()
