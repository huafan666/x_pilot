#include <gtest/gtest.h>
#include <protocol.h>
#include <cstring>
#include <arpa/inet.h>

// 辅助函数：构造一个完整的帧（4字节头 + JSON body）
static std::vector<char> makeFrame(const std::string& jsonStr) {
    uint32_t len = htonl(jsonStr.size());
    std::vector<char> frame;
    frame.resize(4 + jsonStr.size());
    memcpy(frame.data(), &len, 4);
    memcpy(frame.data() + 4, jsonStr.data(), jsonStr.size());
    return frame;
}

// 测试1：单个完整包能正确解析
TEST(ProtocolTest, SingleCompleteFrame) {
    auto frame = makeFrame(R"({"cmd":"SET_SPEED","params":{"speed":2.5}})");
    auto buffer = frame;  // 复制一份

    json result;
    EXPECT_TRUE(parseFrame(buffer, result));
    EXPECT_EQ(result["cmd"], "SET_SPEED");
    EXPECT_EQ(result["params"]["speed"], 2.5);
    // 缓冲区应该被清空
    EXPECT_TRUE(buffer.empty());
}

// 测试2：半包（只有头，没有 body）应该返回 false
TEST(ProtocolTest, HalfFrameHeaderOnly) {
    std::string jsonStr = R"({"cmd":"START_SPRAY"})";
    uint32_t len = htonl(jsonStr.size());
    std::vector<char> buffer;
    buffer.resize(4);
    memcpy(buffer.data(), &len, 4);
    // 故意不写 body

    json result;
    EXPECT_FALSE(parseFrame(buffer, result));
    // 缓冲区应该不变
    EXPECT_EQ(buffer.size(), 4u);
}

// 测试3：半包（头 + 半个 body）
TEST(ProtocolTest, HalfFramePartialBody) {
    auto frame = makeFrame(R"({"cmd":"STOP_SPRAY"})");
    // 只保留前 6 个字节（4字节头 + 2字节body）
    std::vector<char> buffer(frame.begin(), frame.begin() + 6);

    json result;
    EXPECT_FALSE(parseFrame(buffer, result));
    EXPECT_EQ(buffer.size(), 6u);  // 不变
}

// 测试4：粘包（两个包粘在一起，第一次拆一个，第二次拆一个）
TEST(ProtocolTest, StickyTwoFrames) {
    auto frame1 = makeFrame(R"({"cmd":"SET_SPEED","params":{"speed":3.0}})");
    auto frame2 = makeFrame(R"({"cmd":"START_SPRAY"})");

    // 拼在一起
    std::vector<char> buffer;
    buffer.insert(buffer.end(), frame1.begin(), frame1.end());
    buffer.insert(buffer.end(), frame2.begin(), frame2.end());

    // 第一次：拆出第一个包
    json result1;
    EXPECT_TRUE(parseFrame(buffer, result1));
    EXPECT_EQ(result1["cmd"], "SET_SPEED");
    EXPECT_EQ(result1["params"]["speed"], 3.0);

    // 第二次：拆出第二个包
    json result2;
    EXPECT_TRUE(parseFrame(buffer, result2));
    EXPECT_EQ(result2["cmd"], "START_SPRAY");

    // 缓冲区清空
    EXPECT_TRUE(buffer.empty());
}

// 测试5：粘包（2.5个包，拆两个，剩半个）
TEST(ProtocolTest, StickyTwoAndHalfFrames) {
    auto frame1 = makeFrame(R"({"cmd":"SET_SPEED","params":{"speed":1.0}})");
    auto frame2 = makeFrame(R"({"cmd":"START_SPRAY"})");
    auto frame3 = makeFrame(R"({"cmd":"STOP_SPRAY"})");

    std::vector<char> buffer;
    buffer.insert(buffer.end(), frame1.begin(), frame1.end());
    buffer.insert(buffer.end(), frame2.begin(), frame2.end());
    // 第三个包只放前 4 字节（头）
    buffer.insert(buffer.end(), frame3.begin(), frame3.begin() + 4);

    // 拆第一个
    json r1;
    EXPECT_TRUE(parseFrame(buffer, r1));
    EXPECT_EQ(r1["cmd"], "SET_SPEED");

    // 拆第二个
    json r2;
    EXPECT_TRUE(parseFrame(buffer, r2));
    EXPECT_EQ(r2["cmd"], "START_SPRAY");

    // 第三个是半包，拆不出来
    json r3;
    EXPECT_FALSE(parseFrame(buffer, r3));
    EXPECT_EQ(buffer.size(), 4u);  // 剩 4 字节头
}

// 测试6：空缓冲区
TEST(ProtocolTest, EmptyBuffer) {
    std::vector<char> buffer;
    json result;
    EXPECT_FALSE(parseFrame(buffer, result));
    EXPECT_TRUE(buffer.empty());
}