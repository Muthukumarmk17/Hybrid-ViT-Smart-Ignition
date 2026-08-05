ViT-Ride

A Hybrid Vision Transformer Framework for Enhancing Two-Wheeler Safety via Smart Ignition Control

ViT-Ride is a real-time AI + IoT safety system for two-wheelers. It uses a Hybrid Vision Transformer (ViT + CNN) model to detect helmet usage from a live webcam feed, combined with an MQ-3 alcohol sensor on an ESP32 to monitor rider sobriety. The vehicle's ignition is enabled only when both conditions are safe — helmet worn and no alcohol detected — with a smart 60-second shutdown window for graceful failsafe handling.


🚦 Problem

Road accidents from helmet negligence and drunk driving remain a major safety issue, especially where two-wheeler usage is high. Manual enforcement (checkpoints, awareness drives) is inconsistent and reactive. Existing helmet/alcohol detection systems typically only raise alerts — they don't actually prevent the vehicle from starting.

💡 Solution

ViT-Ride enforces safety before the vehicle can move:

A Hybrid ViT-CNN model classifies live video as Helmet / No Helmet, fusing global transformer attention with local CNN features for robustness across lighting, angles, and occlusion.
An MQ-3 gas sensor wired to an ESP32 continuously samples breath alcohol levels.
Ignition is enabled only when Helmet = Yes AND Alcohol = Safe.
If either condition fails mid-ride, a 60-second countdown (OLED + buzzer alerts) gives the rider a chance to correct the issue before the motor is forcibly shut off.

.
📊 Dataset
Trained on a custom dataset of ~4,200 images (Helmet / No Helmet), 80/20 train-val split, with augmentation (flip, rotation, brightness/contrast, Gaussian blur) for real-world robustness.
