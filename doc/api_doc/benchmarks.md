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
| HD2K (4416x1242) | 15 | 13.0 | 13.9% | 66.14 / 198.96 | 59.7 | 9% | 120 | 0.27 |
| HD1080 (3840x1080) | 30 | 23.8 | 20.3% | 33.26 / 133.73 | 38.7 | 16% | 102 | 0.25 |
| HD720 (2560x720) | 60 | 55.5 | 7.5% | 16.45 / 49.63 | 17.0 | 16% | 72 | 0.21 |
| HD720 (2560x720) | 30 | 28.8 | 4.0% | 33.17 / 66.77 | 31.1 | 8% | 72 | 0.21 |
| VGA (1344x376) | 100 | 99.2 | 0.6% | 9.87 / 10.21 | 6.6 | 9% | 61 | 0.19 |
| VGA (1344x376) | 30 | 29.9 | 0.3% | 33.23 / 33.57 | 19.9 | 3% | 61 | 0.19 |

*Received FPS*: frames available to the application. *Lost frames*: frames sent by the camera that did not reach the application, detected from the gaps between the timestamps. *Latency*: time from the start of the frame to its availability. *CPU*: 100% is one core fully used. *Memory*: memory used by the process, including the three copies of the frame kept by the library. *Open*: time to open the camera and get it ready for settings.

### Sensors retrieval

| Video stream | IMU rate [Hz] | Lost samples | Period [ms] | Period 1% / 99% [ms] | Latency [ms] | Mag. [Hz] | Env. [Hz] | Synced frames | CPU |
|---|---|---|---|---|---|---|---|---|---|
| none | 396.3 | 2 | 2.522 | 2.286 / 2.798 | 0.74 | 49.4 | 24.9 | - | 10% |
| HD2K @ 15 FPS | 396.3 | 2 | 2.522 | 2.325 / 2.798 | 0.88 | 49.4 | 24.7 | 98% | 21% |
| HD1080 @ 30 FPS | 393.2 | 34 | 2.522 | 2.248 / 3.113 | 0.96 | 48.8 | 24.4 | 100% | 29% |
| HD720 @ 60 FPS | 395.0 | 15 | 2.522 | 2.208 / 2.916 | 0.90 | 48.7 | 24.9 | 100% | 28% |
| HD720 @ 30 FPS | 395.3 | 13 | 2.522 | 2.207 / 2.916 | 0.78 | 49.1 | 24.6 | 99% | 18% |
| VGA @ 100 FPS | 395.6 | 9 | 2.522 | 2.365 / 2.799 | 0.83 | 49.2 | 24.7 | 100% | 20% |
| VGA @ 30 FPS | 396.3 | 2 | 2.522 | 2.246 / 2.838 | 0.79 | 49.2 | 24.9 | 100% | 13% |

*Lost samples*: gaps in the IMU timestamps. *Latency*: time from the acquisition of a sample to its availability. *Synced frames*: frames that received the timestamp of the IMU sample acquired with their hardware sync signal. *CPU* includes the video stream, when active.

### Depth generation

| Resolution | Depth map | Convert [ms] | Rectify [ms] | Matching [ms] | Depth [ms] | Total [ms] | Depth FPS | Valid pixels | CPU | Refine [ms] |
|---|---|---|---|---|---|---|---|---|---|---|
| HD2K @ 15 FPS | 2208x1242 | 9.8 | 44.8 | 5048.0 | 17.6 | 5120.1 | 0.2 | 56% | 219% | 656 |
| HD2K @ 15 FPS | 1104x621 | 12.7 | 14.1 | 1068.7 | 5.4 | 1100.9 | 0.9 | 70% | 213% | 242 |
| HD1080 @ 30 FPS | 1920x1080 | 7.7 | 49.0 | 3633.9 | 15.1 | 3705.7 | 0.3 | 55% | 224% | 403 |
| HD1080 @ 30 FPS | 960x540 | 10.5 | 10.8 | 741.7 | 4.0 | 767.0 | 1.3 | 71% | 215% | 243 |
| HD720 @ 30 FPS | 1280x720 | 4.0 | 19.2 | 1518.4 | 6.3 | 1547.8 | 0.6 | 68% | 212% | 314 |
| HD720 @ 30 FPS | 640x360 | 5.2 | 6.0 | 294.2 | 2.1 | 307.5 | 3.2 | 69% | 211% | 187 |
| VGA @ 30 FPS | 672x376 | 1.0 | 6.3 | 299.1 | 1.9 | 308.3 | 3.2 | 68% | 228% | 151 |
| VGA @ 30 FPS | 336x188 | 1.6 | 3.0 | 37.8 | 0.5 | 42.9 | 22.9 | 62% | 212% | 113 |

*Depth map*: the first row of each resolution uses `matchScale` 1.0, the second 0.5. *Depth FPS*: depth maps per second, including the image acquisition. *Valid pixels*: pixels with a depth in the configured range, it depends on the scene. *CPU*: 100% is one core fully used. *Refine*: time of the refinement of the rectification, executed on the first two frames and then once every 300 frames, not included in the total.

### Comments

 * **Images**: at VGA resolution all the frames are received up to 100 FPS. At higher resolutions the USB host of the
   Raspberry Pi 4 loses part of the data sent by the camera: the incomplete frames are discarded, so the application
   receives less frames than requested. The same frames are lost reading the camera with `v4l2-ctl`, without the library.
   The amount depends on the other activities of the system: at HD1080 and HD2K it changed from 8% to 20% in different runs.
 * **Sensors**: the IMU data are received at full rate, with or without the video stream. A few samples can be lost when
   the video stream is active (less than 0.1% up to 0.9%).
 * **Depth**: the stereo matching uses the 4 cores and it is the limit of the Raspberry Pi 4. A 336x188 depth map
   (VGA resolution with `matchScale` 0.5) is generated at more than 20 FPS, a 640x360 or 672x376 depth map at about 3 FPS.
   Higher resolutions are not suitable for real time.

## NVIDIA Jetson AGX Thor

 * NVIDIA Jetson AGX Thor Developer Kit, Ubuntu 24.04, kernel 6.8, OpenCV 4.8
 * ZED 2i connected to a USB 3.0 port through a USB hub
 * ZED Open Capture v1.0.0, default build (`Release`), 10 seconds for each measure
 * The GPU is not used

### Image retrieval

| Resolution | Requested FPS | Received FPS | Lost frames | Frame period 1% / 99% [ms] | Latency [ms] | CPU | Memory [MB] | Open [s] |
|---|---|---|---|---|---|---|---|---|
| HD2K (4416x1242) | 15 | 15.1 | 0.0% | 66.25 / 66.38 | 53.4 | 1% | 81 | 0.23 |
| HD1080 (3840x1080) | 30 | 29.6 | 1.0% | 33.37 / 66.88 | 32.6 | 1% | 63 | 0.23 |
| HD720 (2560x720) | 60 | 59.9 | 0.2% | 16.62 / 16.75 | 14.5 | 1% | 33 | 0.22 |
| HD720 (2560x720) | 30 | 30.0 | 0.0% | 33.25 / 33.38 | 28.8 | 1% | 33 | 0.22 |
| VGA (1344x376) | 100 | 99.8 | 0.0% | 10.00 / 10.13 | 5.8 | 1% | 22 | 0.22 |
| VGA (1344x376) | 30 | 30.0 | 0.0% | 33.37 / 33.50 | 19.1 | 0% | 22 | 0.22 |

*Received FPS*: frames available to the application. *Lost frames*: frames sent by the camera that did not reach the application, detected from the gaps between the timestamps. *Latency*: time from the start of the frame to its availability. *CPU*: 100% is one core fully used. *Memory*: memory used by the process, including the three copies of the frame kept by the library. *Open*: time to open the camera and get it ready for settings.

### Sensors retrieval

| Video stream | IMU rate [Hz] | Lost samples | Period [ms] | Period 1% / 99% [ms] | Latency [ms] | Mag. [Hz] | Env. [Hz] | Synced frames | CPU |
|---|---|---|---|---|---|---|---|---|---|
| none | 404.5 | 0 | 2.472 | 2.202 / 2.743 | 0.01 | 50.5 | 24.8 | - | 1% |
| HD2K @ 15 FPS | 404.5 | 0 | 2.472 | 2.236 / 2.704 | -0.04 | 50.6 | 24.6 | 99% | 2% |
| HD1080 @ 30 FPS | 404.5 | 0 | 2.472 | 2.243 / 2.698 | 0.01 | 50.6 | 24.8 | 99% | 2% |
| HD720 @ 60 FPS | 404.5 | 0 | 2.472 | 2.204 / 2.746 | -0.03 | 50.6 | 24.7 | 100% | 2% |
| HD720 @ 30 FPS | 404.5 | 0 | 2.472 | 2.241 / 2.704 | -0.04 | 50.6 | 24.7 | 99% | 1% |
| VGA @ 100 FPS | 404.4 | 0 | 2.472 | 2.241 / 2.704 | 0.04 | 50.6 | 24.8 | 99% | 1% |
| VGA @ 30 FPS | 404.5 | 0 | 2.472 | 2.240 / 2.704 | -0.05 | 50.6 | 24.7 | 99% | 1% |

*Lost samples*: gaps in the IMU timestamps. *Latency*: time from the acquisition of a sample to its availability. *Synced frames*: frames that received the timestamp of the IMU sample acquired with their hardware sync signal. *CPU* includes the video stream, when active.

### Depth generation

| Resolution | Depth map | Convert [ms] | Rectify [ms] | Matching [ms] | Depth [ms] | Total [ms] | Depth FPS | Valid pixels | CPU | Refine [ms] |
|---|---|---|---|---|---|---|---|---|---|---|
| HD2K @ 15 FPS | 2208x1242 | 1.5 | 3.3 | 188.7 | 0.5 | 194.0 | 5.1 | 64% | 357% | 100 |
| HD2K @ 15 FPS | 1104x621 | 1.9 | 1.2 | 42.8 | 0.2 | 46.0 | 13.7 | 78% | 228% | 38 |
| HD1080 @ 30 FPS | 1920x1080 | 1.2 | 2.5 | 140.5 | 0.4 | 144.5 | 6.3 | 63% | 329% | 73 |
| HD1080 @ 30 FPS | 960x540 | 1.5 | 1.0 | 33.2 | 0.2 | 36.0 | 26.8 | 80% | 345% | 31 |
| HD720 @ 30 FPS | 1280x720 | 0.5 | 1.4 | 60.0 | 0.2 | 62.1 | 15.9 | 79% | 362% | 47 |
| HD720 @ 30 FPS | 640x360 | 0.6 | 0.6 | 13.6 | 0.1 | 15.0 | 26.0 | 79% | 142% | 22 |
| VGA @ 30 FPS | 672x376 | 0.1 | 0.7 | 15.2 | 0.1 | 16.1 | 30.0 | 77% | 179% | 22 |
| VGA @ 30 FPS | 336x188 | 0.2 | 0.5 | 3.3 | 0.0 | 4.0 | 29.9 | 69% | 42% | 15 |

*Depth map*: the first row of each resolution uses `matchScale` 1.0, the second 0.5. *Depth FPS*: depth maps per second, including the image acquisition. *Valid pixels*: pixels with a depth in the configured range, it depends on the scene. *CPU*: 100% is one core fully used. *Refine*: time of the refinement of the rectification, executed on the first two frames and then once every 300 frames, not included in the total.

### Comments

 * **Depth**: with the heaviest configurations the depth frame rate is lower than the processing time allows, because
   frames are lost while all the cores are busy. Keeping the application away from the core that serves the USB
   interrupts (the first one on this computer) avoids it: with `taskset -c 4-13` the 640x360 depth map from HD720
   reaches 29.7 FPS and the 1104x621 depth map from HD2K reaches 15 FPS (measured with a ZED Mini).
 * **Images**: a ZED Mini connected to the same USB hub lost up to 8% of the frames at HD2K and HD1080 in one of the
   runs, and none in the others: also on this computer the frames lost at the highest resolutions depend on the USB
   connection and on the system activity.

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
