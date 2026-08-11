#include <iostream>
#include <vector>      // 动态数组容器
#include <string>      // 字符串类
#include <unistd.h>    // Unix标准库
#include <sys/wait.h>  // 进程状态定义
#include <sys/stat.h>  // 创建管道

// 定义结构体来储存子进程信息
struct ProcessInfo {
    pid_t pid;
    std::string name;
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

    // 开启子进程(通信进程)
    pid_t pid_comm = fork();
    if (pid_comm == 0) {
        // 启动通信进程
        execlp("./build/comm_process", "./build/comm_process", NULL);
        // 如果到这里则失败
        perror("通信进程启动失败");
        exit(0);
    } else if (pid_comm > 0) {
        children.push_back(ProcessInfo{pid_comm, "comm_process"});
        std::cout << "[Manager] 已经启动了通信进程, PID: " << pid_comm << std::endl;
    }

    pid_t pid_ctrl = fork();
    if (pid_ctrl == 0) {
        // 启动通信进程
        execlp("./build/control_process", "./build/control_process", NULL);
        // 如果到这里则失败
        perror("通信进程启动失败");
        exit(0);
    } else if (pid_ctrl > 0) {
        children.push_back(ProcessInfo{pid_ctrl, "control_process"});
        std::cout << "[Manager] 已经启动了控制进程, PID: " << pid_ctrl << std::endl;
    }

    std::cout << "[Manager] 所有进程已经启动..." << std::endl;
    while (true) {
        sleep(10);
    }

    return 0;
}