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

<a href="https://www.technexion.com/products/embedded-vision/mipi-csi2/evk/tevs-ar0144-c-s33-ir-rpi22/" target="_blank">
 <img src="https://www.technexion.com/wp-content/uploads/2023/11/tevs-ar0144-c-s33-ir-rpi22.png" width="400" height="400" />
</a>

**VLS3-ORIN-EVK Adaptor for VLS3**

> Follow the [video](https://www.youtube.com/watch?v=Ggu97E-KmsA) to connect VLS3 cameras and VLS3-ORIN-EVK adaptor to **Jetson Orin Nano Developer Kit**.

<a href="http://www.youtube.com/watch?feature=player_embedded&v=Ggu97E-KmsA" target="_blank">
 <img src="https://img.youtube.com/vi/Ggu97E-KmsA/0.jpg" alt="Watch the video"  width="640" height="360" />
</a>

---



#### Method 1 - Using Technexion Pre-built Image

We provide pre-built images to install quickly on Jetson Orin Nano Developer Kit.

[TEV-RPI22 + TEVS Cameras](https://download.technexion.com/demo_software/EVK/NVIDIA/OrinNano/TEV-RPI22_Camera_Series/DiskImage/TEV-RPI22-TEVS_ubuntu-20.04_dp_SD_diskimg_20240105.zip)

[VLS3-ORIN-EVK + VLS3 Cameras](https://download.technexion.com/demo_software/EVK/NVIDIA/OrinNano/VLS3-ORIN-EVK/DiskImage/VLS3-ORIN-EVK-VLS3_ubuntu-20.04_dp_SD_diskimg_20240105.zip)

---

#### Method 2 - Using Technexion Pre-built modules

##### Preparation

We recommend following the [Getting Started Guide](https://developer.nvidia.com/embedded/learn/get-started-jetson-orin-nano-devkit) for Jetson Orin Nano Developer Kit.
After that, you can follow the below method to install TechNexion Cameras Driver.

1. Download pre-built modules.

```
wget https://download.technexion.com/demo_software/EVK/NVIDIA/OrinNano/pre-built-modules/latest/tn_camera_modules.tar.gz
```

2. uncompress the modules.

```shell
tar -xf tn_camera_modules.tar.gz
```

3. Run installation script.

```shell'
cd tn_camera_modules/
sh tn_install.sh
```

4. After you agree to continue the installation, select the pre-installed modules that you want. The default module is TEVS cameras.

```shell
$ sh tn_install.sh
****** TechNexion Camera Driver Installation ******
This installation is easy to install TechnNexion Camera Drivers for Nvidia
Jetson Orin NANO Development Kits. Before start to install camera driver,
You should BACKUP your image to avoid any file you lost while installing process.
Do you want to continue?[Y/n]Y
Continuing with the installation...
Install TN-CAM modules: vizionlink.ko
Install TN-CAM modules: tevs.ko
Install TN-CAM DTB file: tevs
Installed TN-CAM DTB file Done.
Install TN-CAM DTB file: vl316-vls
Installed TN-CAM DTB file Done.
Select modules:
    [1]: TEVS: TEVS Series MIPI Cameras with RPI22 Adaptor
    [2]: VLS3: VLS3 Series Cameras with VLS3-ORIN-EVK Adaptor
Which modules do you select?[default:1]
```
   
Note: You should reboot the device after installation.

---

#### Method 3 - Build drivers from source code

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
