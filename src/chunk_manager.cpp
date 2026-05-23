#include "chunk_manager.h"
#include "user_manager.h"
#include <sqlite_orm/sqlite_orm.h>

namespace app {

using namespace sqlite_orm;

ChunkManager::ChunkManager(Storage& storage, UserManager& userMgr)
    : storage(storage), userMgr(userMgr) {}

Chunk ChunkManager::createOrUpdateChunk(int x, int y, int userId, const std::string& data) {
    if (userId != 0 && !userMgr.isUserActive(userId)) {
        throw UserNotFoundException("ID " + std::to_string(userId));
    }

    auto guard = storage.transaction_guard();
    auto existing = storage.get_all<Chunk>(
        where(c(&Chunk::x) == x and c(&Chunk::y) == y and c(&Chunk::deleted_at) == 0)
    );

    if (!existing.empty()) {
        Chunk chunk = existing.front();
        chunk.data = data;
        chunk.last_updated_by = (userId == 0) ? std::nullopt : std::optional<int>(userId);
        chunk.updated_at = getCurrentTimestamp();
        storage.update(chunk);
        guard.commit();
        return chunk;
    } else {
        Chunk chunk;
        chunk.x = x;
        chunk.y = y;
        chunk.last_updated_by = (userId == 0) ? std::nullopt : std::optional<int>(userId);
        chunk.data = data;
        chunk.created_at = getCurrentTimestamp();
        chunk.updated_at = chunk.created_at;
        chunk.deleted_at = 0;
        chunk.id = storage.insert(chunk);
        guard.commit();
        return chunk;
    }
}

Chunk ChunkManager::getChunk(int x, int y) const {
    auto chunks = storage.get_all<Chunk>(
        where(c(&Chunk::x) == x and c(&Chunk::y) == y and c(&Chunk::deleted_at) == 0)
    );
    if (chunks.empty()) {
        throw ChunkNotFoundException(x, y);
    }
    return chunks.front();
}

void ChunkManager::deleteChunk(int x, int y) {
    auto chunks = storage.get_all<Chunk>(
        where(c(&Chunk::x) == x and c(&Chunk::y) == y and c(&Chunk::deleted_at) == 0)
    );
    if (chunks.empty()) {
        throw ChunkNotFoundException(x, y);
    }
    Chunk chunk = chunks.front();
    chunk.deleted_at = getCurrentTimestamp();
    storage.update(chunk);
}

std::vector<Chunk> ChunkManager::getChunksInArea(int minX, int minY, int maxX, int maxY) const {
    return storage.get_all<Chunk>(
        where(c(&Chunk::deleted_at) == 0
              and c(&Chunk::x) >= minX and c(&Chunk::x) <= maxX
              and c(&Chunk::y) >= minY and c(&Chunk::y) <= maxY)
    );
}

} // namespace app
