#ifndef X_PILOT_PROTOCOL_H
#define X_PILOT_PROTOCOL_H

#include <string>
#include <vector>
#include <cstdint>
#include <json.hpp>

using json = nlohmann::json;

const int FRAME_HEADER_LEN = 4;

// 尝试从缓冲区头部拆出一个完整包
// 成功：返回 true，out_json 填入解析结果，buffer 移除已消费字节
// 失败（半包/解析失败）：返回 false，buffer 不变或移除坏包
bool parseFrame(std::vector<char>& buffer, json& out_json);

#endif