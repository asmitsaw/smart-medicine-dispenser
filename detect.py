import sys
import io
import cv2
import mediapipe as mp
import requests
import time
import math

# Fix Windows console encoding for emoji / special chars
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

# 🔥 YOUR BLYNK TOKEN
BLYNK_TOKEN = "9IZnV6T6HxYj6A4wMb6dM-aoUAo0J8eU"

# 🔧 INIT MEDIAPIPE
mp_hands = mp.solutions.hands
mp_face = mp.solutions.face_detection

hands = mp_hands.Hands(min_detection_confidence=0.7)
face = mp_face.FaceDetection(min_detection_confidence=0.7)

cap = cv2.VideoCapture(0)

last_sent_time = 0
cooldown = 5   # seconds

def send_taken():
    print("[TAKEN] Medicine Taken detected!")

    # Send to Blynk
    requests.get(f"https://blynk.cloud/external/api/update?token={BLYNK_TOKEN}&V3=Medicine%20Taken")

def get_distance(p1, p2):
    return math.sqrt((p1[0]-p2[0])**2 + (p1[1]-p2[1])**2)

while True:
    ret, frame = cap.read()
    if not ret:
        break

    h, w, _ = frame.shape
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

    hand_result = hands.process(rgb)
    face_result = face.process(rgb)

    detected = False

    if hand_result.multi_hand_landmarks and face_result.detections:

        # Get hand point
        for handLms in hand_result.multi_hand_landmarks:
            x_hand = int(handLms.landmark[8].x * w)
            y_hand = int(handLms.landmark[8].y * h)

            cv2.circle(frame, (x_hand, y_hand), 8, (0,255,0), -1)

            # Get face center
            for faceDet in face_result.detections:
                bbox = faceDet.location_data.relative_bounding_box

                x_face = int((bbox.xmin + bbox.width/2) * w)
                y_face = int((bbox.ymin + bbox.height/2) * h)

                cv2.circle(frame, (x_face, y_face), 8, (255,0,0), -1)

                dist = get_distance((x_hand, y_hand), (x_face, y_face))

                # 🔥 MAIN LOGIC
                if dist < 80:
                    detected = True

    # 🚀 ACTION
    if detected:
        cv2.putText(frame, "TAKING MEDICINE", (50,50),
                    cv2.FONT_HERSHEY_SIMPLEX,1,(0,255,0),3)

        if time.time() - last_sent_time > cooldown:
            send_taken()
            last_sent_time = time.time()

    cv2.imshow("AI Medicine Detection", frame)

    if cv2.waitKey(1) == 27:
        break

cap.release()
cv2.destroyAllWindows()