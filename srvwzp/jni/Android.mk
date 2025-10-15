# jni/Android.mk
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := srvwzp
LOCAL_SRC_FILES := ../native/srvwzp.c
LOCAL_LDLIBS    := -llog
# (Opcional) flags de compilação:
# LOCAL_CFLAGS   += -std=c11 -O2 -fvisibility=hidden
include $(BUILD_SHARED_LIBRARY)
