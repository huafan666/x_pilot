#include <gtest/gtest.h>
#include <config.h>
#include <fstream>

// 辅助函数：写一个临时配置文件
static std::string writeTempConfig(const std::string& content) {
    std::string path = "test_temp_config.json";
    std::ofstream file(path);
    file << content;
    file.close();
    return path;
}

// 测试1：正常配置能正确加载
TEST(ConfigTest, LoadNormalConfig) {
    std::string path = writeTempConfig(R"({
        "server_ip": "192.168.1.200",
        "server_port": 9999,
        "comm_process_path": "./build/comm_process",
        "control_process_path": "./build/control_process",
        "log_level": "DEBUG",
        "shm_path": "/test_shm",
        "heartbeat_interval": 2,
        "heartbeat_timeout": 10
    })");

    Config cfg = ConfigLoader::load(path);
    EXPECT_EQ(cfg.server_ip, "192.168.1.200");
    EXPECT_EQ(cfg.server_port, 9999);
    EXPECT_EQ(cfg.log_level, "DEBUG");
    EXPECT_EQ(cfg.shm_path, "/test_shm");
    EXPECT_EQ(cfg.heartbeat_interval, 2);
    EXPECT_EQ(cfg.heartbeat_timeout, 10);
}

// 测试2：缺少 heartbeat_interval，应该用默认值 1
TEST(ConfigTest, MissingFieldUsesDefault) {
    std::string path = writeTempConfig(R"({
        "server_ip": "127.0.0.1",
        "server_port": 8888
    })");

    Config cfg = ConfigLoader::load(path);
    EXPECT_EQ(cfg.server_ip, "127.0.0.1");
    EXPECT_EQ(cfg.server_port, 8888);
    // 缺少 heartbeat_interval，应该用默认值
    EXPECT_EQ(cfg.heartbeat_interval, 1);
    // 缺少 heartbeat_timeout，应该用默认值
    EXPECT_EQ(cfg.heartbeat_timeout, 5);
}

// 测试3：文件不存在，应该 exit（用 death test）
TEST(ConfigTest, FileNotExistExit) {
    // config.cpp 里文件不存在会 std::exit(1)
    // 用 ASSERT_DEATH 捕获进程退出
    ASSERT_EXIT(ConfigLoader::load("不存在的文件.json"),
                ::testing::ExitedWithCode(1),
                "配置文件打开失败");
}