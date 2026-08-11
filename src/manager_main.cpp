#include <iostream>
#include <vector>      // 动态数组容器
#include <string>      // 字符串类
#include <unistd.h>    // Unix标准库
#include <sys/wait.h>  // 进程状态定义
#include <sys/stat.h>  // 创建管道
#include <utils.h>     // 日志头文件

// 定义结构体来储存子进程信息
struct ProcessInfo {
    pid_t pid;
    std::string name;
    std::string path;
};

// 定义管道名字和位置
#define PIPE_NAME "/tmp/x_pilot_pipe"

int main() {
    std::cout << "x_pilot 管理程序运行" << std::endl;

    // 如果管道创建失败
    if (mkfifo(PIPE_NAME, 0666) == -1) {
        if (errno != EEXIST) {
            perror("管道创建失败");
            return 1;
        }
    }

    // 如果管道创建成功
    std::cout << "[Manager] 管道准备就绪: " << PIPE_NAME << std::endl;

    // 创建结构体放置进程说明
    std::vector<ProcessInfo> children;

    auto start_process = [&](const std::string& name, const std::string& path) {
        pid_t pid = fork();
        if (pid == 0) {
            execlp(path.c_str(), path.c_str(), NULL);
            perror("进程启动失败");
            exit(1);
        } else if (pid > 0) {
            children.push_back({pid, name, path});
            std::cout << "[Manager] 启动进程: " << name << " (PID: " << pid << ")" << std::endl;
        }
    };

    // 启动两个子进程(通信和控制)
    start_process("comm_process", "./build/comm_process");
    start_process("control_process", "./build/control_process");

    std::cout << "[Manager] 所有进程已启动，开始监控..." << std::endl;

    // 监控循环, 保证进程挂掉之后重启
    while (true) {
        int status;

        pid_t result = waitpid(-1, &status, WNOHANG);

        if (result > 0) {
            // 说明有进程挂了
            std::string dead_name = "Unknown";
            std::string dead_path = "";

            // 寻找是谁并且移除掉
            for (auto it = children.begin(); it != children.end(); ++it) {
                if (it->pid == result) {
                    dead_name = it->name;
                    dead_path = it->path;
                    children.erase(it);
                    break;
                }
            }

            std::cout << "检测到进程 " << dead_name << "PID: " << result << "意外退出" << std::endl;
            LOG_ERROR("检测到进程挂掉, 尝试重启...");

            // 重启挂掉的进程
            if (!dead_path.empty()) {
                start_process(dead_name, dead_path);
            }
            LOG_INFO("进程重启成功!");
        } 

        sleep(1);
    }

    return 0;
}