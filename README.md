Moved to https://git.opendlv.org.

## OpenDLV Microservice to interface with MJPEG-over-HTTP-encapsulated video streams

This repository provides source code to interface with cameras providing an
MJPEG stream over HTTP for the OpenDLV software ecosystem. This microservice
provides the captured frames in two separate shared memory areas, one for a
picture in [I420 format](https://wiki.videolan.org/YUV/#I420)
and one in ARGB format.

This software is based in part on the work of the Independent JPEG Group.

[![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)


## Table of Contents
* [Dependencies](#dependencies)
* [Usage](#usage)
* [License](#license)


## Dependencies
You need a C++14-compliant compiler to compile this project. The following
dependency is shipped as part of the source distribution:

* [libcluon](https://github.com/chrberger/libcluon) - [![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)
* [libcurl](https://github.com/curl/curl) - [License](https://github.com/curl/curl/blob/master/COPYING)
* [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo) - [IJG License/BSD 3-Clause/zlib](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/master/LICENSE.md)
* [libyuv](https://chromium.googlesource.com/libyuv/libyuv/+/master) - [![License: BSD 3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause) - [Google Patent License Conditions](https://chromium.googlesource.com/libyuv/libyuv/+/master/PATENTS)


## Usage
This microservice is created automatically on changes to this repository via Docker's public registry for:
* [x86_64](https://hub.docker.com/r/chalmersrevere/opendlv-device-camera-mjpegoverhttp-amd64/tags/)
* [armhf](https://hub.docker.com/r/chalmersrevere/opendlv-device-camera-mjpegoverhttp-armhf/tags/)
* [aarch64](https://hub.docker.com/r/chalmersrevere/opendlv-device-camera-mjpegoverhttp-aarch64/tags/)

To run this microservice using our pre-built Docker multi-arch images to open
an OpenCV-encapsulated camera, simply start it as follows:

```
docker run --rm -ti --init --ipc=host --net=host -e DISPLAY=$DISPLAY -v /tmp:/tmp chalmersrevere/opendlv-device-camera-mjpegoverhttp-multi:v0.0.3 --url=http://192.168.0.123/myStream.mjpeg --width=640 --height=480
```

If you want to display the captured frames, simply append `--verbose` to the
commandline above; you might also need to enable access to your X11 server: `xhost +`.

The parameters to the application are:
* `--cid`: CID of the OD4Session to listen for cluon.data.RecorderCommand
* `--id:`: ID to use in case of multiple instances
* `--rec`: name of the recording file; default: YYYY-MM-DD_HHMMSS.rec
* `--recsuffix`: additional suffix to add to the .rec file
* `--remote`: listen for cluon.data.RecorderCommand to start/stop recording
* `--url`:       URL providing an MJPEG stream over http
* `--name.i420`: name of the shared memory for the I420 formatted image; when omitted, video0.i420 is chosen
* `--name.argb`: name of the shared memory for the I420 formatted image; when omitted, video0.argb is chosen
* `--width`:     desired width of a frame
* `--height`:    desired height of a frame
* `--freq`:      desired frame rate
* `--skip.i420`: don't decode MJPEG frame into i420 format; default: false
* `--skip.argb`: don't decode MJPEG frame into argb format; default: false
* `--verbose`:   display captured image


## License

* This project is released under the terms of the GNU GPLv3 License

