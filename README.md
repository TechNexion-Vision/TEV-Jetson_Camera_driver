[![Technexion](https://github.com/TechNexion-Vision/TEV-Jetson_Camera_driver/assets/28101204/08cd2fa9-7333-4a16-819f-c69a3dbf290c)](https://www.technexion.com/products/embedded-vision/)

[![Producer: Technexion](https://img.shields.io/badge/Producer-Technexion-blue.svg)](https://www.technexion.com)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

## Introduction

[TechNexion Embedded Vision Solutions](https://www.technexion.com/products/embedded-vision/) provide embedded system developers access to high-performance, industrial-grade camera solutions to accelerate their time to market for embedded vision projects.

### Version 0.0.1 (Beta)

---

## Support JetPack Version

- [JetPack 4.6.1](https://developer.nvidia.com/embedded/jetpack-sdk-461) [[L4T 32.7.1]](https://developer.nvidia.com/embedded/linux-tegra-r3271)


## Support Camera Modules
#### TEVI Series Cameras
- TEVI-OV5640  
- TEVI-AR0144-C
- TEVI-AR0234-C
- TEVI-AR0521-C
- TEVI-AR0522-C
- TEVI-AR0522-M
- TEVI-AR0821-C
- TEVI-AR1335-C

#### TEVS Series Cameras
- TEVS-AR0144-C
- TEVS-AR0234-C
- TEVS-AR0521-C
- TEVS-AR0522-C
- TEVS-AR0522-M
- TEVS-AR0821-C
- TEVS-AR1335-C


[More Camera Products Details...](https://www.technexion.com/products/embedded-vision)
## Supported NVIDIA Jetson Developer Kit

- [Nvidia Jetson NANO](https://developer.nvidia.com/embedded/jetson-nano-developer-kit) (reached EOL)

---
## Install TN Camera on Jetson Developer Kit

### Adaptor for Nvidia **Jetson NANO Development Kit**

> Connect TN camera with adaptor to **Jetson Nano Developer Kit** directly. 

<a href="https://www.technexion.com/products/embedded-vision/mipi-csi2/evk/tevs-ar0521-c-s85-ir-rpi15/" target="_blank">
 <img src="https://www.technexion.com/wp-content/uploads/2023/11/tevs-ar0521-c-s85-ir-rpi15.png" width="400" height="400" />
</a>

---

#### Method 1 - Using Technexion Pre-built modules

##### Preparation

We recommend following the [Getting Started Guide](https://developer.nvidia.com/embedded/learn/get-started-jetson-nano-devkit) for Jetson Nano Developer Kit.
After that, you can follow the below method to install TechNexion Cameras Driver.

1. Download pre-built modules.

```
wget https://download.technexion.com/demo_software/EVK/NVIDIA/JetsonNano/pre-built-modules/latest/tn-camera-modules-jetson-nano.tar.gz
```

2. uncompress the modules.

```shell
tar -xf tn-camera-modules-jetson-nano.tar.gz
```

3. Run installation script.

```shell'
cd tn-camera-modules-jetson-nano/
sh tn_install.sh
```

4. After you agree to continue the installation, select the pre-installed modules that you want. The default module is TEVS cameras.

```shell
$ sh tn_install.sh
[sudo] password for ubuntu:
****** TechNexion Camera Driver Installation ******
This installation is easy to install TechnNexion Camera Drivers for Nvidia
Jetson NANO Development Kits. Before start to install camera driver,
You should BACKUP your image to avoid any file you lost while installing process.
Do you want to continue?[Y/n]Y
Continuing with the installation...
Install EEPROM modules
Install TN-CAM modules: tevi_ov5640.ko
Install TN-CAM modules: tevi_ap1302.ko
Install TN-CAM modules: tevs.ko
Install TN-CAM DTB file: tn
Installed TN-CAM DTB file Done.
Install TN-CAM DTBO file: tevi-ov5640
Installed TN-CAM DTBO file Done.
Install TN-CAM DTBO file: tevi-ap1302
Installed TN-CAM DTBO file Done.
Install TN-CAM DTBO file: tevs
Installed TN-CAM DTBO file Done.
Select modules:
    [1]: TEVS: TEVS Series MIPI Cameras with TEVS-RPI15 Adaptor
    [2]: TEVI-AP1302: TEVI-AR Series Cameras with TEV-RPI15 Adaptor
    [3]: TEVI-OV5640: TEVI-OV5640 Cameras with TEV-RPI15 Adaptor
Which modules do you select?[default:1]
```
   
Note: You should reboot the device after installation.

---

#### Method 2 - Build drivers from source code

1. Follow [TN BSP install steps](https://github.com/TechNexion-Vision/nvidia_jetson_tn_bsp)

2. After downloading, you can build kernel image and modules manually by following [compile steps](https://developer.technexion.com/docs/compile-step).

---

## Bring up Camera by GStreamer

If you succeed in initialing the camera, you can follow the steps to open the camera.

1. Check the supported resolutions:

```shell
$ gst-device-monitor-1.0 Video/Source
Device found:

        name  : vi-output, tevi-ov5640 7-003c
        class : Video/Source
        caps  : video/x-raw, format=(string)YUY2, width=(int)2592, height=(int)1944, framerate=(fraction)15/1;
                video/x-raw, format=(string)YUY2, width=(int)1920, height=(int)1080, framerate=(fraction)30/1;
                video/x-raw, format=(string)YUY2, width=(int)1280, height=(int)720, framerate=(fraction)60/1;
                video/x-raw, format=(string)YUY2, width=(int)2592, height=(int)1944, framerate=(fraction)15/1;
                video/x-raw, format=(string)YUY2, width=(int)1920, height=(int)1080, framerate=(fraction)30/1;
                video/x-raw, format=(string)YUY2, width=(int)1280, height=(int)720, framerate=(fraction)60/1;
        properties:
                udev-probed = true
                device.bus_path = platform-54080000.vi
                sysfs.path = /sys/devices/50000000.host1x/54080000.vi/video4linux/video0
                device.subsystem = video4linux
                device.product.name = "vi-output\,\ tevi-ov5640\ 7-003c"
                device.capabilities = :capture:
                device.api = v4l2
                device.path = /dev/video0
                v4l2.device.driver = tegra-video
                v4l2.device.card = "vi-output\,\ tevi-ov5640\ 7-003c"
                v4l2.device.bus_info = platform:54080000.vi:0
                v4l2.device.version = 264701 (0x000409fd)
                v4l2.device.capabilities = 2216689665 (0x84200001)
                v4l2.device.device_caps = 69206017 (0x04200001)
        gst-launch-1.0 v4l2src ! ...
```
2. Bring up the camera (/dev/video0) with 1280x720 by Gstreamer pipeline:

```shell
DISPLAY=:0 gst-launch-1.0 v4l2src device=/dev/video0 ! "video/x-raw, format=(string)YUY2, width=(int)1280, height=(int)720" ! xvimagesink sync=false
```

### Troubleshooting

**1. Cannot Find Cameras**

If you cannot bring up the cameras, you can check if the video device does exist.

```shell
$ ls /dev/video*  # List all video devices
/dev/video0  /dev/video1
```
If you cannot see the devices, you should check if the drivers have been probed.

---
## WIKI Pages

[TechNexion cameras with Nvidia EVK guide](https://developer.technexion.com/docs/tevi-arxxxx-cameras-on-nvidia-jetson-nano)

[TechNexion Nvidia products guide](https://developer.technexion.com/docs/1)
