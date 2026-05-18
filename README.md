# 💊 Smart Medicine Dispenser (MedBox)

An innovative, IoT-enabled automated medicine dispenser system equipped with an AI-powered camera monitor, a responsive neo-brutalist web dashboard, real-time Telegram alerts, and an intelligent medical patient chatbot. It integrates hardware (ESP32) and modern software technologies to ensure precise and timely delivery of medicine to patients, verifying ingestion via computer vision.

## ✨ Features

- **Automated & Manual Dispensing**: Schedule medicines via the dashboard or manually trigger a dose instantly. 
- **AI Camera Verification**: Uses **OpenCV** and **MediaPipe** to detect when the patient picks up the medicine and brings it to their mouth, visually confirming the medicine intake.
- **Real-Time Push Alerts & Telegram Notifications**: Get instant push notifications and Telegram alerts with photo confirmation whenever medicine is successfully taken or if a dose is missed.
- **PWA Web Dashboard**: A "neo-brutalist" style responsive web app displaying live device status, daily medicine intake statistics, a manual override control, and a full cloud-synced history log.
- **Firebase Sync**: Medicine adherence history is backed up automatically via Firebase Realtime Database.
- **Senor Medicine Assistant (AI Chatbot)**: A Telegram chatbot powered by the **Google Gemini 2.5 Flash** model, trained to act as a helpful and compassionate AI Medical Assistant to answer basic disease, medication, and usage questions safely.

---

## 🏗️ System Architecture

The project consists of three main operational environments communicating in sync:

1. **Hardware / IoT Node (ESP32 via Blynk IoT)**
   - Manages physical servos to dispense medicine.
   - Monitors physical IR Sensors at the dispensary shoot.
   - Hosts schedules internally triggered via Blynk Virtual Pins.
   
2. **Web Dashboard (`index.html` + `sw.js` + `manifest.json`)**
   - Built with HTML, CSS (Vanilla), JS, and Chart.js.
   - Connects to **Blynk API** to send dispense signals and read camera/IR sensor status.
   - Installs as a standalone PWA on mobile and desktop.
   
3. **Computer Vision & Alert System (`detect.py` + `telegram_alerts.py`)**
   - Runs in the background (Windows/Linux/Mac).
   - Polls Blynk for dispense events.
   - Triggers the webcam on dispense, uses **MediaPipe** to calculate Euclidean distance between hands and mouth. Submits a photo snapshot to Telegram if successful (intake verified).
   - Reports the physical success or missed status back to the dashboard immediately.

4. **AI Patient Chatbot (`patient_bot.py`)**
   - A Telegram bot integrating with the **Google Gemini API**.
   - Listens for patient queries and responds naturally with a supportive medical persona.

---

## 🛠️ Prerequisites & Installation

### 1. Requirements
Ensure you have the following installed:
- Python 3.8+
- Modern Web Browser (Chrome/Edge/Safari)

### 2. Python Dependencies
Install the required packages using strictly:
```bash
pip install -r reqirements.txt
```
*(Packages include `opencv-python`, `mediapipe`, `requests`, `google-generativeai`)*

### 3. API Keys Setup
The system relies on various API keys for communication. Update the codebase with your keys:
- **Blynk Token**: Add your token into `detect.py` and `index.html`.
- **Firebase Realtime Database**: Paste your config JSON into `index.html`.
- **Telegram Bot Token & Chat ID**: Put them in `telegram_alerts.py` and `patient_bot.py` where required.
- **Gemini API Key**: Provide an AI Studio API key in `patient_bot.py`.

---

## 🚀 Usage Guide

### Starting the Background Services
A convenient startup script is provided for Windows users. Simply double-click on `start.bat` from your file explorer. Alternatively, you can trigger the systems manually:

Start the **AI Camera Detect System**:
```bash
python detect.py
```

Start the **Telegram AI Assistant**:
```bash
python patient_bot.py
```

### Navigating the Web Dashboard
1. Open the project folder and double-click `index.html` (or host it locally).
2. For an optimal experience on mobile, use your browser's **"Add to Home Screen"** function to install the site as a native PWA app.
3. Configure the dosing schedule or press **"Dispense Now"** to test the system manually.
4. When medicine is dispensed, look at the connected webcam to perform the 'pill to mouth' gesture. Wait for the success tone/alert!

---

## 📋 File Layout

* `index.html` — The main frontend web interface.
* `detect.py` — The core logic integrating the Blynk API trigger and OpenCV/Mediapipe inference.
* `patient_bot.py` — The Telegram bot wrapper connecting user chats to Google's Gemini LLM.
* `telegram_alerts.py` — The helper utility handling photo and message dispatch through the Telegram Bot API.
* `sw.js` / `manifest.json` — Progressive Web App metadata and caching setup.
* `start.bat` — One-click launcher for Windows.

---

## ⚠️ Medical Disclaimer
This software is provided for educational and assistive purposes only. It should **not** replace a professional healthcare provider. Please consult a real human doctor or a pharmacist for professional medical advice, diagnoses, or treatments.
