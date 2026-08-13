#include <gtest/gtest.h>
#include <utils.h>
#include <fstream>
#include <cstdio>

// 测试字符串是否显示正确
TEST(LoggerTest, LevelStringCorrect) {
    LOG_WARN("测试警告消息");
    LOG_ERROR("测试错误消息");

    // 读日志文件
    std::time_t t = std::time(nullptr);
    char dateBuf[9] = {0};
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d", std::localtime(&t));
    std::string filename = "log/x_pilot_" + std::string(dateBuf) + ".log";

    std::ifstream file(filename);
    ASSERT_TRUE(file.is_open()) << "日志文件没生成: " << filename;

    std::string line;
    std::string all_content;
    while (std::getline(file, line)) {
        all_content += line + "\n";
    }

    // 验证 WARN 级别确实写成了 WARNING（不是 INFO）
    EXPECT_NE(all_content.find("[WARNING]"), std::string::npos)
        << "WARNING 级别没正确写入";

    // 验证 ERROR 级别确实写成了 ERROR
    EXPECT_NE(all_content.find("[ERROR]"), std::string::npos)
        << "ERROR 级别没正确写入";
}

// 测试：日志包含文件名和行号
TEST(LoggerTest, ContainsFileAndLine) {
    LOG_INFO("测试文件名行号");

    std::time_t t = std::time(nullptr);
    char dateBuf[9] = {0};
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d", std::localtime(&t));
    std::string filename = "log/x_pilot_" + std::string(dateBuf) + ".log";

    std::ifstream file(filename);
    std::string line;
    std::string last_line;
    while (std::getline(file, line)) {
        last_line = line;
    }

    // 验证包含文件名和行号格式 [文件名:行号]
    EXPECT_NE(last_line.find("test_logger.cpp:"), std::string::npos)
        << "日志缺少文件名:行号，实际内容: " << last_line;
}