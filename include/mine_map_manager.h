#pragma once
#include "storage.h"
#include "exceptions.h"
#include <unordered_map>
#include <functional>
#include <vector>
#include <utility>
#include <array>

namespace app {

struct Cell {
    bool has_mine = false;
    bool is_revealed = false;
    bool is_flagged = false;
    int  adjacent_mines = 0;
    int  owner_id = 0;       // 0=无主
    int  terrain = 0;
};

struct ChunkGrid {
    static constexpr int SIZE = 16;
    std::array<std::array<Cell, SIZE>, SIZE> cells;
};

struct CachedChunk {
    int id = 0;
    int x = 0;
    int y = 0;
    ChunkGrid grid;
    bool dirty = false;
    int64_t last_access = 0;
};

struct Coord {
    int x, y;
    bool operator==(const Coord& o) const { return x == o.x && y == o.y; }
};

struct CoordHasher {
    std::size_t operator()(const Coord& c) const {
        return std::hash<int>()(c.x) ^ (std::hash<int>()(c.y) << 1);
    }
};

class ChunkManager;
class UserManager;

class MineMapManager {
public:
    MineMapManager(ChunkManager& chunkMgr, UserManager& userMgr,
                   int maxCached = 512, int flushIntervalSec = 60);

    Cell getCell(int worldX, int worldY);

    void modifyCell(int worldX, int worldY, int userId,
                    std::function<void(Cell&)> modifier);

    void placeMines(const std::vector<std::pair<int,int>>& positions, int userId);

    void flushAll(int userId);
    void flushChunk(int chunkX, int chunkY, int userId);
    void unloadChunk(int chunkX, int chunkY, int userId);
    size_t cacheSize() const;

    static std::string serializeGrid(const ChunkGrid& grid);
    static ChunkGrid deserializeGrid(const std::string& data);

private:
    ChunkManager& chunkMgr_;
    UserManager& userMgr_;
    std::unordered_map<Coord, CachedChunk, CoordHasher> cache_;
    int maxCached_;
    int64_t flushIntervalSec_;
    int64_t lastFlushTime_;

    static Coord worldToChunk(int wx, int wy);
    static Coord worldToLocal(int wx, int wy);

    CachedChunk& getOrLoadChunk(int chunkX, int chunkY, int userId);
    void writeBack(CachedChunk& cached, int userId);
    void autoFlushIfNeeded(int userId);
    void evictIfNeeded(int userId);
};

} // namespace app
