"""
detect.py — Smart Medicine Cam Monitor
=======================================
Flow:
  1. Polls Blynk V2 every 2 s — ESP32 writes 1 when medicine dispensed
  2. When V2 == 1 → opens camera, watches for hand-to-mouth gesture
  3. If detected  → writes V9=1  (Camera: Taken)   + V3="Medicine Taken"
  4. If not (20s) → writes V9=0  (Camera: Missed)  + V3="Missed Dose"
  5. Also tracks IR1/IR2 state via Blynk V11/V12 (optional shadow pins)
  Runs headlessly in background; camera window shown during detection window.
"""

import sys, io, time, math, threading, requests, cv2
import mediapipe as mp

# ── Windows console UTF-8 fix ─────────────────────────────
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

# ── BLYNK CONFIG ─────────────────────────────────────────
TOKEN   = "9IZnV6T6HxYj6A4wMb6dM-aoUAo0J8eU"
BASE    = "https://blynk.cloud/external/api"

# Virtual pins (must match ESP32)
V_DISPENSED = "V2"     # ESP32 writes 1 when dispensed; cam resets to 0
V_CAM_RESULT= "V9"     # cam writes 1=taken, 0=missed
V_STATUS    = "V3"     # shared human-readable status
V_PUSH      = "V10"    # push notification string

# Detection window (seconds) — should be <= ESP32's CAM_TIMEOUT_MS/1000
CAM_WINDOW_SEC  = 18
COOLDOWN_SEC    = 8    # min gap between two "Taken" events
HAND_FACE_DIST  = 90   # pixel threshold for hand→mouth proximity

# ── BLYNK HELPERS ────────────────────────────────────────
def blynk_get(pin):
    try:
        r = requests.get(f"{BASE}/get?token={TOKEN}&{pin}", timeout=4)
        return r.text.strip().strip('"')
    except Exception as e:
        print(f"[BLYNK GET ERR] {e}")
        return None

def blynk_set(pin, value):
    try:
        requests.get(f"{BASE}/update?token={TOKEN}&{pin}={value}", timeout=4)
    except Exception as e:
        print(f"[BLYNK SET ERR] {e}")

def blynk_status(msg):
    blynk_set(V_STATUS, requests.utils.quote(msg))
    print(f"[STATUS] {msg}")

def blynk_push(msg):
    blynk_set(V_PUSH, requests.utils.quote(msg))
    print(f"[PUSH] {msg}")

# ── MEDIAPIPE SETUP ───────────────────────────────────────
mp_hands = mp.solutions.hands
mp_face  = mp.solutions.face_detection
mp_draw  = mp.solutions.drawing_utils

def euclidean(p1, p2):
    return math.sqrt((p1[0]-p2[0])**2 + (p1[1]-p2[1])**2)

# ── CAMERA DETECTION LOGIC ───────────────────────────────
def run_camera_detection():
    """
    Opens webcam, runs Mediapipe hands + face detection.
    Returns True  if medicine-taking gesture detected within CAM_WINDOW_SEC,
            False if not detected (missed dose).
    """
    print("[CAM] Starting camera detection window...")
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("[CAM] ERROR: Cannot open camera — defaulting to IR result")
        return None  # let ESP32 fall back to IR

    hands_det = mp_hands.Hands(
        min_detection_confidence=0.7,
        min_tracking_confidence=0.6,
        max_num_hands=2
    )
    face_det  = mp_face.FaceDetection(min_detection_confidence=0.7)

    start      = time.time()
    detected   = False
    last_detect= 0
    confirm_count = 0      # need N consecutive detections for robustness
    abort      = False     # set True if ESP32 resets V2 (IR confirmed first)

    # --- Background thread: watch V2 for early-exit signal ---
    def watch_v2():
        nonlocal abort
        while not abort and (time.time() - start) < CAM_WINDOW_SEC:
            v = blynk_get(V_DISPENSED)
            if v == "0":
                print("[CAM] V2 reset by ESP32 (IR confirmed) — closing camera early")
                abort = True
                break
            time.sleep(2)
    import threading
    watcher = threading.Thread(target=watch_v2, daemon=True)
    watcher.start()

    while True:
        elapsed = time.time() - start
        if elapsed > CAM_WINDOW_SEC or abort:
            break

        ret, frame = cap.read()
        if not ret:
            break

        h, w, _ = frame.shape
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        rgb.flags.writeable = False

        h_res = hands_det.process(rgb)
        f_res = face_det.process(rgb)

        rgb.flags.writeable = True
        frame_draw = frame.copy()

        # ── time bar ───────────────────────────────────────
        remain = int(CAM_WINDOW_SEC - elapsed)
        bar_w  = int((elapsed / CAM_WINDOW_SEC) * w)
        cv2.rectangle(frame_draw, (0, h-8), (bar_w, h), (0, 200, 100), -1)
        cv2.putText(frame_draw, f"Window: {remain}s", (10, h-14),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, (255,255,255), 1)

        this_frame_detected = False

        if h_res.multi_hand_landmarks and f_res.detections:
            for handLms in h_res.multi_hand_landmarks:
                mp_draw.draw_landmarks(frame_draw, handLms,
                                       mp_hands.HAND_CONNECTIONS)
                # Index fingertip (landmark 8) as hand proxy
                x_hand = int(handLms.landmark[8].x * w)
                y_hand = int(handLms.landmark[8].y * h)
                cv2.circle(frame_draw, (x_hand, y_hand), 9, (0, 255, 80), -1)

                for faceDet in f_res.detections:
                    bbox = faceDet.location_data.relative_bounding_box
                    # Mouth is roughly at 80% of face height
                    x_mouth = int((bbox.xmin + bbox.width * 0.5) * w)
                    y_mouth = int((bbox.ymin + bbox.height * 0.75) * h)
                    cv2.circle(frame_draw, (x_mouth, y_mouth), 9, (255, 80, 0), -1)

                    dist = euclidean((x_hand, y_hand), (x_mouth, y_mouth))

                    if dist < HAND_FACE_DIST:
                        this_frame_detected = True

                    # Visual line
                    color = (0, 255, 0) if dist < HAND_FACE_DIST else (80, 80, 255)
                    cv2.line(frame_draw, (x_hand, y_hand),
                             (x_mouth, y_mouth), color, 2)
                    cv2.putText(frame_draw, f"dist:{int(dist)}", 
                                (x_hand+5, y_hand-10),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.45,
                                (255,255,255), 1)

        if this_frame_detected:
            confirm_count += 1
        else:
            confirm_count = max(0, confirm_count - 1)

        # Require 4 consecutive positive frames (≈ 0.13 s at 30 fps)
        if confirm_count >= 4 and not detected:
            detected   = True
            last_detect= time.time()
            print("[CAM] *** MEDICINE TAKEN DETECTED ***")

        # OSD
        label  = "TAKEN MEDICINE" if detected else ("DETECTING..." if this_frame_detected else "WATCHING...")
        color  = (0, 220, 0) if detected else (0, 200, 220)
        cv2.putText(frame_draw, label, (12, 45),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.0, color, 3)

        cv2.imshow("AI Medicine Detection", frame_draw)
        if cv2.waitKey(1) == 27:  # ESC to abort
            break

    hands_det.close()
    face_det.close()
    cap.release()
    cv2.destroyAllWindows()
    return detected

# ── MAIN LOOP ────────────────────────────────────────────
print("[DETECT] Smart Medicine Monitor started.")
print("[DETECT] Polling Blynk V2 for dispense signal...")

last_taken_time = 0

while True:
    val = blynk_get(V_DISPENSED)

    if val == "1":
        print("[DETECT] Dispense signal received! Starting detection...")

        # --- Run camera in main thread (OpenCV needs main thread on Windows)
        taken = run_camera_detection()

        if taken is None:
            # Camera failed — let ESP32 handle via IR timeout
            print("[DETECT] Camera unavailable, skipping result write")
        elif taken:
            now_t = time.time()
            if now_t - last_taken_time > COOLDOWN_SEC:
                blynk_set(V_CAM_RESULT, "1")
                blynk_status("Medicine%20Taken")
                blynk_push("Medicine%20taken%20-%20camera%20confirmed!")
                last_taken_time = now_t
                print("[DETECT] Result: TAKEN -> written V9=1")
        else:
            blynk_set(V_CAM_RESULT, "0")
            blynk_status("Missed%20Dose")
            blynk_push("MISSED!%20Medicine%20not%20taken%20-%20check%20patient")
            print("[DETECT] Result: MISSED -> written V9=0")

        # Reset dispense flag so we don't re-trigger
        blynk_set(V_DISPENSED, "0")
        print("[DETECT] V2 reset. Resuming polling...")

    else:
        # Show idle indicator in console
        print(f"[DETECT] Idle... V2={val}", end="\r")

    time.sleep(2)