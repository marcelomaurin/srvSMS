#include <jni.h>
#include <android/log.h>

#define LOG_TAG "srvwzp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/*
 * Java side example:
 * package com.example.srvwzp;
 * public class NativeSrv { public static native String msg(); }
 */

JNIEXPORT jstring JNICALL
Java_com_example_srvwzp_NativeSrv_msg(JNIEnv* env, jclass clazz) {
    LOGI("JNI msg() called from srvwzp (ndk-build).");
    return (*env)->NewStringUTF(env, "Hello from srvwzp (NDK ndk-build)!");
}
