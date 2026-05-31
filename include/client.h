#pragma once
#include <SFML/Network.hpp>
#include <moodycamel/concurrentqueue.h>
#include <thread>
#include <atomic>
#include <optional>
#include <condition_variable>

namespace miso {

class Client {
public:
    // 构造：服务器 IP、TCP 端口、UDP 时间同步端口
    Client(sf::IpAddress serverIp, unsigned short tcpPort, unsigned short udpPort = 54000);
    ~Client();

    // 连接服务器（阻塞直到 TCP 连接成功，并完成初始时间同步）
    bool connect();

    // 断开连接（停止所有线程）
    void disconnect();

    // 主动进行一次时间同步（阻塞直到收到一次新的同步应答）
    void syncTime();

    // 获取估算的服务器当前时间（微秒），基于本地时钟 + 最新偏移
    int64_t getTimestamp() const;

    // 发送一条消息给服务器（自动在数据包头部插入时间戳）
    bool sendMessage(sf::Packet packet);

    // 获取一条来自服务器的消息（若无则返回 nullopt）
    std::optional<sf::Packet> receiveMessage();

private:
    void networkThreadFunc();       // TCP 收发线程
    void timeSyncThreadFunc();      // UDP 时间同步后台线程

    sf::IpAddress serverIp_;
    unsigned short tcpPort_;
    unsigned short udpPort_;

    sf::TcpSocket tcpSocket_;
    sf::UdpSocket udpSocket_;

    std::thread netThread_;
    std::thread syncThread_;
    std::atomic<bool> running_{false};

    // 时间同步状态
    std::atomic<int64_t> timeOffsetUs_{0};  // 当前平滑偏移（微秒）
    std::mutex syncMutex_;
    std::condition_variable syncCv_;
    bool syncReady_ = false;                // 是否完成过至少一次同步（用于初始等待）
    bool stopSync_ = false;                 // 通知同步线程退出

    // 本地单调时钟（与 SFML 的 sf::Clock 配合）
    sf::Clock localClock_;

    // 无锁消息队列
    moodycamel::ConcurrentQueue<sf::Packet> incomingQueue_;  // 服务器 → 客户端
    moodycamel::ConcurrentQueue<sf::Packet> outgoingQueue_;  // 客户端 → 服务器
};

}  // namespace miso
