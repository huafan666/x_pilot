#include "protocol.h"
#include "utils.h"
#include <cstring>
#include <arpa/inet.h>

bool parseFrame(std::vector<char>& buffer, json& out_json) {
    while (true) {
        // 情况1：缓冲区连 4 字节头都不够 → 半包
        if (buffer.size() < FRAME_HEADER_LEN) {
            return false;
        }

        // 读出长度（网络字节序转主机字节序）
        uint32_t body_len = ntohl(*reinterpret_cast<uint32_t*>(buffer.data()));

        // 安全检查
        if (body_len > 1024 * 1024) {
            LOG_ERROR("收到超大帧长度: " + std::to_string(body_len));
            return false;
        }

        // 情况2：头有了但 body 不够 → 半包
        size_t total_len = FRAME_HEADER_LEN + body_len;
        if (buffer.size() < total_len) {
            return false;
        }

        // 情况3：完整包，拆出来
        std::string body(buffer.begin() + FRAME_HEADER_LEN,
                         buffer.begin() + total_len);

        // 移除已消费的字节
        buffer.erase(buffer.begin(), buffer.begin() + total_len);

        // 解析 JSON
        try {
            out_json = json::parse(body);
            return true;
        } catch (const json::parse_error& e) {
            LOG_ERROR("JSON 解析失败: " + std::string(e.what()));
            continue;  // 丢掉坏包，继续拆下一个
        }
    }
}