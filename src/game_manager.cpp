#include "game_manager.h"
#include "user_manager.h"
#include <sqlite_orm/sqlite_orm.h>

namespace app {

using namespace sqlite_orm;

GameManager::GameManager(Storage& storage, UserManager& userMgr)
    : storage(storage), userMgr(userMgr) {}

int GameManager::addGameRecord(int userId, int mode, int durationSec, int threeBv) {
    // 验证用户活跃
    if (!userMgr.isUserActive(userId)) {
        throw UserNotFoundException("ID " + std::to_string(userId));
    }
    // 验证模式有效
    if (mode < 1 || mode > 3) {
        throw InvalidGameModeException(mode);
    }
    // 验证数据合法性（用时和3BV应为非负数）
    if (durationSec < 0) {
        throw GameException("Duration cannot be negative");
    }
    if (threeBv < 0) {
        throw GameException("3BV cannot be negative");
    }

    GameRecord rec;
    rec.user_id = userId;
    rec.mode = mode;
    rec.played_at = getCurrentTimestamp();
    rec.duration_seconds = durationSec;
    rec.three_bv = threeBv;
    return storage.insert(rec);
}

std::vector<GameRecord> GameManager::getLeaderboard(int mode, int limit) const {
    if (mode < 1 || mode > 3) {
        throw InvalidGameModeException(mode);
    }
    return storage.get_all<GameRecord>(
        where(c(&GameRecord::mode) == mode),
        order_by(&GameRecord::duration_seconds).asc(),
        sqlite_orm::limit(limit)
    );
}

std::vector<GameRecord> GameManager::getUserRecords(int userId, int mode) const {
    // 验证用户存在（允许查询已删除用户的记录？这里选择允许查询，因为是历史记录）
    auto users = storage.get_all<User>(where(c(&User::id) == userId));
    if (users.empty()) {
        throw UserNotFoundException("ID " + std::to_string(userId));
    }

    if (mode == -1) {
        return storage.get_all<GameRecord>(
            where(c(&GameRecord::user_id) == userId),
            order_by(&GameRecord::played_at).desc()
        );
    } else {
        if (mode < 1 || mode > 3) {
            throw InvalidGameModeException(mode);
        }
        return storage.get_all<GameRecord>(
            where(c(&GameRecord::user_id) == userId and c(&GameRecord::mode) == mode),
            order_by(&GameRecord::played_at).desc()
        );
    }
}

} // namespace app
