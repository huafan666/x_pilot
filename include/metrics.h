#ifndef X_PILOT_METRICS_H
#define X_PILOT_METRICS_H

#include <string>
#include <map>

namespace Metrics {

// 发送一个埋点事件
// event_name: 事件名（如 "process_start"）
// fields: 键值对（如 {"process_name","manager"}, {"pid","123"}）
void emit(const std::string& event_name,
          const std::map<std::string, std::string>& fields = {});

}
#endif