# ESP32 Hand Gesture → OLED

Detect hand gestures with your **phone's camera** (Google MediaPipe, running in the browser) and show them **live on an ESP32 + SSD1306 OLED** — over WiFi, no app to install.

Your phone does the heavy lifting (real 21‑landmark hand tracking). The ESP32 just receives which fingers are up and draws a little hand + the gesture name.

![demo](assets/demo.gif)

## Features

- ✋ **Real hand tracking** — MediaPipe `HandLandmarker`, 21 landmarks, per‑finger detection (not a skin‑colour hack, not confused by your face).
- 🔤 **Gesture names** — FIST, OPEN, PEACE, THUMBSUP, POINT, ROCK, GUN, … from the 5‑finger mask.
- 🖐️ **Schematic hand on the OLED** — folded fingers fold down, extended fingers stand up, + a live finger count.
- 📶 **Wireless** — phone talks to the ESP32 over WiFi; no USB tether, no app.
- 👍 **Handedness‑independent thumb** — thumb is detected by geometry, so it works for either hand.

## How it works

```
Phone browser (MediaPipe, camera)  --WiFi /g?m=01100&n=PEACE&c=2-->  ESP32  -->  OLED
```

1. The ESP32 joins your WiFi (station mode) and serves a tiny web page; its IP shows on the OLED.
2. You open that IP on your phone. The page loads MediaPipe (from a CDN) and uses the phone camera.
3. For each frame it computes which fingers are extended, classifies the gesture, and `GET`s it to the ESP32.
4. The ESP32 draws the hand + gesture name on the SSD1306.

## Hardware

| ESP32 pin | SSD1306 OLED |
|-----------|--------------|
| 3V3       | VCC          |
| GND       | GND          |
| GPIO21    | SDA          |
| GPIO22    | SCL          |

- Any classic **ESP32** dev board.
- **0.96"/1.3" SSD1306 128×64 I2C OLED** (address `0x3C`/`0x3D`, auto‑detected).

## Setup

1. In `firmware/hand_gesture_oled/hand_gesture_oled.ino`, set **`WIFI_SSID`** and **`WIFI_PASS`** to your network.
2. Install libraries (Arduino Library Manager): **Adafruit GFX Library**, **Adafruit SSD1306**.
3. Flash (Arduino IDE, or arduino-cli):
   ```
   arduino-cli core install esp32:esp32
   arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306"
   arduino-cli compile --fqbn esp32:esp32:esp32 firmware/hand_gesture_oled
   arduino-cli upload  -p COM3 --fqbn esp32:esp32:esp32 firmware/hand_gesture_oled
   ```
   (replace `COM3` with your port, e.g. `/dev/ttyUSB0`).
4. The OLED shows the ESP32's **IP** (e.g. `192.168.1.13`).

## Using it

1. Put your phone on the **same WiFi** (it needs internet so MediaPipe can load its model).
2. Camera over `http` needs a secure context, so enable this flag once in the phone's **Chrome**:
   `chrome://flags` → **"Insecure origins treated as secure"** → add **`http://<the-ESP32-IP>`** → **Relaunch**.
3. Open **`http://<the-ESP32-IP>`** → **Start** → point the (rear) camera at your hand.

Fold/extend fingers → the OLED mirrors them with the gesture name. Use **flip cam** to switch front/rear.

## Notes & limitations

- The phone needs **internet** on that WiFi — MediaPipe's WASM + model load from a CDN (not hosted on the ESP32; the model is ~7 MB, far bigger than ESP32 flash).
- `getUserMedia` is blocked on plain `http` unless you set the Chrome flag above (or serve over https).
- Finger up/down for index–pinky assumes a roughly upright hand; the **thumb** uses a geometry rule (`dist(tip,pinky‑base) > dist(ip,pinky‑base)`) so handedness doesn't matter.
- The ESP32's DHCP IP can change on reboot — just read the new one off the OLED and update the flag.

## License

MIT — see [LICENSE](LICENSE).
