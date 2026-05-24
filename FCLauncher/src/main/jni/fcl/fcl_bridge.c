//
// Created by Tungsten on 2022/10/11.
// Modified for Explicitly Loading awt_headless Tunnel
//

#include "fcl_internal.h"

#include <android/native_window_jni.h>
#include <jni.h>
#include <android/log.h>
#include <assert.h>

#include <dlfcn.h> 

#define TUNNEL_LOAD_TAG "FCL_BRIDGE_LOADER"
#define LOG_LOAD_D(...) __android_log_print(ANDROID_LOG_DEBUG, TUNNEL_LOAD_TAG, __VA_ARGS__)
#define LOG_LOAD_E(...) __android_log_print(ANDROID_LOG_ERROR, TUNNEL_LOAD_TAG, __VA_ARGS__)
// ============================================================================

struct FCLInternal *fcl;

__attribute__((constructor)) void env_init() {
    char* strptr_env = getenv("FCL_ENVIRON");
    if (strptr_env == NULL) {
        __android_log_print(ANDROID_LOG_INFO, "Environ", "No FCL environ found, creating...");
        fcl = malloc(sizeof(struct FCLInternal));
        assert(fcl);
        memset(fcl, 0 , sizeof(struct FCLInternal));
        if (asprintf(&strptr_env, "%p", fcl) == -1)
            abort();
        setenv("FCL_ENVIRON", strptr_env, 1);
        free(strptr_env);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "Environ", "Found existing FCL environ: %s", strptr_env);
        fcl = (void*) strtoul(strptr_env, NULL, 0x10);
    }
    __android_log_print(ANDROID_LOG_INFO, "Environ", "%p", fcl);
}

JNIEXPORT void JNICALL Java_com_tungsten_fclauncher_bridge_FCLBridge_setFCLBridge(JNIEnv *env, jobject thiz, jobject fcl_bridge) {
    fcl->object_FCLBridge = (jclass)(*env)->NewGlobalRef(env, thiz);
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    // ======================== 【核心注入点：雷霆显式加载占位穿透库】 ========================
    LOG_LOAD_D("fcl 库已就绪，正在强行显式加载 libawt_headless.so...");
    
    // RTLD_NOW 表示立刻解析库里所有的符号，这会强行触发 awt_headless 的 constructor 构造函数
    void *handle = dlopen("libawt_headless.so", RTLD_NOW);
    
    if (handle == NULL) {
        // 如果找不到或者加载失败，把 Linux 底层的报错原因打出来（方便你调式路径或架构问题）
        LOG_LOAD_E("【警告】显式加载 libawt_headless.so 失败！原因: %s", dlerror());
    } else {
        LOG_LOAD_D("成功通过 dlopen 桥接拉起 libawt_headless.so 穿透核心！");
        // 加载完可以不用关闭 handle，让它在整个游戏生命周期里常驻内存
    }
    // ====================================================================================

    if (fcl->android_jvm == NULL) {
        fcl->android_jvm = vm;
        JNIEnv* env = 0;
        jint result = (*fcl->android_jvm)->AttachCurrentThread(fcl->android_jvm, &env, 0);
        if (result != JNI_OK || env == 0) {
            FCL_INTERNAL_LOG("Failed to attach thread to JavaVM.");
            abort();
        }
        jclass class_FCLBridge = (*env)->FindClass(env, "com/tungsten/fclauncher/bridge/FCLBridge");
        if (class_FCLBridge == 0) {
            FCL_INTERNAL_LOG("Failed to find class: com/tungsten/fclauncher/bridge/FCLBridge.");
            abort();
        }
        fcl->class_FCLBridge = (jclass)(*env)->NewGlobalRef(env, class_FCLBridge);
    }

    return JNI_VERSION_1_2;
}
