#pragma once
#include "storage.h"
#include "exceptions.h"
#include <vector>

namespace miso {

class UserManager;

class OnlineManager {
public:
    OnlineManager(Storage& storage, UserManager& userMgr);

    // 用户上线（更新心跳，若不存在则创建）
    void userOnline(int userId);

    // 用户下线
    void userOffline(int userId);

    // 更新心跳（保持在线）
    void updateHeartbeat(int userId);

    // 检查用户是否在线（心跳未超时，默认60秒）
    bool isUserOnline(int userId, int timeoutSeconds = 60) const;

    // 获取当前在线用户列表（心跳未超时）
    std::vector<int> getOnlineUsers(int timeoutSeconds = 60) const;

private:
    Storage& storage;
    UserManager& userMgr;
};

} // namespace miso
