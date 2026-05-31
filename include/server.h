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

// 服务器接收到的原始消息（游戏线程使用）
struct RawClientMessage {
    int clientId;
    int64_t timestamp;                  // 客户端发送时估算的服务器时间（微秒）
    std::optional<sf::Packet> packet;   // 空值表示客户端断开连接
};

// 服务器发送给指定客户端的消息
struct OutgoingMessage {
    int clientId;
    sf::Packet packet;
};

class Server {
public:
    // 构造：指定 TCP 端口，UDP 时间同步端口（默认 54000）
    explicit Server(unsigned short tcpPort, unsigned short udpPort = 54000);
    ~Server();

    // 启动服务（开始监听并启动网络线程）
    void start();
    // 停止服务（线程安全，阻塞等待网络线程退出）
    void stop();

    // 获取当前服务器时间（微秒），基于服务器启动以来的单调时钟
    int64_t getCurrentTimestamp() const;

    // 获取一条待处理的消息（内部维护小根堆）
    // timeWindow: 消息时间戳 + 窗口 >= 当前时间戳时，消息可被返回
    // 返回 nullopt 表示暂无到期消息（断开消息会立即返回，不受窗口限制）
    std::optional<RawClientMessage> getNextMessage(sf::Time timeWindow = sf::milliseconds(20));

    // 向指定客户端发送一条消息（不自动添加时间戳）
    void sendMessage(int clientId, sf::Packet packet);

private:
    void networkThreadFunc();   // 网络线程主函数

    // 用于小根堆排序
    struct HeapMessage {
        int64_t timestamp;
        int clientId;
        sf::Packet packet;          // 有效数据
        bool disconnected = false;  // 是否是断开连接消息

        // 小根堆：时间戳小的优先
        bool operator<(const HeapMessage& other) const {
            return timestamp > other.timestamp;
        }
    };

    const unsigned short tcpPort_;
    const unsigned short udpPort_;

    sf::TcpListener listener_;
    sf::UdpSocket udpSocket_;
    sf::Clock serverClock_;         // 服务器单调时钟

    std::thread netThread_;
    std::atomic<bool> running_{false};

    // 客户端管理（仅网络线程访问）
    std::unordered_map<int, sf::TcpSocket> clients_;
    int nextClientId_ = 1;
    std::mutex clientsMutex_;       // 保护 clients_ 的有限互斥（实际可无锁，但为安全加上）

    // 无锁队列：网络 → 游戏 和 游戏 → 网络
    moodycamel::ConcurrentQueue<RawClientMessage> incomingQueue_;
    moodycamel::ConcurrentQueue<OutgoingMessage> outgoingQueue_;

    // 小根堆（仅游戏线程访问，在 getNextMessage 中使用）
    std::priority_queue<HeapMessage> msgHeap_;
};

}  // namespace miso
