[![Technexion](https://github.com/TechNexion-Vision/TEV-Jetson_Camera_driver/assets/28101204/08cd2fa9-7333-4a16-819f-c69a3dbf290c)](https://www.technexion.com/products/embedded-vision/)

[![Producer: Technexion](https://img.shields.io/badge/Producer-Technexion-blue.svg)](https://www.technexion.com)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

## Introduction

[TechNexion Embedded Vision Solutions](https://www.technexion.com/products/embedded-vision/) provide embedded system developers access to high-performance, industrial-grade camera solutions to accelerate their time to market for embedded vision projects.

### Version 0.0.1 (Beta)
---

## Support JetPack Version

- [JetPack 5.1.1](https://developer.nvidia.com/embedded/jetpack-sdk-511) [[L4T 35.3.1]](https://developer.nvidia.com/embedded/jetson-linux-r3531)
- [JetPack 5.1.2](https://developer.nvidia.com/embedded/jetpack-sdk-512) [[L4T 35.4.1]](https://developer.nvidia.com/embedded/jetson-linux-r3541)

## Support Camera Modules

#### MIPI Cameras
- TEVS-AR0144-C
- TEVS-AR0234-C
- TEVS-AR0521-C
- TEVS-AR0522-C
- TEVS-AR0522-M
- TEVS-AR0821-C
- TEVS-AR1335-C

#### FPD-LINK Cameras
- VLS3-AR0144-C
- VLS3-AR0234-C
- VLS3-AR0521-C
- VLS3-AR0522-C
- VLS3-AR0522-M
- VLS3-AR0821-C
- VLS3-AR1335-C

[More Camera Products Details...](https://www.technexion.com/products/embedded-vision)
## Supported NVIDIA Jetson Developer Kit

- [Nvidia Jetson Orin NANO](https://developer.nvidia.com/embedded/learn/get-started-jetson-orin-nano-devkit)
- [Nvidia Jetson Xavier NX ](https://developer.nvidia.com/embedded/learn/get-started-jetson-xavier-nx-devkit) (reached EOL)

## Supported TechNexion TEK Series

- [TEK6040-ORIN-NANO](https://www.technexion.com/products/embedded-computing/aivision/tek6040-orin-nano/)
- [TEK8021-NX-V](https://www.technexion.com/product/tek8021-nx-v/)

---
## Install TN Camera on Jetson Developer Kit

### Adaptor for Nvidia **Jetson ORIN NANO Development Kit**

**TEV-RPI22 Adaptor for TEVS**

> Connect TEVS camera and TEV-RPI22 adaptor to **Jetson Orin Nano Developer Kit** directly. 

[![RPI-22](https://www.technexion.com/wp-content/uploads/2023/11/tevs-ar0144-c-s33-ir-rpi22.png)](https://www.technexion.com/products/embedded-vision/mipi-csi2/evk/tevs-ar0144-c-s33-ir-rpi22/)

**VLS3-ORIN-EVK Adaptor for VLS3**

> Follow the [video](https://www.youtube.com/watch?v=Ggu97E-KmsA) to connect VLS3 cameras and VLS3-ORIN-EVK adaptor to **Jetson Orin Nano Developer Kit**.

[![VLS3-ORIN-EVK](https://img.youtube.com/vi/Ggu97E-KmsA/0.jpg)](https://www.youtube.com/watch?v=Ggu97E-KmsA)



#### Method 1 - Using Pre-built modules

##### Preparation

We recommend following the [Getting Started Guide](https://developer.nvidia.com/embedded/learn/get-started-jetson-orin-nano-devkit) for Jetson Orin Nano Developer Kit.

1. Download pre-built modules.

```
wget https://download.technexion.com/demo_software/EVK/NVIDIA/OrinNano/pre-built-modules/latest/tn-camera-modules.tar.gz
```

2. uncompress the modules.

```shell
tar -xf tn-camera-modules.tar.gz
```

3. Run install script.

```shell'
cd tn_camera_modules/
sh tn-install.sh
```
Note: You should reboot the device after installation.

#### Method 2 - Build drivers from source code

1. Follow [TN BSP install steps](https://github.com/TechNexion-Vision/nvidia_jetson_tn_bsp)

2. Clone the source codes.

```shell
git clone https://github.com/TechNexion-Vision/TEV-Jetson_Camera_driver.git
```

---

## Bring up Camera by GStreamer

If you succeed in initialing the camera, you can follow the steps to open the camera.

1. Check the supported resolutions:

```shell
$ gst-device-monitor-1.0 Video/Source
```
2. Bring up the camera (/dev/video0) by Gstreamer pipeline:

```shell
DISPLAY=:0 gst-launch-1.0 v4l2src device=/dev/video0 ! \
"video/x-raw, format=(string)UYVY, width=(int)2592, height=(int)1944" ! \
nvvidconv ! nv3dsink sync=false
```

---
## WIKI Pages

[TechNexion cameras with Nvidia EVK guide](https://developer.technexion.com/docs/tevi-arxxxx-cameras-on-nvidia-jetson-nano)

[TechNexion Nvidia products guide](https://developer.technexion.com/docs/1)
