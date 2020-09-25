#include $(call all-subdir-makefiles)
PROJ_PATH := $(call my-dir)
LOCAL_SHORT_COMMANDS := true
include $(CLEAR_VARS)

include $(PROJ_PATH)/euhatrtsp/Android.mk
