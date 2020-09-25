# EuhatRtsp
This is an open source code of rtsp stream player in Android device.

## Official site:　
[http://euhat.com/rtsp.html](http://euhat.com/rtsp.html) 

## It has features:
* rtsp stream playing.
* rtsp stream acquiring.
* automatic switch to hard decoder if available.
* auto reconnecting for rendering when interrupted.
* get rtsp url through ip camera onvif protocol. (todo)
* add ptz function. (todo)

## How to use:
1. open local.properties, modify sdk.dir and ndk.dir path.
2. open Android Studio, import project using build.gradle in EuhatRtsp root dir.
compile and load to device to run.

## Notice
1. The h264 stream must be 1920x1080 resolution.
2. If the h264 stream has no sps frame, you will not see video rendering in EuhatRtsp.

## Faq
1. http://euhat.com/wp/2020/04/11/how-to-record-the-h264-stream-using-euhatrtsp-lib/
2. http://euhat.com/wp/2020/04/11/whether-to-go-through-tcp-or-udp-using-euhatrtsp/
3. http://euhat.com/wp/2020/09/26/how-to-build-rtsp-stream-from-a-mp4-file-to-test-euhatrtsp/

Have any question, welcome to contact me through email: euhat@hotmail.com
