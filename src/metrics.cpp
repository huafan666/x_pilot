#include "metrics.h"
#include "utils.h"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Metrics {

void emit(const std::string& event_name,
          const std::map<std::string, std::string>& fields) {
    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");

    // 拼成结构化的一行：[时间] event=xxx key1=val1 key2=val2
    std::string line = "[" + ss.str() + "] event=" + event_name;
    for (const auto& kv : fields) {
        line += " " + kv.first + "=" + kv.second;
    }

    // 单独写到 metrics.log，和普通日志分开
    std::ofstream file("log/metrics.log", std::ios::app);
    if (file.is_open()) {
        file << line << std::endl;
    }
}

}