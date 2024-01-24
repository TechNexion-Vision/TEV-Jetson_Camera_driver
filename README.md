[![Technexion](https://github.com/TechNexion-Vision/TEV-Jetson_Camera_driver/assets/28101204/08cd2fa9-7333-4a16-819f-c69a3dbf290c)](https://www.technexion.com/products/embedded-vision/)

[![Producer: Technexion](https://img.shields.io/badge/Producer-Technexion-blue.svg)](https://www.technexion.com)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

## Introduction

[TechNexion Embedded Vision Solutions](https://www.technexion.com/products/embedded-vision/) provide embedded system developers access to high-performance, industrial-grade camera solutions to accelerate their time to market for embedded vision projects.

### Version 0.0.1 (Beta)
---
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
- [Nvidia Jetson Xavier NX ](https://developer.nvidia.com/embedded/learn/get-started-jetson-xavier-nx-devkit) ( reached EOL)

## Supported TechNexion TEK Series

- [TEK6040-ORIN-NANO](https://www.technexion.com/products/embedded-computing/aivision/tek6040-orin-nano/)
- [TEK8021-NX-V](https://www.technexion.com/product/tek8021-nx-v/)

---
## Install TN Camera on Jetson Developer Kit

#### Method 1
Follow [TN BSP install steps](https://github.com/TechNexion-Vision/nvidia_jetson_tn_bsp)

#### Method 2
(TBD)
---
## Bring up TN Camera by GStreamer

If you succeed in initialing the camera, you can follow the steps to open the camera.

1. Check the supported resolutions:

```shell
$ gst-device-monitor-1.0 Video/Source
```
2. Bring up the camera by Gstreamer pipeline:
   
```shell
DISPLAY=:0 gst-launch-1.0 v4l2src device=/dev/video0 ! \
"video/x-raw, format=(string)UYVY, width=(int)2592, height=(int)1944" ! \
nvvidconv ! nv3dsink sync=false
```

---
## WIKI Pages

[TechNexion cameras with Nvidia EVK guide](https://developer.technexion.com/docs/tevi-arxxxx-cameras-on-nvidia-jetson-nano)

[TechNexion Nvidia products guide](https://developer.technexion.com/docs/1)
