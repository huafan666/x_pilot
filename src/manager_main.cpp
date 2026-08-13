#include <iostream>
#include <vector>      // 动态数组容器
#include <string>      // 字符串类
#include <unistd.h>    // Unix标准库
#include <sys/wait.h>  // 进程状态定义
#include <sys/stat.h>  // 创建管道
#include <utils.h>     // 日志头文件
#include <config.h>    // 配置头文件
#include <csignal>     // 信号处理
#include <chrono>      // 时间库
#include <thread>      // 线程休眠
#include <ctime>       // 计时
#include "metrics.h"   // 数据埋点

// 定义结构体来储存子进程信息
struct ProcessInfo {
    pid_t pid;
    std::string name;
    std::string path;
    int restart_count = 0;          // 记录重启次数
    time_t last_restart_time = 0;   // 记录上次重启的时间点
};

// 定义全局退出的标记
volatile sig_atomic_t g_should_exit = 0;

// 信号处理函数
void signal_handler(int signum) {
    g_should_exit = 1;
}

int main() {
    // 埋点：进程启动
    Metrics::emit("process_start", {
        {"process_name", "manager"},
        {"pid", std::to_string(getpid())}
    });

    // 捕获程序退出信号
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    // 忽略 SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    // 读取配置信息
    Config cfg = ConfigLoader::load("config/config.json");

    // 程序开始运行提示
    std::cout << "x_pilot 管理程序运行" << std::endl;

    // 创建结构体放置进程说明
    std::vector<ProcessInfo> children;

    auto start_process = [&](ProcessInfo& info) {
        pid_t pid = fork();
        if (pid == 0) {
            execlp(info.path.c_str(), info.path.c_str(), NULL);
            perror("进程启动失败");
            exit(1);
        } else if (pid > 0) {
            info.pid = pid;
            children.push_back(info);
            std::cout << "[Manager] 启动进程: " << info.name 
                      << " (PID: " << info.pid << ", Restart Count: " 
                      << info.restart_count << ")" << std::endl;
        } else {
            // 【父进程】fork 失败（新增这部分）
            perror("[Manager] fork 失败");
        }
    };

    // 启动两个子进程(通信和控制)
    ProcessInfo control_info = {0, "control_process", cfg.control_process_path, 0, 0};
    start_process(control_info);

    sleep(2);
    
    ProcessInfo comm_info = {0, "comm_process", cfg.comm_process_path, 0, 0};
    start_process(comm_info);

    if (!cfg.view_process_path.empty()) {
        ProcessInfo view_info = {0, "view_process", cfg.view_process_path, 0, 0};
        start_process(view_info);
    }

    std::cout << "[Manager] 所有进程已启动，开始监控..." << std::endl;

    // 监控循环, 保证进程挂掉之后重启
    while (!g_should_exit) {
        int status;

        pid_t result = waitpid(-1, &status, WNOHANG);

        // 说明有进程挂了
        if (result > 0) {
            // 保存挂掉的进程信息
            ProcessInfo dead_info;
            bool found = false;

            // 寻找是谁并且移除掉
            for (auto it = children.begin(); it != children.end(); ++it) {
                if (it->pid == result) {
                    dead_info = *it;
                    children.erase(it);
                    found = true;
                    break;
                }
            }

            // 重启挂掉的进程
            if (found) {
                std::cout << "检测到进程 " << dead_info.name << "PID: " << result << "意外退出" << std::endl;
                LOG_ERROR("检测到进程挂掉, 尝试重启...");

                // 埋点：子进程崩溃
                Metrics::emit("process_crash", {
                    {"process_name", dead_info.name},
                    {"exit_code", std::to_string(status)}
                });

                time_t now = time(nullptr);
                if ((now - dead_info.last_restart_time) < 10 && dead_info.restart_count >= 3) {
                    std::cout << "[Manager] 频繁崩溃！停止重启 " << dead_info.name << std::endl;
                    LOG_ERROR("频繁崩溃，停止重启该进程");
                } else {
                    dead_info.last_restart_time = now;
                    dead_info.restart_count++;
                    
                    // 等待3秒再重启
                    sleep(3);

                    // 重启子进程
                    start_process(dead_info);
                    LOG_INFO("进程重启成功");
                }
            }
        } 

        sleep(1);
    }

    // 如果退出之后也需要打印信息
    std::cout << "[Manager] 收到退出信号, 停止监控, 清理子进程" << std::endl;
    
    // 开始清理子进程
    for (const auto& child : children) {
        std::cout << "[Manager] 正在停止" << child.name << "PID: " << child.pid << std::endl;
        LOG_INFO("正在停止子进程");
        if (kill(child.pid, SIGTERM) == -1) {
            perror("发送 SIGTERM 失败");
            LOG_ERROR("发送 SIGTERM 失败");
        } else {
            std::cout << "[Manager] -> 已发送 SIGTERM" << std::endl;
            LOG_INFO("已发送 SIGTERM");
        }
    }

    // 等待 5 秒，给子进程留出清理时间
    std::cout << "[Manager] 等待子进程退出 (5秒)..." << std::endl;
    // 使用 C++11 的 sleep_for
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (!children.empty()) {
        // 检查是否超时
        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }

        // 非阻塞地回收任意一个子进程
        int status;
        pid_t result = waitpid(-1, &status, WNOHANG);

        if (result > 0) {
            // 找到是哪个子进程，从列表移除
            for (auto it = children.begin(); it != children.end(); ++it) {
                if (it->pid == result) {
                    std::cout << "[Manager] 进程 " << it->name
                              << " (PID: " << it->pid << ") 已退出" << std::endl;
                    LOG_INFO("进程 " + it->name + " 已退出");
                    children.erase(it);
                    break;
                }
            }
        } else if (result == 0) {
            // 没有子进程退出，睡 100ms 再查
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            // result == -1，通常是 ECHILD（没有子进程了）
            break;
        }
    }

    // 5 秒后还没退出的，强制 SIGKILL
    for (const auto& child : children) {
        std::cout << "[Manager] 进程 " << child.name
                  << " (PID: " << child.pid << ") 未退出，强制终止..." << std::endl;
        LOG_ERROR("进程 " + child.name + " 超时未退出, 强制终止");
        kill(child.pid, SIGKILL);
    }

    // 回收被 SIGKILL 的子进程，避免僵尸
    while (true) {
        int status;
        pid_t result = waitpid(-1, &status, WNOHANG);
        if (result <= 0) break;   // 没有更多子进程了
    }

    std::cout << "[Manager] 所有进程已清理完毕，Manager 退出。" << std::endl;
    LOG_INFO("所有进程清理完毕, 退出程序");

    return 0;
}