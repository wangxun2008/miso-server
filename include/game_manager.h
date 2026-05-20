#pragma once
#include "storage.h"
#include "exceptions.h"
#include <vector>
#include <optional>

namespace app {

class UserManager;

class GameManager {
public:
    GameManager(Storage& storage, UserManager& userMgr);

    // 添加一条游戏记录，返回记录ID
    int addGameRecord(int userId, int mode, int durationSec, int threeBv);

    // 获取指定模式的排行榜（按用时升序，用时越短越靠前）
    std::vector<GameRecord> getLeaderboard(int mode, int limit = 10) const;

    // 获取某用户的游戏记录（可按模式过滤，-1 表示全部模式）
    std::vector<GameRecord> getUserRecords(int userId, int mode = -1) const;

private:
    Storage& storage;
    UserManager& userMgr;
};

} // namespace app
