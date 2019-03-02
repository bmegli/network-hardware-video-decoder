LOCAL_PATH := $(call my-dir)
SRC_DIR := .
HVD_DIR := hardware-video-decoder
MLSP_DIR := minimal-latency-streaming-protocol

include $(CLEAR_VARS)

# FFmpeg
#LOCAL_C_INCLUDES += /data/meglickib/android-ndk/android-ndk-r13b/sources/FFmpeg/android/arm/include

## avcodec
LOCAL_MODULE := avcodec
LOCAL_SRC_FILES := libavcodec.so
LOCAL_EXPORT_C_INCLUDES := /data/meglickib/android-ndk/android-ndk-r13b/sources/FFmpeg/android/arm/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

## avutil
LOCAL_MODULE := avutil
LOCAL_SRC_FILES := libavutil.so
LOCAL_EXPORT_C_INCLUDES := /data/meglickib/android-ndk/android-ndk-r13b/sources/FFmpeg/android/arm/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)


## FFmpeg
LOCAL_C_INCLUDES := /data/meglickib/android-ndk/android-ndk-r13b/sources/FFmpeg/android/arm/include


# hardware-video-decoder
LOCAL_SRC_FILES += $(HVD_DIR)/hvd.c

# minimal-latency-streaming-protocl
LOCAL_SRC_FILES += $(MLSP_DIR)/mlsp.c


# NHVD

LOCAL_MODULE := nhvd
LOCAL_C_INCLUDES += $(SRC_DIR) \
						$(HVD_DIR) \
						$(MLSP_DIR)

LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES +=  $(SRC_DIR)/nhvd.cpp
LOCAL_SHARED_LIBRARIES += avcodec avutil
LOCAL_LDLIBS := -llog

# build
include $(BUILD_SHARED_LIBRARY)

