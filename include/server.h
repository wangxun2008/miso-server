#pragma once
#include <moodycamel/concurrentqueue.h>
#include <SFML/Network.hpp>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <queue>
#include <functional>
#include <cstdint>
#include <optional>

namespace miso {

/**
 * 服务器接收到的原始请求（由游戏线程消费）。
 * 当 packet 为空时，表示客户端已断开连接。
 */
struct RawClientMessage {
    int clientId;
    int64_t timestamp;                  // 客户端发送时估算的服务器时间（微秒）
    int64_t requestId;                  // 客户端生成的请求ID
    std::optional<sf::Packet> packet;   // 用户数据，空值表示断开
};

/**
 * 服务器发送给指定客户端的回复。
 * 仅包含用户数据， requestId 会在发送时自动拼接到包头部。
 */
struct OutgoingReply {
    int clientId;
    int64_t requestId;
    sf::Packet packet;
};

/**
 * 游戏服务器主类。
 * 管理 TCP 连接、UDP 时间同步、消息队列和网络线程。
 */
class Server {
public:
    /**
     * 构造服务器。
     * @param tcpPort TCP 监听端口
     * @param udpPort UDP 时间同步端口
     */
    explicit Server(unsigned short tcpPort, unsigned short udpPort = 54000);
    ~Server();

    void start();   // 启动网络线程
    void stop();    // 停止服务器，阻塞等待线程结束

    int64_t getCurrentTimestamp() const;   // 获取服务器本地时钟（微秒）

    /**
     * 获取一条待处理的请求（基于时间窗口）。
     * 内部会按时间戳排序，仅当消息时间戳落在 [now - window, now] 范围内时才返回。
     * @param timeWindow 时间窗口长度，默认20ms
     * @return 请求消息，若无符合条件的请求则返回 nullopt
     */
    std::optional<RawClientMessage> getNextRequest(sf::Time timeWindow = sf::milliseconds(20));

    /**
     * 回复客户端。
     * 会自动将 requestId 拼接到数据包头部，用户只需提供用户数据包。
     * @param clientId 目标客户端 ID
     * @param requestId 对应的请求 ID
     * @param packet 用户数据（不含 requestId）
     */
    void sendReply(int clientId, int64_t requestId, sf::Packet packet);

private:
    void networkThreadFunc();   // 网络线程主循环

    // 用于消息排序的内部结构
    struct HeapMessage {
        int64_t timestamp;
        int clientId;
        int64_t requestId;
        sf::Packet packet;
        bool disconnected = false;

        bool operator<(const HeapMessage& other) const {
            return timestamp > other.timestamp;   // 最小堆
        }
    };

    const unsigned short tcpPort_;
    const unsigned short udpPort_;

    sf::TcpListener listener_;
    sf::UdpSocket udpSocket_;
    sf::Clock serverClock_;      // 服务器单调时钟，用于生成时间戳

    std::thread netThread_;
    std::atomic<bool> running_{false};

    std::unordered_map<int, sf::TcpSocket> clients_;   // clientId -> socket
    int nextClientId_ = 1;
    std::mutex clientsMutex_;

    moodycamel::ConcurrentQueue<RawClientMessage> incomingQueue_;   // 网络线程 -> 游戏线程
    moodycamel::ConcurrentQueue<OutgoingReply> outgoingQueue_;      // 游戏线程 -> 网络线程

    std::priority_queue<HeapMessage> msgHeap_;   // 临时堆，用于 getNextRequest 中的时间窗口排序
};

} // namespace miso
