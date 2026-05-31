#pragma once
#include "storage.h"
#include "exceptions.h"
#include <vector>
#include <optional>

namespace miso {

class UserManager;

class ChunkManager {
public:
    ChunkManager(Storage& storage, UserManager& userMgr);

    // 创建或更新区块：如果坐标已有活跃块则更新，否则创建
    // 返回操作后的区块信息
    Chunk createOrUpdateChunk(int x, int y, int userId, const std::string& data);

    // 获取活跃区块（deleted_at == 0）
    Chunk getChunk(int x, int y) const;

    // 软删除区块（设置 deleted_at）
    void deleteChunk(int x, int y);

    // 范围查询活跃区块（边界包含 min/max）
    std::vector<Chunk> getChunksInArea(int minX, int minY, int maxX, int maxY) const;

private:
    Storage& storage;
    UserManager& userMgr;
};

} // namespace miso
