#ifndef X_PILOT_TYPES_H
#define X_PILOT_TYPES_H

#include <string>
#include <cstdint> // 使用固定的大小的数据类型

namespace x_pilot {
// 定义一个机器人的结构体
struct RobotState {
    double x;
    double y; 
    double z; 
    double speed;
    int32_t battary;
    bool is_spraying;       // 是否正在喷洒
    double sprayed_amount;  // 已经喷洒的量
    uint64_t timestamp;     // 状态时间戳（ms毫秒）

    //构造函数
    RobotState() : x(0), y(0), z(0), speed(0), battary(100), is_spraying(false), sprayed_amount(0), timestamp(0) {}
};

// 指令枚举, 地面站对机器人或无人机的命令类型
enum class CommandType {
    UNKNOWN = 0,
    SET_SPEED,      // 设置速度
    TAKE_OFF,       // 起飞
    LAND,           // 降落
    START_SPRAY,    // 开始喷洒
    STOP_SPRAY,     // 停止喷洒
    EMERGENCY_STOP  // 紧急停止
};

// 发给control的指令
struct Command {
    CommandType cmd_type; // 命令类型
    double param;         // 命令参数

    Command() : cmd_type(CommandType::UNKNOWN), param(0.0) {}
};
}

#endif // X_PILOT_TYPES_H