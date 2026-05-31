#pragma once
#include <SFML/Network.hpp>
#include <moodycamel/concurrentqueue.h>
#include <thread>
#include <atomic>
#include <optional>
#include <condition_variable>
#include <unordered_map>

namespace miso {

/**
 * 服务器回复的原始消息（由网络线程产生，游戏线程消费）。
 * packet 中不含 requestId，已剥离。
 */
struct RawServerMessage {
    int64_t requestId;
    sf::Packet packet;
};

/**
 * 游戏客户端类。
 * 管理与服务端的 TCP 连接、UDP 时间同步、请求发送和回复接收。
 */
class Client {
public:
    /**
     * 构造客户端。
     * @param serverIp 服务器 IP 地址
     * @param tcpPort 服务器 TCP 端口
     * @param udpPort 服务器 UDP 时间同步端口
     */
    Client(sf::IpAddress serverIp, unsigned short tcpPort, unsigned short udpPort = 54000);
    ~Client();

    bool connect();      // 连接服务器并完成初始时间同步
    void disconnect();   // 断开连接，停止所有线程

    void syncTime();                // 手动触发一次时间同步（阻塞直到完成）
    int64_t getTimestamp() const;   // 获取当前客户端时间（经过与服务器的偏移校正）

    /**
     * 发送一个请求。
     * 自动附加上当前时间戳和自增的 requestId。
     * @param packet 用户数据（不含头部）
     * @return 生成的 requestId，用于后续匹配回复
     */
    int64_t sendRequest(sf::Packet packet);

    /**
     * 获取指定请求的回复。
     * 若已收到则返回，否则返回 nullopt。
     * 该函数会消耗内部队列中所有已到达的回复，并缓存到映射中。
     */
    std::optional<sf::Packet> receiveReply(int64_t requestId);

private:
    void networkThreadFunc();   // 网络线程：收发 TCP 数据
    void timeSyncThreadFunc();  // 时间同步线程：周期性 UDP 测量

    sf::IpAddress serverIp_;
    unsigned short tcpPort_;
    unsigned short udpPort_;

    sf::TcpSocket tcpSocket_;
    sf::UdpSocket udpSocket_;

    std::thread netThread_;
    std::thread syncThread_;
    std::atomic<bool> running_{false};

    std::atomic<int64_t> nextRequestId_{1};

    // 时间同步相关
    std::atomic<int64_t> timeOffsetUs_{0};   // 本地时钟到服务器时钟的偏移量
    std::mutex syncMutex_;
    std::condition_variable syncCv_;
    bool syncReady_ = false;
    bool stopSync_ = false;

    sf::Clock localClock_;   // 客户端本地单调时钟

    moodycamel::ConcurrentQueue<RawServerMessage> incomingQueue_;   // 网络线程 -> 游戏线程
    moodycamel::ConcurrentQueue<sf::Packet> outgoingQueue_;        // 游戏线程 -> 网络线程

    std::unordered_map<int64_t, sf::Packet> pendingReplies_;   // 游戏线程缓存已收到的回复
};

} // namespace miso
