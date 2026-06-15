import cv2
import socket
import math
import mediapipe as mp
import numpy as np
import time

from mediapipe.tasks import python
from mediapipe.tasks.python import vision

# -----------------------------
# OneEuroFilter Definition
# -----------------------------
class OneEuroFilter:
    """Adaptive low-pass filter to eliminate jitter without adding lag."""
    def __init__(self, t0, x0, min_cutoff=1.0, beta=0.007, d_cutoff=1.0):
        self.min_cutoff = float(min_cutoff)
        self.beta = float(beta)
        self.d_cutoff = float(d_cutoff)
        self.x_prev = float(x0)
        self.dx_prev = 0.0
        self.t_prev = float(t0)

    def _alpha(self, cutoff, dt):
        tau = 1.0 / (2 * np.pi * cutoff)
        return dt / (dt + tau)

    def __call__(self, t, x):
        dt = t - self.t_prev
        if dt <= 0:
            return self.x_prev

        # 1. Compute velocity of change
        dx = (x - self.x_prev) / dt
        
        # 2. Filter the velocity
        alpha_d = self._alpha(self.d_cutoff, dt)
        dx_hat = alpha_d * dx + (1 - alpha_d) * self.dx_prev
        
        # 3. Use velocity to dynamically scale the cutoff frequency
        cutoff = self.min_cutoff + self.beta * abs(dx_hat)
        
        # 4. Filter the actual data point
        alpha = self._alpha(cutoff, dt)
        x_hat = alpha * x + (1 - alpha) * self.x_prev

        # 5. Save state for next frame iteration
        self.x_prev = x_hat
        self.dx_prev = dx_hat
        self.t_prev = t

        return x_hat

# -----------------------------
# UDP setup
# -----------------------------

UDP_IP = "127.0.0.1"
UDP_PORT = 5005

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# -----------------------------
# MediaPipe setup
# -----------------------------

base_options = python.BaseOptions(
    model_asset_path="face_landmarker.task"
)

options = vision.FaceLandmarkerOptions(
    base_options=base_options,
    num_faces=1,
    output_face_blendshapes=False,
    output_facial_transformation_matrixes=False
)

landmarker = vision.FaceLandmarker.create_from_options(
    options
)

def get_real_world_3d_coordinates(landmarks, frame_width, frame_height, hfov_degrees=75, left_eye_idx=33, right_eye_idx=263):
    """
    Calculates real-world X, Y, and Z (depth) in meters relative to the camera lens.
    Uses known 0.0886m eye distance and the camera's HFOV.
    """
    # 1. Denormalize eye coordinates to pixel positions
    p1 = landmarks[left_eye_idx]
    p2 = landmarks[right_eye_idx]
    
    eye1_x = p1.x * frame_width
    eye1_y = p1.y * frame_height
    eye1_z = p1.z * frame_width  
    eye2_x = p2.x * frame_width
    eye2_y = p2.y * frame_height
    eye2_z = p2.z * frame_width  
    
    # 2. Calculate the pixel distance between the eyes
    pixel_distance_eyes = math.sqrt((eye2_x - eye1_x)**2 + (eye2_y - eye1_y)**2 + (eye2_z - eye1_z)**2)
    if pixel_distance_eyes == 0:
        return None
        
    # 3. Calculate camera focal length in pixels based on its active HFOV
    hfov_radians = np.radians(hfov_degrees)
    f_pixel = frame_width / (2 * np.tan(hfov_radians / 2))
    
    # 4. Calculate Real-World Depth (Z) in meters
    real_eye_dist_meters = 0.0886
    real_z_meters = (f_pixel * real_eye_dist_meters) / pixel_distance_eyes
    
    # 5. Calculate X and Y scale factor at this specific depth
    meters_per_pixel = real_eye_dist_meters / pixel_distance_eyes
    
    # 6. Convert target landmark (e.g., Nose tip at index 1) to meters
    target = landmarks[1]
    target_px_x = target.x * frame_width
    target_px_y = target.y * frame_height
    
    # Shift origin (0,0) to the center of the camera frame
    centered_x = target_px_x - (frame_width / 2)
    centered_y = (frame_height / 2) - target_px_y  # Flip Y so 'up' is positive
    
    # Convert lateral pixel coordinates to meters
    real_x_meters = centered_x * meters_per_pixel
    real_y_meters = centered_y * meters_per_pixel
    
    return real_x_meters, real_y_meters, real_z_meters


# -----------------------------
# Webcam & Filter Initialization
# -----------------------------

cap = cv2.VideoCapture(0)

if not cap.isOpened():
    raise RuntimeError("Could not open webcam")

# Instantiate tracking variable for the filter state
z_filter = None

print("Tracking started...")

while True:

    success, frame = cap.read()

    if not success:
        continue

    frame = cv2.flip(frame, 1)

    rgb = cv2.cvtColor(
        frame,
        cv2.COLOR_BGR2RGB
    )

    mp_image = mp.Image(
        image_format=mp.ImageFormat.SRGB,
        data=rgb
    )

    result = landmarker.detect(mp_image)

    if len(result.face_landmarks) > 0:

        face = result.face_landmarks[0]

        # MediaPipe landmark indices
        LEFT_EYE = 33
        RIGHT_EYE = 263

        left_eye = face[LEFT_EYE]
        right_eye = face[RIGHT_EYE]
        x = 0
        y = 0
        z = 0
        
        # Get frame properties dynamically for calculation
        w_prop = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        h_prop = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        
        coords = get_real_world_3d_coordinates(face, w_prop, h_prop, hfov_degrees=75, left_eye_idx=LEFT_EYE, right_eye_idx=RIGHT_EYE)
        
        if coords:
            x, y, raw_z = coords
            current_time = time.time()
            
            # Initialize or update the One Euro Filter for Z exclusively
            if z_filter is None:
                # min_cutoff=0.8 handles the stationary jitter; beta=0.01 responds to rapid movements
                z_filter = OneEuroFilter(t0=current_time, x0=raw_z, min_cutoff=0.8, beta=0.01)
                z = raw_z
            else:
                z = z_filter(t=current_time, x=raw_z)

        packet = (
            f"{x:.6f},"
            f"{y:.6f},"
            f"{z:.6f}"
        )

        sock.sendto(
            packet.encode(),
            (UDP_IP, UDP_PORT)
        )

        h, w = frame.shape[:2]

        lx = int(left_eye.x * w)
        ly = int(left_eye.y * h)

        rx = int(right_eye.x * w)
        ry = int(right_eye.y * h)

        cv2.circle(frame, (lx, ly), 4, (0,255,0), -1)
        cv2.circle(frame, (rx, ry), 4, (0,255,0), -1)

        cv2.putText(
            frame,
            f"Smooth Depth Z: {z:.3f}m",
            (20, 40),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            (0,255,0),
            2
        )

    cv2.imshow(
        "MediaPipe Head Tracking",
        frame
    )

    key = cv2.waitKey(1)

    if key == 27:
        break

cap.release()
cv2.destroyAllWindows()










"""
import cv2
import socket
import math
import mediapipe as mp
import numpy as np

from mediapipe.tasks import python
from mediapipe.tasks.python import vision

# -----------------------------
# UDP setup
# -----------------------------

UDP_IP = "127.0.0.1"
UDP_PORT = 5005

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# -----------------------------
# MediaPipe setup
# -----------------------------

base_options = python.BaseOptions(
    model_asset_path="face_landmarker.task"
)

options = vision.FaceLandmarkerOptions(
    base_options=base_options,
    num_faces=1,
    output_face_blendshapes=False,
    output_facial_transformation_matrixes=False
)

landmarker = vision.FaceLandmarker.create_from_options(
    options
)

import math
import numpy as np

def get_real_world_3d_coordinates(landmarks, frame_width, frame_height, hfov_degrees=75, left_eye_idx=33, right_eye_idx=263):
    
    #Calculates real-world X, Y, and Z (depth) in meters relative to the camera lens.
    #Uses known 0.0886m eye distance and the camera's HFOV.
    
    # 1. Denormalize eye coordinates to pixel positions
    p1 = landmarks[left_eye_idx]
    p2 = landmarks[right_eye_idx]
    
    eye1_x = p1.x * frame_width
    eye1_y = p1.y * frame_height
    eye2_x = p2.x * frame_width
    eye2_y = p2.y * frame_height
    
    # 2. Calculate the pixel distance between the eyes
    pixel_distance_eyes = math.sqrt((eye2_x - eye1_x)**2 + (eye2_y - eye1_y)**2)
    if pixel_distance_eyes == 0:
        return None
        
    # 3. Calculate camera focal length in pixels based on its active HFOV
    hfov_radians = np.radians(hfov_degrees)
    f_pixel = frame_width / (2 * np.tan(hfov_radians / 2))
    
    # 4. Calculate Real-World Depth (Z) in meters
    real_eye_dist_meters = 0.0886
    real_z_meters = (f_pixel * real_eye_dist_meters) / pixel_distance_eyes
    
    # 5. Calculate X and Y scale factor at this specific depth
    meters_per_pixel = real_eye_dist_meters / pixel_distance_eyes
    
    # 6. Convert target landmark (e.g., Nose tip at index 1) to meters
    target = landmarks[1]
    target_px_x = target.x * frame_width
    target_px_y = target.y * frame_height
    
    # Shift origin (0,0) to the center of the camera frame
    centered_x = target_px_x - (frame_width / 2)
    centered_y = (frame_height / 2) - target_px_y  # Flip Y so 'up' is positive
    
    # Convert lateral pixel coordinates to meters
    real_x_meters = centered_x * meters_per_pixel
    real_y_meters = centered_y * meters_per_pixel
    
    return real_x_meters, real_y_meters, real_z_meters

# Example of reading the data inside your MediaPipe loop:
# if results.multi_face_landmarks:
#     for face_landmarks in results.multi_face_landmarks:
#         coords = get_real_world_3d_coordinates(face_landmarks.landmark, width, height, hfov_degrees=75)
#         if coords:
#             x, y, z = coords
#             print(f"Position from lens -> X: {x:.2f}m, Y: {y:.2f}m, Depth Z: {z:.2f}m")


# -----------------------------
# Webcam
# -----------------------------

cap = cv2.VideoCapture(0)

if not cap.isOpened():
    raise RuntimeError("Could not open webcam")

print("Tracking started...")

while True:

    success, frame = cap.read()

    if not success:
        continue

    frame = cv2.flip(frame, 1)

    rgb = cv2.cvtColor(
        frame,
        cv2.COLOR_BGR2RGB
    )

    mp_image = mp.Image(
        image_format=mp.ImageFormat.SRGB,
        data=rgb
    )

    result = landmarker.detect(mp_image)

    if len(result.face_landmarks) > 0:

        face = result.face_landmarks[0]

        # MediaPipe landmark indices
        LEFT_EYE = 33
        RIGHT_EYE = 263

        left_eye = face[LEFT_EYE]
        right_eye = face[RIGHT_EYE]
        x = 0
        y = 0
        z = 0
        coords = get_real_world_3d_coordinates(face, int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)), int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT)), hfov_degrees=75, left_eye_idx=LEFT_EYE, right_eye_idx=RIGHT_EYE)
        if coords:
            x, y, z = coords

        packet = (
            f"{x:.6f},"
            f"{y:.6f},"
            f"{z:.6f}"
        )

        sock.sendto(
            packet.encode(),
            (UDP_IP, UDP_PORT)
        )

        h, w = frame.shape[:2]

        lx = int(left_eye.x * w)
        ly = int(left_eye.y * h)

        rx = int(right_eye.x * w)
        ry = int(right_eye.y * h)

        cv2.circle(frame, (lx, ly), 4, (0,255,0), -1)
        cv2.circle(frame, (rx, ry), 4, (0,255,0), -1)

        cv2.putText(
            frame,
            f"EyeDist: {z:.3f}",
            (20, 40),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            (0,255,0),
            2
        )

    cv2.imshow(
        "MediaPipe Head Tracking",
        frame
    )

    key = cv2.waitKey(1)

    if key == 27:
        break

cap.release()
cv2.destroyAllWindows()
"""