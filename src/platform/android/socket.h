#pragma once

#include <SDL.h>
#include <jni.h>
#include <sys/types.h>


#include <android/log.h>

#define log(...) __android_log_print(ANDROID_LOG_WARN, "GBmulator", __VA_ARGS__)

static inline ssize_t send_pkt(int fd, const void *buf, size_t n, int flags) {
    return send(fd, buf, n, flags);
}

static inline ssize_t recv_pkt(int fd, void *buf, size_t n, int flags) {
    return recv(fd, buf, n, flags);
}

// TODO optimize by storing env and activity (maybe clazz) as global variables
// static inline ssize_t send_pkt(int fd, const void *buf, size_t n, int flags) {
//     JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();
//     jobject activity = (jobject) SDL_AndroidGetActivity();
//     jclass clazz = (*env)->GetObjectClass(env, activity);
//     jmethodID method_id = (*env)->GetMethodID(env, clazz, "linkSendData", "([B)I");

//     //log("SEND_PKT BEFORE");

//     jbyteArray array = (*env)->NewByteArray(env, n);
//     (*env)->SetByteArrayRegion(env, array, 0, n, (jbyte *) buf);
//     ssize_t ret = (*env)->CallIntMethod(env, activity, method_id, array);

//     //log("SEND_PKT AFTER");

//     (*env)->DeleteLocalRef(env, array);
//     (*env)->DeleteLocalRef(env, activity);
//     (*env)->DeleteLocalRef(env, clazz);
//     return ret;
// }

// TODO optimize by storing env and activity (maybe clazz) as global variables
// static inline ssize_t recv_pkt(int fd, void *buf, size_t n, int flags) {
//     JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();
//     jobject activity = (jobject) SDL_AndroidGetActivity();
//     jclass clazz = (*env)->GetObjectClass(env, activity);
//     jmethodID method_id = (*env)->GetMethodID(env, clazz, "linkReceiveData", "(I)[B");

//     //log("RECV_PKT BEFORE");

//     jbyteArray array = (*env)->CallObjectMethod(env, activity, method_id, n);
//     if (!array) return -1;
//     ssize_t len = (*env)->GetArrayLength(env, array);
//     if (len > n) return -1;

//     //log("RECV_PKT AFTER");

//     (*env)->GetByteArrayRegion(env, array, 0, len, (jbyte *) buf);

//     //log("RECV_PKT AFTER AFTER");

//     (*env)->DeleteLocalRef(env, array);
//     (*env)->DeleteLocalRef(env, activity);
//     (*env)->DeleteLocalRef(env, clazz);
//     return len;
// }
