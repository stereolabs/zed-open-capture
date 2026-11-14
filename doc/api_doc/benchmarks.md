# Benchmarks {#benchmarks}

The values of this page have been measured with the tool `zed_open_capture_benchmark`, installed with the examples.
The tool does not need a display, so you can run it on your computer from a remote terminal:

    $ zed_open_capture_benchmark                 # all the measures
    $ zed_open_capture_benchmark --depth         # only the selected measures: --video, --sensors, --depth
    $ zed_open_capture_benchmark --duration 20   # seconds for each measure, default 5

The results depend on the computer, on the other running applications and, for the depth, on the scene.

## Raspberry Pi 4

 * Raspberry Pi 4 Model B Rev 1.4, 8 GB, Raspberry Pi OS 13 (64 bit), kernel 6.18, OpenCV 4.10
 * ZED 2 connected to a USB 3.0 port
 * ZED Open Capture v1.0.0, default build (`Release`), 10 seconds for each measure
 * The desktop was running, with a remote desktop application that used about one CPU core

### Image retrieval

| Resolution | Requested FPS | Received FPS | Lost frames | Frame period 1% / 99% [ms] | Latency [ms] | CPU | Memory [MB] | Open [s] |
|---|---|---|---|---|---|---|---|---|
| HD2K (4416x1242) | 15 | 13.4 | 11.3% | 66.09 / 198.85 | 59.9 | 10% | 120 | 0.26 |
| HD1080 (3840x1080) | 30 | 25.0 | 16.1% | 33.24 / 100.49 | 39.1 | 18% | 102 | 0.25 |
| HD720 (2560x720) | 60 | 55.2 | 8.0% | 16.48 / 50.00 | 16.8 | 16% | 72 | 0.22 |
| HD720 (2560x720) | 30 | 28.3 | 5.7% | 33.15 / 100.02 | 31.0 | 8% | 72 | 0.20 |
| VGA (1344x376) | 100 | 98.3 | 1.6% | 9.86 / 20.03 | 6.6 | 9% | 61 | 0.19 |
| VGA (1344x376) | 30 | 30.0 | 0.0% | 33.28 / 33.54 | 20.0 | 3% | 61 | 0.19 |

*Received FPS*: frames available to the application. *Lost frames*: frames sent by the camera that did not reach the application, detected from the gaps between the timestamps. *Latency*: time from the start of the frame to its availability. *CPU*: 100% is one core fully used. *Memory*: memory used by the process, including the three copies of the frame kept by the library. *Open*: time to open the camera and get it ready for settings.

### Sensors retrieval

| Video stream | IMU rate [Hz] | Lost samples | Period [ms] | Period 1% / 99% [ms] | Latency [ms] | Mag. [Hz] | Env. [Hz] | Synced frames | CPU |
|---|---|---|---|---|---|---|---|---|---|
| none | 396.3 | 2 | 2.522 | 2.246 / 2.798 | 0.77 | 49.4 | 24.8 | - | 10% |
| HD2K @ 15 FPS | 395.7 | 8 | 2.522 | 2.248 / 2.837 | 0.89 | 49.3 | 24.7 | 98% | 21% |
| HD1080 @ 30 FPS | 394.7 | 19 | 2.522 | 2.246 / 2.962 | 0.94 | 48.9 | 23.9 | 99% | 29% |
| HD720 @ 60 FPS | 394.1 | 24 | 2.522 | 2.246 / 2.995 | 0.89 | 48.6 | 24.6 | 99% | 29% |
| HD720 @ 30 FPS | 395.8 | 7 | 2.522 | 2.246 / 2.837 | 0.81 | 49.3 | 24.6 | 99% | 19% |
| VGA @ 100 FPS | 396.0 | 5 | 2.522 | 2.286 / 2.837 | 0.81 | 49.6 | 24.8 | 100% | 20% |
| VGA @ 30 FPS | 396.3 | 2 | 2.522 | 2.286 / 2.837 | 0.80 | 49.5 | 24.7 | 99% | 12% |

*Lost samples*: gaps in the IMU timestamps. *Latency*: time from the acquisition of a sample to its availability. *Synced frames*: frames that received the timestamp of the IMU sample acquired with their hardware sync signal. *CPU* includes the video stream, when active.

### Depth generation

| Resolution | Depth map | Convert [ms] | Rectify [ms] | Matching [ms] | Depth [ms] | Total [ms] | Depth FPS | Valid pixels | CPU | Refine [ms] |
|---|---|---|---|---|---|---|---|---|---|---|
| HD2K @ 15 FPS | 2208x1242 | 12.3 | 54.3 | 4767.2 | 17.8 | 4851.6 | 0.2 | 16% | 218% | 441 |
| HD2K @ 15 FPS | 1104x621 | 12.4 | 13.4 | 1040.6 | 5.4 | 1071.8 | 0.9 | 18% | 213% | 202 |
| HD1080 @ 30 FPS | 1920x1080 | 8.0 | 38.5 | 3499.2 | 12.6 | 3558.3 | 0.3 | 22% | 225% | 396 |
| HD1080 @ 30 FPS | 960x540 | 11.0 | 9.3 | 782.2 | 4.0 | 806.4 | 1.2 | 34% | 209% | 215 |
| HD720 @ 30 FPS | 1280x720 | 3.4 | 17.6 | 1458.2 | 7.7 | 1486.8 | 0.7 | 27% | 221% | 237 |
| HD720 @ 30 FPS | 640x360 | 4.7 | 6.3 | 292.9 | 1.7 | 305.6 | 3.3 | 25% | 209% | 144 |
| VGA @ 30 FPS | 672x376 | 0.9 | 7.1 | 302.0 | 1.8 | 311.8 | 3.2 | 40% | 229% | 152 |
| VGA @ 30 FPS | 336x188 | 1.5 | 2.7 | 35.8 | 0.5 | 40.7 | 24.1 | 48% | 208% | 104 |

*Depth map*: the first row of each resolution uses `matchScale` 1.0, the second 0.5. *Depth FPS*: depth maps per second, including the image acquisition. *Valid pixels*: pixels with a depth in the configured range, it depends on the scene. *CPU*: 100% is one core fully used. *Refine*: time of the refinement of the rectification, executed on the first two frames and then once every 300 frames, not included in the total.

### Comments

 * **Images**: at VGA resolution all the frames are received up to 100 FPS. At higher resolutions the USB host of the
   Raspberry Pi 4 loses part of the data sent by the camera: the incomplete frames are discarded, so the application
   receives less frames than requested. The same frames are lost reading the camera with `v4l2-ctl`, without the library.
   The amount depends on the other activities of the system.
 * **Sensors**: the IMU data are received at full rate, with or without the video stream. A few samples can be lost when
   the video stream is active (less than 0.1% up to 0.6%).
 * **Depth**: the stereo matching uses the 4 cores and it is the limit of the Raspberry Pi 4. A 336x188 depth map
   (VGA resolution with `matchScale` 0.5) is generated at more than 20 FPS, a 640x360 or 672x376 depth map at about 3 FPS.
   Higher resolutions are not suitable for real time.

## NVIDIA Jetson AGX Thor

 * NVIDIA Jetson AGX Thor Developer Kit, Ubuntu 24.04, kernel 6.8, OpenCV 4.8
 * ZED Mini connected to a USB 3.0 port
 * ZED Open Capture v1.0.0, default build (`Release`), 10 seconds for each measure
 * The GPU is not used

### Image retrieval

| Resolution | Requested FPS | Received FPS | Lost frames | Frame period 1% / 99% [ms] | Latency [ms] | CPU | Memory [MB] | Open [s] |
|---|---|---|---|---|---|---|---|---|
| HD2K (4416x1242) | 15 | 14.9 | 1.3% | 66.25 / 132.51 | 53.4 | 1% | 81 | 0.23 |
| HD1080 (3840x1080) | 30 | 29.9 | 0.0% | 33.37 / 33.50 | 32.6 | 1% | 63 | 0.22 |
| HD720 (2560x720) | 60 | 60.0 | 0.0% | 16.62 / 16.75 | 14.5 | 1% | 33 | 0.22 |
| HD720 (2560x720) | 30 | 30.0 | 0.0% | 33.25 / 33.38 | 28.8 | 1% | 33 | 0.22 |
| VGA (1344x376) | 100 | 99.8 | 0.0% | 10.00 / 10.13 | 5.8 | 1% | 22 | 0.22 |
| VGA (1344x376) | 30 | 30.0 | 0.0% | 33.37 / 33.50 | 19.1 | 0% | 22 | 0.22 |

*Received FPS*: frames available to the application. *Lost frames*: frames sent by the camera that did not reach the application, detected from the gaps between the timestamps. *Latency*: time from the start of the frame to its availability. *CPU*: 100% is one core fully used. *Memory*: memory used by the process, including the three copies of the frame kept by the library. *Open*: time to open the camera and get it ready for settings.

### Sensors retrieval

| Video stream | IMU rate [Hz] | Lost samples | Period [ms] | Period 1% / 99% [ms] | Latency [ms] | Mag. [Hz] | Env. [Hz] | Synced frames | CPU |
|---|---|---|---|---|---|---|---|---|---|
| none | 802.3 | 1 | 1.246 | 1.207 / 1.285 | 0.45 | 0.0 | 0.0 | - | 1% |
| HD2K @ 15 FPS | 802.3 | 1 | 1.246 | 1.207 / 1.285 | 0.50 | 0.0 | 0.0 | 99% | 2% |
| HD1080 @ 30 FPS | 802.5 | 0 | 1.246 | 1.207 / 1.285 | 0.45 | 0.0 | 0.0 | 99% | 3% |
| HD720 @ 60 FPS | 802.4 | 0 | 1.246 | 1.207 / 1.285 | 0.43 | 0.0 | 0.0 | 99% | 3% |
| HD720 @ 30 FPS | 802.4 | 0 | 1.246 | 1.207 / 1.285 | 0.49 | 0.0 | 0.0 | 100% | 2% |
| VGA @ 100 FPS | 802.3 | 1 | 1.246 | 1.207 / 1.285 | 0.43 | 0.0 | 0.0 | 99% | 2% |
| VGA @ 30 FPS | 802.3 | 0 | 1.246 | 1.207 / 1.285 | 0.44 | 0.0 | 0.0 | 99% | 2% |

*Lost samples*: gaps in the IMU timestamps. *Latency*: time from the acquisition of a sample to its availability. *Synced frames*: frames that received the timestamp of the IMU sample acquired with their hardware sync signal. *CPU* includes the video stream, when active.

### Depth generation

| Resolution | Depth map | Convert [ms] | Rectify [ms] | Matching [ms] | Depth [ms] | Total [ms] | Depth FPS | Valid pixels | CPU | Refine [ms] |
|---|---|---|---|---|---|---|---|---|---|---|
| HD2K @ 15 FPS | 2208x1242 | 1.5 | 3.3 | 183.7 | 0.4 | 188.9 | 5.3 | 77% | 365% | 73 |
| HD2K @ 15 FPS | 1104x621 | 1.9 | 1.1 | 42.6 | 0.2 | 45.7 | 14.1 | 72% | 234% | 28 |
| HD1080 @ 30 FPS | 1920x1080 | 1.2 | 2.5 | 139.5 | 0.5 | 143.8 | 6.9 | 63% | 363% | 54 |
| HD1080 @ 30 FPS | 960x540 | 1.5 | 0.9 | 33.4 | 0.2 | 36.0 | 26.5 | 67% | 343% | 18 |
| HD720 @ 30 FPS | 1280x720 | 0.5 | 1.4 | 59.1 | 0.2 | 61.2 | 16.3 | 69% | 370% | 36 |
| HD720 @ 30 FPS | 640x360 | 0.6 | 0.6 | 14.0 | 0.1 | 15.3 | 22.2 | 60% | 123% | 14 |
| VGA @ 30 FPS | 672x376 | 0.1 | 0.7 | 15.2 | 0.1 | 16.1 | 30.0 | 64% | 180% | 15 |
| VGA @ 30 FPS | 336x188 | 0.2 | 0.5 | 3.3 | 0.0 | 4.0 | 30.0 | 53% | 42% | 4 |

*Depth map*: the first row of each resolution uses `matchScale` 1.0, the second 0.5. *Depth FPS*: depth maps per second, including the image acquisition. *Valid pixels*: pixels with a depth in the configured range, it depends on the scene. *CPU*: 100% is one core fully used. *Refine*: time of the refinement of the rectification, executed on the first two frames and then once every 300 frames, not included in the total.

### Comments

 * **Depth**: with the heaviest configurations the depth frame rate is lower than the processing time allows, because
   frames are lost while all the cores are busy. Keeping the application away from the core that serves the USB
   interrupts (the first one on this computer) avoids it: with `taskset -c 4-13` the 640x360 depth map from HD720
   reaches 29.7 FPS and the 1104x621 depth map from HD2K reaches 15 FPS.

## Tips

 * Connect the camera to a USB 3.0 port with a USB 3.0 cable. `zed_open_capture_benchmark` and
   `VideoCapture::getDeviceList()` report the speed of the connection: with USB 2.0 only a few low resolution modes are
   available.
 * Use `Frame::frame_id` or the frame timestamps to detect lost frames.
 * Read the IMU with `SensorCapture::getIMUData()` to receive all the samples.
 * For the depth, choose the camera resolution and `matchScale` according to the tables above. The other parameters that
   change the processing time are `numDisparities` (the time is proportional to it) and `mode` (see the depth settings in
   the README).
 * On a computer with many cores, keep the processing threads away from the core that serves the USB interrupts
   (see `/proc/interrupts`), for example with `taskset`.
