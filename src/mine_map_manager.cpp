#include "mine_map_manager.h"
#include "chunk_manager.h"
#include "user_manager.h"
#include <cstring>
#include <algorithm>

namespace app {

static int floorDiv(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((a < 0) != (b < 0))) --q;
    return q;
}

Coord MineMapManager::worldToChunk(int wx, int wy) {
    return { floorDiv(wx, ChunkGrid::SIZE), floorDiv(wy, ChunkGrid::SIZE) };
}

Coord MineMapManager::worldToLocal(int wx, int wy) {
    Coord chunk = worldToChunk(wx, wy);
    return { wx - chunk.x * ChunkGrid::SIZE, wy - chunk.y * ChunkGrid::SIZE };
}

std::string MineMapManager::serializeGrid(const ChunkGrid& grid) {
    std::string binary(ChunkGrid::SIZE * ChunkGrid::SIZE * 7, '\0');
    size_t idx = 0;
    for (int y = 0; y < ChunkGrid::SIZE; ++y) {
        for (int x = 0; x < ChunkGrid::SIZE; ++x) {
            const Cell& cell = grid.cells[y][x];
            uint8_t flags = 0;
            if (cell.has_mine)  flags |= 1;
            if (cell.is_revealed) flags |= 2;
            if (cell.is_flagged) flags |= 4;
            binary[idx] = static_cast<char>(flags);
            binary[idx + 1] = static_cast<char>(cell.adjacent_mines & 0xFF);
            int32_t owner = cell.owner_id;
            std::memcpy(&binary[idx + 2], &owner, sizeof(owner));
            binary[idx + 6] = static_cast<char>(cell.terrain & 0xFF);
            idx += 7;
        }
    }
    // 转为十六进制字符串
    static const char hex_chars[] = "0123456789ABCDEF";
    std::string hex;
    hex.reserve(binary.size() * 2);
    for (unsigned char c : binary) {
        hex.push_back(hex_chars[c >> 4]);
        hex.push_back(hex_chars[c & 0x0F]);
    }
    return hex;
}

ChunkGrid MineMapManager::deserializeGrid(const std::string& hex) {
    ChunkGrid grid;
    if (hex.size() < ChunkGrid::SIZE * ChunkGrid::SIZE * 7 * 2) {
        return grid; // 无效数据返回空网格
    }
    // 十六进制解码
    auto hexCharToInt = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return 0;
    };
    std::string binary(ChunkGrid::SIZE * ChunkGrid::SIZE * 7, '\0');
    for (size_t i = 0; i < binary.size(); ++i) {
        int high = hexCharToInt(hex[i * 2]);
        int low = hexCharToInt(hex[i * 2 + 1]);
        binary[i] = static_cast<char>((high << 4) | low);
    }
    size_t idx = 0;
    for (int y = 0; y < ChunkGrid::SIZE; ++y) {
        for (int x = 0; x < ChunkGrid::SIZE; ++x) {
            Cell& cell = grid.cells[y][x];
            uint8_t flags = static_cast<uint8_t>(binary[idx]);
            cell.has_mine = flags & 1;
            cell.is_revealed = flags & 2;
            cell.is_flagged = flags & 4;
            cell.adjacent_mines = static_cast<uint8_t>(binary[idx + 1]);
            std::memcpy(&cell.owner_id, &binary[idx + 2], sizeof(int32_t));
            cell.terrain = static_cast<uint8_t>(binary[idx + 6]);
            idx += 7;
        }
    }
    return grid;
}

MineMapManager::MineMapManager(ChunkManager& chunkMgr, UserManager& userMgr,
                               int maxCached, int flushIntervalSec)
    : chunkMgr_(chunkMgr), userMgr_(userMgr),
      maxCached_(maxCached), flushIntervalSec_(flushIntervalSec),
      lastFlushTime_(getCurrentTimestamp())
{}

CachedChunk& MineMapManager::getOrLoadChunk(int chunkX, int chunkY, int userId) {
    Coord coord{chunkX, chunkY};
    auto it = cache_.find(coord);
    if (it != cache_.end()) {
        it->second.last_access = getCurrentTimestamp();
        return it->second;
    }

    CachedChunk cached;
    cached.x = chunkX;
    cached.y = chunkY;
    try {
        Chunk dbChunk = chunkMgr_.getChunk(chunkX, chunkY);
        cached.id = dbChunk.id;
        cached.grid = deserializeGrid(dbChunk.data);
        cached.dirty = false;
    } catch (const ChunkNotFoundException&) {
        cached.id = 0;
        cached.grid = ChunkGrid{};
        cached.dirty = false;  // 新建空块不脏，等待首次修改
    }
    cached.last_access = getCurrentTimestamp();
    auto res = cache_.emplace(coord, std::move(cached));
    return res.first->second;
}

Cell MineMapManager::getCell(int worldX, int worldY) {
    Coord chunk = worldToChunk(worldX, worldY);
    Coord local = worldToLocal(worldX, worldY);
    CachedChunk& cached = getOrLoadChunk(chunk.x, chunk.y, 0);
    return cached.grid.cells[local.y][local.x];
}

void MineMapManager::modifyCell(int worldX, int worldY, int userId,
                                std::function<void(Cell&)> modifier) {
    if (!userMgr_.isUserActive(userId)) {
        throw UserNotFoundException("ID " + std::to_string(userId));
    }
    Coord chunk = worldToChunk(worldX, worldY);
    Coord local = worldToLocal(worldX, worldY);
    CachedChunk& cached = getOrLoadChunk(chunk.x, chunk.y, userId);
    modifier(cached.grid.cells[local.y][local.x]);
    cached.dirty = true;
    cached.last_access = getCurrentTimestamp();
    autoFlushIfNeeded(userId);
    evictIfNeeded(userId);
}

void MineMapManager::placeMines(const std::vector<std::pair<int,int>>& positions, int userId) {
    for (const auto& pos : positions) {
        modifyCell(pos.first, pos.second, userId, [](Cell& c) {
            c.has_mine = true;
        });
    }
}

void MineMapManager::writeBack(CachedChunk& cached, int userId) {
    std::string data = serializeGrid(cached.grid);
    Chunk dbChunk = chunkMgr_.createOrUpdateChunk(cached.x, cached.y, userId, data);
    cached.id = dbChunk.id;
    cached.dirty = false;
}

void MineMapManager::flushAll(int userId) {
    for (auto& pair : cache_) {
        if (pair.second.dirty) {
            writeBack(pair.second, userId);
        }
    }
    lastFlushTime_ = getCurrentTimestamp();
}

void MineMapManager::flushChunk(int chunkX, int chunkY, int userId) {
    Coord coord{chunkX, chunkY};
    auto it = cache_.find(coord);
    if (it != cache_.end() && it->second.dirty) {
        writeBack(it->second, userId);
    }
}

void MineMapManager::unloadChunk(int chunkX, int chunkY, int userId) {
    flushChunk(chunkX, chunkY, userId);
    cache_.erase(Coord{chunkX, chunkY});
}

size_t MineMapManager::cacheSize() const {
    return cache_.size();
}

void MineMapManager::autoFlushIfNeeded(int userId) {
    int64_t now = getCurrentTimestamp();
    if (now - lastFlushTime_ >= flushIntervalSec_) {
        flushAll(userId);
    }
}

void MineMapManager::evictIfNeeded(int userId) {
    while (cache_.size() > static_cast<size_t>(maxCached_)) {
        auto nonDirty = cache_.end();
        int64_t minAccess = std::numeric_limits<int64_t>::max();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (!it->second.dirty && it->second.last_access < minAccess) {
                nonDirty = it;
                minAccess = it->second.last_access;
            }
        }
        if (nonDirty != cache_.end()) {
            cache_.erase(nonDirty);
            continue;
        }
        // 全是脏块，淘汰最久未访问的脏块（先写回）
        auto oldestDirty = cache_.end();
        minAccess = std::numeric_limits<int64_t>::max();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second.dirty && it->second.last_access < minAccess) {
                oldestDirty = it;
                minAccess = it->second.last_access;
            }
        }
        if (oldestDirty != cache_.end()) {
            writeBack(oldestDirty->second, userId);
            cache_.erase(oldestDirty);
        } else {
            break;
        }
    }
}

} // namespace app
