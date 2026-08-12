#include <iostream>
#include <thread>
#include <chrono>
#include <json.hpp>
#include <ipc.h>      // 复用项目里的 IPC 客户端
#include <utils.h>    // 复用日志

using json = nlohmann::json;

int main() {
    LOG_INFO("=== IPC 测试客户端启动 ===");

    // 1. 创建客户端对象
    UnixSocketClient client;

    // 2. 尝试连接服务端
    // 注意：确保 Manager 已经启动了 control_process
    if (!client.connectToServer("/tmp/x_pilot.ipc")) {
        LOG_ERROR("连接服务端失败");
        return -1;
    }
    LOG_INFO("服务端连接成功！");

    // 3. 构造第一条指令：SET_SPEED
    // 注意：根据你的协议设计，这里可以加上 type 字段，也可以不加，看 handleCommand 是否强校验
    // 你的 handleCommand 只检查了 "cmd" 字段，所以这里构造简单的即可
    json cmd_speed;
    cmd_speed["cmd"] = "SET_SPEED";
    cmd_speed["params"]["speed"] = 2.5; // 设置速度为 2.5 m/s

    LOG_INFO("发送指令: SET_SPEED (2.5)");
    if (!client.sendFrame(cmd_speed)) {
        LOG_ERROR("发送失败");
    }

    // 休息 2 秒，观察效果
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 4. 构造第二条指令：START_SPRAY
    json cmd_spray;
    cmd_spray["cmd"] = "START_SPRAY";

    LOG_INFO("发送指令: START_SPRAY");
    if (!client.sendFrame(cmd_spray)) {
        LOG_ERROR("发送失败");
    }

    // 休息 5 秒，让 view_process 打印一会儿数据，看看速度是否变了
    LOG_INFO("等待 5 秒观察效果...");
    std::this_thread::sleep_for(std::chrono::seconds(5));

    LOG_INFO("测试结束，客户端退出");
    return 0;
}
