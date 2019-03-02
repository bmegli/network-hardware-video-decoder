#!/bin/bash
export PATH=$PATH:/data/meglickib/android-ndk/android-ndk-r13b/
#NDK=/data/meglickib/android-ndk/android-ndk-r13b
export NDK_PROJECT_PATH=.
ndk-build NDK_APPLICATION_MK=./Application.mk

cp libs/armeabi-v7a/*.so ../unity-network-hardware-video-decoder/Assets/Plugins/Android/


