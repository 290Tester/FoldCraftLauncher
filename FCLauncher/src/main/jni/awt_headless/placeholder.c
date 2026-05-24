// 占位文件，用于创建空的awt_headless库
// 该库已改造成 fully embedded 穿透守护核心（包名已精准修正为 com.tungsten.fcl）

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <signal.h>
#include <time.h>
#include <android/log.h>

#define TUNNEL_TAG "FCL_HEADLESS_TUNNEL"
#define LOG_TUNNEL_D(...) __android_log_print(ANDROID_LOG_DEBUG, TUNNEL_TAG, __VA_ARGS__)
#define LOG_TUNNEL_E(...) __android_log_print(ANDROID_LOG_ERROR, TUNNEL_TAG, __VA_ARGS__)

// 🔥 引入你转换出来的二进制数据头文件
// 如果编译时提示找不到文件，可以尝试写成相对路径如 #include "../fcl/cloudflared_data.h"
#include "cloudflared_data.h"

// 将自定义事件记录到 libfcl.log
void write_headless_log(const char* message) {
    FILE *log_file = fopen("/storage/emulated/0/FCL/log/libfcl.log", "a");
    if (log_file != NULL) {
        time_t now;
        time(&now);
        struct tm *local = localtime(&now);
        fprintf(log_file, "[%04d-%02d-%02d %02d:%02d:%02d] [Headless_Tunnel] %s\n",
                local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
                local->tm_hour, local->tm_min, local->tm_sec, message);
        fclose(log_file);
    }
}

// 穿透释放与执行核心
void start_cloudflared_from_placeholder() {
    pid_t pid = fork();
    
    if (pid < 0) {
        LOG_TUNNEL_E("Fork 失败");
        return;
    }
    
    if (pid == 0) {
        // ====== 独立子进程 ======
        // 绑定死亡信号：当 FCL 或游戏主进程死掉时，内核会自动给当前子进程发送 SIGKILL 信号避免残留
        prctl(PR_SET_PDEATHSIG, SIGKILL); 

        // 建立外部存储 Log 目录
        mkdir("/storage/emulated/0/FCL", 0755);
        mkdir("/storage/emulated/0/FCL/log", 0755);

        write_headless_log("==== 占位库加载成功：穿透守护进程开始初始化 ====");

        // ✨ 精准锁定包名对应的私有绝对路径
        const char* bin_dir = "/data/data/com.tungsten.fcl/files/bin";
        const char* bin_path = "/data/data/com.tungsten.fcl/files/bin/cloudflared_android";

        // 创建多级私有文件夹，确保 files 和 bin 文件夹存在
        mkdir("/data/data/com.tungsten.fcl/files", 0755);
        mkdir(bin_dir, 0755);

        // 检查文件完整性，若不存在或大小不对则进行内存提取释放
        struct stat st;
        if (stat(bin_path, &st) != 0 || st.st_size != (long)cloudflared_android_len) {
            write_headless_log("本地穿透内核缺失，正在从 so 资产中提取...");

            FILE *fw = fopen(bin_path, "wb");
            if (fw != NULL) {
                fwrite(cloudflared_android, 1, cloudflared_android_len, fw);
                fclose(fw);
                write_headless_log("提取成功！");
            } else {
                write_headless_log("【严重错误】无法写入私有文件目录！请检查存储空间。");
                exit(-1);
            }
        } else {
            write_headless_log("检测到本地内核已存在且完整，跳过重复写操作。");
        }

        // 赋予可执行权限 (rwxr-xr-x)
        chmod(bin_path, 0755);

        // 重定向 stdout 和 stderr，把 cloudflared 本身的所有联网、握手日志也写进 libfcl.log
        freopen("/storage/emulated/0/FCL/log/libfcl.log", "a", stdout);
        freopen("/storage/emulated/0/FCL/log/libfcl.log", "a", stderr);

        // 固定穿透参数
        char* args[] = {
            (char*)bin_path,
            (char*)"access",
            (char*)"tcp",
            (char*)"--hostname",
            (char*)"minecraft.tianluo.us.ci",
            (char*)"--url",
            (char*)"0.0.0.0:25565",
            NULL
        };

        if (execv(bin_path, args) < 0) {
            perror("execv 失败");
            exit(-1);
        }
    } else {
        // ====== 父进程 ======
        LOG_TUNNEL_D("占位符异步穿透释放成功 (PID: %d)", pid);
    }
}

// 当这个动态库被 System.loadLibrary("awt_headless") 加载进内存时，系统自动触发此函数
__attribute__((constructor)) void init_placeholder_tunnel() {
    LOG_TUNNEL_D("libawt_headless.so 正在被 FCL 框架加载...");
    
    // 一被加载，雷霆拉起
    start_cloudflared_from_placeholder();
}
