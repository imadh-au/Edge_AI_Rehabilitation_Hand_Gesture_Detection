# 🦾 Edge AI Arm Movement Quality Assessment
### Real-time rehabilitation exercise monitoring on Arduino Nano 33 BLE Rev2

![Accuracy](https://img.shields.io/badge/Accuracy-98.6%25-brightgreen)
![Inference](https://img.shields.io/badge/Inference-1ms-blue)
![RAM](https://img.shields.io/badge/Peak%20RAM-1.7KB-orange)
![Flash](https://img.shields.io/badge/Flash-18.9KB-yellow)
![Platform](https://img.shields.io/badge/Platform-Arduino%20Nano%2033%20BLE%20Rev2-teal)
![License](https://img.shields.io/badge/License-MIT-lightgrey)

---

## 📌 Overview

This project is a wearable **Edge AI** system that classifies arm movement quality in real time for upper limb rehabilitation patients. Built during a research internship at **Digital Pakistan Lab (DPL)**, Rawalpindi, Pakistan.

The system runs entirely on a **$25 Arduino Nano 33 BLE Rev2** microcontroller — no cloud, no internet, no therapist required. A patient straps the board to their hand, performs arm raise exercises, and receives immediate feedback on movement quality after every repetition.

---

## 🗂️ Repository Contents

```
├── Quality_Model_V01_Arduino_Code/    Arduino inference firmware
├── DataSet.zip                        Labelled 6-axis IMU dataset (230 samples)
├── Imadh_Rehab_EdgeAI_Report.pdf      Full research report
└── ei-quality_model-arduino-1.0.1-impulse-#1.zip   Edge Impulse Arduino library
```

---

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                  Arduino Nano 33 BLE Rev2                    │
│                                                             │
│  BMI270 IMU (59Hz)                                          │
│  ax, ay, az, gx, gy, gz                                     │
│        │                                                    │
│        ▼                                                    │
│  Low Pass Filter (α=0.2, gyroscope only)                    │
│        │                                                    │
│        ▼                                                    │
│  Spectral Analysis Block (FFT=32, 126 features)             │
│        │                                                    │
│   ┌────┴────┐                                               │
│   ▼         ▼                                               │
│ Neural    Physics                                           │
│ Network   Threshold                                         │
│ (quality) (position)                                        │
│   └────┬────┘                                               │
│        ▼                                                    │
│  Hybrid Override Logic                                      │
│        │                                                    │
│        ▼                                                    │
│  Serial Output → GESTURE, QUAL, SCORE, FEEDBACK             │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔬 What It Does

- Detects arm movement quality during rehabilitation exercises in real time
- Classifies each repetition into one of five clinically meaningful categories
- Applies a **hybrid physics-AI architecture** — the neural network assesses motion quality while physics-based accelerometer thresholds detect arm position and incomplete raises
- Outputs a quality score and patient feedback message over Serial after every rep
- Tracks session statistics: total reps, good reps, average score, success rate

---

## 💡 Key Research Contribution

> A machine learning model alone **cannot detect incomplete raises** — because a smooth incomplete raise produces an identical motion signature to a good raise. The only difference is where the arm stops, not how it moves.

**Solution:** A hybrid physics-AI architecture that uses accelerometer gravity sensing to check the arm endpoint *independently* of the neural network, overriding the model when the arm is detected in the halfway zone.

This finding demonstrates that **knowing when NOT to use AI** is as important as the AI itself.

---

## 🏷️ Movement Quality Classes

| Class | Description | Score |
|-------|-------------|-------|
| `good_raise` | Smooth, controlled raise to full extension (2–3 sec) | 100 |
| `fast_raise` | Rushed raise completed under 1 second | 50 |
| `jerky_raise` | Raise with 2–3 deliberate stops mid-movement | 40 |
| `incomplete_raise` | Raise that stops at ~45° without reaching horizontal | 20 |
| `arm_flat` | Arm held steady at full horizontal extension | 100 |

---

## 📊 Model Performance

### Overall Metrics (Validation Set)

| Metric | Value |
|--------|-------|
| Accuracy | **98.6%** |
| Loss | 0.04 |
| Area under ROC curve | 1.00 |
| Weighted Precision | 0.99 |
| Weighted Recall | 0.99 |
| Weighted F1-score | 0.99 |

### Per-Class Accuracy

| Class | Accuracy | F1 Score |
|-------|----------|----------|
| arm_flat | 100% | 1.00 |
| fast_raise | 100% | 1.00 |
| jerky_raise | 100% | 1.00 |
| good_raise | 97.1% | 0.96 |
| incomplete_raise | 95.2% | 0.96 |

### On-Device Performance (Arduino Nano 33 BLE Rev2)

| Metric | Value | Available |
|--------|-------|-----------|
| Inference time | **1 ms** | — |
| Peak RAM | **1.7 KB** | 256 KB |
| Flash usage | **18.9 KB** | 1,024 KB |

---

## 🛠️ Hardware Required

- Arduino Nano 33 BLE Rev2
- Elastic strap or medical tape (for wrist mounting)
- USB-A to Micro-USB cable
- PC with Arduino IDE 2.x and Chrome browser

---

## ⚙️ How to Run

### Step 1 — Install the Edge Impulse Library

1. Download `ei-quality_model-arduino-1.0.1-impulse-#1.zip` from this repository
2. Open Arduino IDE → Sketch → Include Library → Add .ZIP Library
3. Select the downloaded zip file

### Step 2 — Upload the Inference Sketch

1. Open `Quality_Model_V01_Arduino_Code/` in Arduino IDE
2. Select board: **Tools → Board → Arduino Mbed OS Nano Boards → Arduino Nano 33 BLE**
3. Select the correct COM port
4. Click Upload *(first compile takes 15–30 minutes — normal for this board)*

### Step 3 — Mount the Board

```
Mount the board on the back of your hand using an elastic strap
USB port facing toward your fingers
Board lying flat against the skin — firm mount is critical
```

### Step 4 — Run

1. Open Serial Monitor at **115200 baud**
2. Hold arm straight down — system waits for arm_down confirmation
3. Raise arm — system collects 3-second window automatically
4. Result prints after each rep

---

## 📡 Serial Output Format

```
STATUS:arm_down — ready for next rep
GESTURE:arm_down,CONF:100
STATUS:movement detected — starting classification
STATUS:collecting...
CONF | good_raise:97.1% fast_raise:1.2% ...
REP:1,QUAL:good_raise,SCORE:100,AX:0.34
STATE:drawing_good
FEEDBACK:Great movement! Keep going!
STATUS:cooldown 2 seconds...
```

Every 5 reps a session summary prints:
```
================================
       SESSION SUMMARY
================================
Total reps:    5
Good reps:     4
Avg score:     84
Success rate:  80%
================================
```

---

## 📐 Physics Threshold Reference

Derived from measured accelerometer ax values with board mounted on hand:

| Arm Position | ax Range | Classification |
|-------------|----------|----------------|
| Arm straight down | +0.98 to +1.00 g | arm_down (ax > 0.70) |
| Arm fully horizontal | 0.00 to +0.25 g | arm_flat (ax < 0.25) |
| Halfway (45°) | +0.50 to +0.65 g | incomplete override |
| In motion | +0.25 to +0.70 g | model classification zone |

---

## 🧠 Model Configuration (Edge Impulse)

| Setting | Value |
|---------|-------|
| Window size | 3000 ms |
| Window stride | 200 ms |
| Sampling frequency | 59 Hz |
| Input axes | ax, ay, az, gx, gy, gz |
| Processing block | Spectral Analysis (FFT=32) |
| Features generated | 126 |
| Architecture | Dense(32)→Drop(0.3)→Dense(16)→Drop(0.2)→Dense(8)→Softmax(5) |
| Training cycles | 300 |
| Learning rate | 0.0005 |
| Quantisation | int8 |

---

## 📁 Dataset

The `DataSet.zip` contains 230 labelled IMU samples across 5 classes collected from 3 subjects. Each sample is 2–3 seconds of 6-axis data at 59 Hz.

> ⚠️ **Note:** This dataset was collected from healthy subjects during a research internship. It is provided for reproducibility purposes only and is not validated for clinical use.

To retrain the model:
1. Create a project on [Edge Impulse](https://edgeimpulse.com)
2. Upload the dataset samples with their labels
3. Configure the impulse as described above
4. Train and export as Arduino library

---

## 📄 Research Report

The full research report `Imadh_Rehab_EdgeAI_Report.pdf` covers:

- Literature review (14 papers on IMU rehabilitation, Edge AI, TinyML)
- Complete system design and methodology
- Results and analysis with all performance tables
- Discussion of the hybrid physics-AI architecture contribution
- Future work including FSR grip strength integration

---

## 🔮 Future Work

- **FSR pressure sensor** — grip strength measurement via force-sensitive resistor on A0 pin
- **BLE mobile app** — wireless session data to smartphone, eliminating USB cable
- **Patient calibration routine** — automatic threshold derivation at system startup
- **Clinical validation** — testing with actual rehabilitation patients
- **Extended exercise set** — shoulder rotation, elbow flexion, wrist range of motion

---

## 👨‍💻 Author

**Imadh Aboo Ubaith**
BE Computer Engineering — NUST EME (College of Electrical and Mechanical Engineering)
Research Internship at Digital Pakistan Lab (DPL), Rawalpindi, Pakistan
June – August 2026

---

## 🎓 Supervised by

**Dr. Ali Hassan**
Digital Pakistan Lab (DPL), Rawalpindi, Pakistan

---

## 📜 License

This project is licensed under the MIT License.
Free to use for research and educational purposes with attribution.

---

*Built with Edge Impulse · Arduino · BMI270 IMU · Python*
