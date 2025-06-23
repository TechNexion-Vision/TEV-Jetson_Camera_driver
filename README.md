[![Technexion](https://github.com/user-attachments/assets/d20ae328-b4ff-4152-b4dd-6753674d0fe8)](https://www.technexion.com/products/embedded-vision/)

[![Producer: Technexion](https://img.shields.io/badge/Producer-Technexion-blue.svg)](https://www.technexion.com)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

## Introduction

[TechNexion Embedded Vision Solutions](https://www.technexion.com/products/embedded-vision/) provide embedded system developers access to high-performance, industrial-grade camera solutions to accelerate their time to market for embedded vision projects.

---

## Supported JetPack Version

- [JetPack 6.1](https://developer.nvidia.com/embedded/jetpack-sdk-61) [[L4T 36.4]](https://developer.nvidia.com/embedded/jetson-linux-r3640)
- [JetPack 6.2](https://developer.nvidia.com/embedded/jetpack-sdk-62) [[L4T 36.4.3]](https://developer.nvidia.com/embedded/jetson-linux-r3643)

</br>

>**Note**: If You'd like to find other supported JetPack versions, you should switch to the other branches:
>- [JetPack 4.6.1](https://github.com/TechNexion-Vision/TEV-Jetson_Camera_driver/tree/tn_l4t-r32.7.1_kernel-4.9)
>- [JetPack 5.1.x](https://github.com/TechNexion-Vision/TEV-Jetson_Camera_driver/tree/tn_l4t-r35.3.1.ga_kernel-5.10)

## Supported Camera Modules

| **Camera Series** | **Products** |
| --- | --- |
| TEVS | TEVS-AR0144<br>TEVS-AR0145<br>TEVS-AR0234<br>TEVS-AR0521<br>TEVS-AR0522<br>TEVS-AR0821<br>TEVS-AR0822<br>TEVS-AR1335 |
| VLS3 | VLS3-AR0144<br>VLS3-AR0145<br>VLS3-AR0234<br>VLS3-AR0521<br>VLS3-AR0522<br>VLS3-AR0821<br>VLS3-AR0822<br>VLS3-AR1335 |
| VLS-GM2 | VLS-GM2-AR0144<br>VLS-GM2-AR0145<br>VLS-GM2-AR0234<br>VLS-GM2-AR0521<br>VLS-GM2-AR0522<br>VLS-GM2-AR0821<br>VLS-GM2-AR0822<br>VLS-GM2-AR1335 |


[More Camera Products Details...](https://www.technexion.com/products/embedded-vision)
## Supported NVIDIA Jetson Developer Kit

- [NVIDIA Jetson Orin NANO](https://developer.nvidia.com/embedded/learn/get-started-jetson-orin-nano-devkit)

---
## Install TN Camera on Jetson Developer Kit

### Adaptor for Nvidia **Jetson ORIN NANO Development Kit**

**TEV-RPI22 Adaptor for TEVS**

> Connect TEVS camera and TEV-RPI22 adaptor to **Jetson Orin Nano Developer Kit** directly.

<a href="https://www.technexion.com/products/embedded-vision/mipi-csi2/evk/tevs-ar0144-c-s33-ir-rpi22/" target="_blank">
 <img src="https://www.technexion.com/wp-content/uploads/2024/12/tevs-ar0144-c-s33-ir-rpi22.png" width="400" height="400" />
</a>

**VLS3-ORIN-EVK Adaptor for VLS3**

> Follow the [video](https://www.youtube.com/watch?v=Ggu97E-KmsA) to connect VLS3 cameras and VLS3-ORIN-EVK adaptor to **Jetson Orin Nano Developer Kit**.

<a href="http://www.youtube.com/watch?feature=player_embedded&v=Ggu97E-KmsA" target="_blank">
 <img src="https://img.youtube.com/vi/Ggu97E-KmsA/0.jpg" alt="Watch the video"  width="640" height="360" />
</a>

**VLS-GM2-ORIN-EVK Adaptor for VLS-GM2**

> Follow the video to connect VLS-GM2 cameras and VLS-GM2-ORIN-EVK adaptor to **Jetson Orin Nano Developer Kit**.

---

---

#### Method 1 - Using Technexion Pre-built modules

##### Preparation

We recommend following the [Getting Started Guide](https://developer.nvidia.com/embedded/learn/get-started-jetson-orin-nano-devkit) for Jetson Orin Nano Developer Kit.
After that, you can follow the below method to install TechNexion Cameras Driver.

1. Download pre-built modules.

JetPack 6.1
```
wget https://download.technexion.com/demo_software/EVK/NVIDIA/OrinNano/pre-built-modules/latest/JP61/tn_camera_modules_jp61.tar.gz
```

JetPack 6.2
```
wget https://download.technexion.com/demo_software/EVK/NVIDIA/OrinNano/pre-built-modules/latest/JP62/tn_camera_modules_jp62.tar.gz
```

2. uncompress the modules.

```shell
tar -xf tn_camera_modules_<jp_ver>.tar.gz
```

3. Run installation script.

```shell'
cd tn_camera_modules_<release_time>/
./tn_install.sh
```

4. After you agree to continue the installation, select the pre-installed modules that you want. The default module is TEVS cameras.

```shell
$ ./tn_install.sh
****** TechNexion Camera Driver Installation ******
This installation is easy to install TechnNexion Camera Drivers for Nvidia
Jetson Orin NANO Development Kits. Before start to install camera driver,
You should BACKUP your image to avoid any file you lost while installing process.
Do you want to continue?[Y/n]Y
Continuing with the installation...
Install TN-CAM modules: max96724.ko
Install TN-CAM modules: max96717.ko
Install TN-CAM modules: vls3.ko
Install TN-CAM modules: tevs.ko
Install TN-CAM DTBO file: tevs-dual
Installed TN-CAM DTB file Done.
Install TN-CAM DTBO file: vls3
Installed TN-CAM DTB file Done.
Install TN-CAM DTBO file: vls-gm2
Installed TN-CAM DTB file Done.
Install TN-CAM DTBO file: vls-gm2-fsync
Installed TN-CAM DTB file Done.
Install TN-CAM DTBO file: vls-gm2-tunnel
Installed TN-CAM DTB file Done.
Install TN-CAM DTBO file: vls-gm2-tunnel-fsync
Installed TN-CAM DTB file Done.
Select modules:
    [1]: TEVS: TEVS Series MIPI Cameras with TEV-RPI22 Adaptor
    [2]: VLS3: VLS3 Series Cameras with VLS3-ORIN-EVK Adaptor
    [3]: VLS-GM2: VLS-GM2 Series Cameras with VLS-GM2-ORIN-EVK Adaptor
Which modules do you select?[default:1]
```

Note: You should reboot the device after installation.

---

#### Method 2 - Build drivers from source codes

Please follow the [guide](https://developer.technexion.com/docs/embedded-vision/tevs/usage-guides/nvidia/quick-start/technexion-camera-modules-for-jetpack-6x) to build camera driver modules.

---

## Bring up Camera by GStreamer

If you succeed in initialing the camera, you can follow the steps to open the camera.

1. Check the supported resolutions:

```shell
$ gst-device-monitor-1.0 Video/Source
Device found:

        name  : vi-output, tevs 10-0048
        class : Video/Source
        caps  : video/x-raw, format=UYVY, width=640, height=480, framerate=120/1
                video/x-raw, format=UYVY, width=1280, height=720, framerate=120/1
                video/x-raw, format=UYVY, width=1920, height=1080, framerate=60/1
                video/x-raw, format=UYVY, width=1920, height=1200, framerate=60/1
                video/x-raw, format=NV16, width=640, height=480, framerate=120/1
                video/x-raw, format=NV16, width=1280, height=720, framerate=120/1
                video/x-raw, format=NV16, width=1920, height=1080, framerate=60/1
                video/x-raw, format=NV16, width=1920, height=1200, framerate=60/1
                video/x-raw, format=UYVY, width=640, height=480, framerate=120/1
                video/x-raw, format=UYVY, width=1280, height=720, framerate=120/1
                video/x-raw, format=UYVY, width=1920, height=1080, framerate=60/1
                video/x-raw, format=UYVY, width=1920, height=1200, framerate=60/1
        properties:
                object.path = v4l2:/dev/video0
                device.api = v4l2
                media.class = Video/Source
                api.v4l2.path = /dev/video0
                api.v4l2.cap.driver = tegra-video
                api.v4l2.cap.card = "vi-output\,\ tevs\ 10-0048"
                api.v4l2.cap.bus_info = platform:tegra-capture-vi:1
                api.v4l2.cap.version = 5.15.148
                api.v4l2.cap.capabilities = 84200001
                api.v4l2.cap.device-caps = 04200001
                device.id = 33
                node.name = v4l2_input.platform-tegra-capture-vi
                node.description = "vi-output\,\ tevs\ 10-0048"
                factory.name = api.v4l2.source
                node.pause-on-idle = false
                factory.id = 10
                client.id = 32
                clock.quantum-limit = 8192
                media.role = Camera
                node.driver = true
                object.id = 34
                object.serial = 34
        gst-launch-1.0 pipewiresrc path=34 ! ...
```
2. Bring up the camera (/dev/video0) with 1280x720 by Gstreamer pipeline:

```shell
DISPLAY=:0 gst-launch-1.0 v4l2src device=/dev/video0 ! \
"video/x-raw, format=UYVY, width=1280, height=720" ! xvimagesink sync=false
```

### Troubleshooting

**1. Cannot Find Cameras**

If you cannot bring up the cameras, you can check if the video device does exist.

```shell
$ ls /dev/video*  # List all video devices
/dev/video0  /dev/video1
```
If you cannot see the devices, you should check if the drivers have been probed.

**2. Occur Error: Could not get EGL display connection**

If you occurred the errors `nvbufsurftransform: Could not get EGL display connection` by rununing the Gstreamer command, you can modify the parameter 'DISPLAY' by the command:

```shell
# Check and Set environment parameter for 'DISPLAY'
$ export DISPLAY=$(w| tr -s ' '| cut -d ' ' -f 3 | grep :)
# Run Gstreamer pipeline
$ gst-launch-1.0 v4l2src device=/dev/video0 ! \
"video/x-raw, format=(string)UYVY, width=(int)1280, height=(int)720" ! \
nvvidconv ! nv3dsink sync=false
```

---
## WIKI Pages

[TechNexion NVIDIA products guide](https://developer.technexion.com/docs/embedded-vision/tevs/usage-guides/nvidia/quick-start/)
