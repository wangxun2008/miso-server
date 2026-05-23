#include "game_logic_manager.h"
#include "user_manager.h"
#include <unordered_set>
#include <queue>

namespace app {

// 将坐标对组合为 int64 作为哈希种子
static inline uint64_t coordHash(int x, int y) {
    return (static_cast<uint64_t>(x) << 32) | (static_cast<uint32_t>(y));
}

GameLogicManager::GameLogicManager(MineMapManager& mineMap, UserManager& userMgr)
    : mineMap_(mineMap), userMgr_(userMgr), rng_(std::random_device{}())
{}

void GameLogicManager::seedRngForCoord(int worldX, int worldY) {
    uint64_t seed = coordHash(worldX, worldY);
    // 为了给周边格子不同的随机，加入邻居相对坐标的混合可避免？此处简单使用当前坐标种子
    rng_.seed(seed);
}

std::vector<std::pair<int,int>> GameLogicManager::getNeighbors(int worldX, int worldY) const {
    std::vector<std::pair<int,int>> neighbors;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            neighbors.emplace_back(worldX + dx, worldY + dy);
        }
    }
    return neighbors;
}

void GameLogicManager::initializeCell(int worldX, int worldY) {
    Cell cell = mineMap_.getCell(worldX, worldY);
    if (cell.is_initialized) return;

    // 随机布雷：基于坐标种子，概率可调（这里固定 20% 概率）
    seedRngForCoord(worldX, worldY);
    bool hasMine = (rng_() % 100) < 20; // 20% 概率

    mineMap_.modifyCell(worldX, worldY, 0, [hasMine](Cell& c) {
        c.has_mine = hasMine;
        c.is_initialized = true;
        // 其他字段保持默认
    });
}

void GameLogicManager::calcAdjacentMines(int worldX, int worldY) {
    int count = 0;
    for (auto& n : getNeighbors(worldX, worldY)) {
        Cell neighbor = mineMap_.getCell(n.first, n.second);
        if (neighbor.has_mine) ++count;
    }
    mineMap_.modifyCell(worldX, worldY, 0, [count](Cell& c) {
        c.adjacent_mines = count;
    });
}

void GameLogicManager::ensureAreaInitialized(int worldX, int worldY) {
    // 首先初始化当前格
    initializeCell(worldX, worldY);
    // 然后初始化周围8格
    for (auto& n : getNeighbors(worldX, worldY)) {
        initializeCell(n.first, n.second);
    }
    // 计算周围雷数（当前格需要）
    calcAdjacentMines(worldX, worldY);
    // 周围格子也需要计算，因为后续可能直接翻开它们
    for (auto& n : getNeighbors(worldX, worldY)) {
        calcAdjacentMines(n.first, n.second);
    }
}

bool GameLogicManager::revealCell(int worldX, int worldY, int userId) {
    if (!userMgr_.isUserActive(userId)) {
        throw UserNotFoundException("ID " + std::to_string(userId));
    }
    ensureAreaInitialized(worldX, worldY);

    Cell current = mineMap_.getCell(worldX, worldY);
    if (current.is_revealed) {
        throw CellAlreadyRevealedException(worldX, worldY);
    }
    if (current.is_flagged) {
        // 标记的格子不能翻开，需要先取消标记
        throw GameLogicException("Cell is flagged, cannot reveal");
    }

    // 翻开当前格
    mineMap_.modifyCell(worldX, worldY, userId, [userId](Cell& c) {
        c.is_revealed = true;
        c.owner_id = userId;
    });

    if (current.has_mine) {
        return true; // 踩雷
    }

    // 如果周围雷数为0，递归翻开相邻格（BFS）
    if (current.adjacent_mines == 0) {
        std::queue<std::pair<int,int>> q;
        std::unordered_set<uint64_t> visited; // 用坐标哈希标记已处理
        q.emplace(worldX, worldY);
        visited.insert(coordHash(worldX, worldY));

		while (!q.empty()) {
			auto [cx, cy] = q.front(); q.pop();
			for (auto& n : getNeighbors(cx, cy)) {
				int nx = n.first, ny = n.second;
				uint64_t hash = coordHash(nx, ny);
				if (visited.count(hash)) continue;
				visited.insert(hash);

				Cell neighbor = mineMap_.getCell(nx, ny);
				// 只处理已初始化的格子，并且跳过已翻开、已标记、有雷的格子
				if (!neighbor.is_initialized || neighbor.is_revealed || neighbor.is_flagged || neighbor.has_mine)
					continue;

				// 翻开邻居
				mineMap_.modifyCell(nx, ny, userId, [userId](Cell& c) {
					c.is_revealed = true;
					c.owner_id = userId;
				});

				if (neighbor.adjacent_mines == 0) {
					q.emplace(nx, ny);
				}
			}
		}
    }
    return false;
}

void GameLogicManager::toggleFlag(int worldX, int worldY, int userId) {
    if (!userMgr_.isUserActive(userId)) {
        throw UserNotFoundException("ID " + std::to_string(userId));
    }
    ensureAreaInitialized(worldX, worldY);

    Cell cell = mineMap_.getCell(worldX, worldY);
    if (cell.is_revealed) {
        throw CellAlreadyRevealedException(worldX, worldY); // 已翻开不能标记
    }

    bool newFlag = !cell.is_flagged;
    mineMap_.modifyCell(worldX, worldY, userId, [userId, newFlag](Cell& c) {
        c.is_flagged = newFlag;
        c.owner_id = userId;
    });
}

void GameLogicManager::quickFlag(int worldX, int worldY, int userId) {
    if (!userMgr_.isUserActive(userId)) {
        throw UserNotFoundException("ID " + std::to_string(userId));
    }
    ensureAreaInitialized(worldX, worldY);

    Cell current = mineMap_.getCell(worldX, worldY);
    if (!current.is_revealed) {
        throw CellNotRevealedException(worldX, worldY);
    }
    if (current.adjacent_mines == 0) {
        return; // 周围无雷，无需标记
    }

    // 统计周围未翻开未标记格子数
    std::vector<std::pair<int,int>> toFlag;
    for (auto& n : getNeighbors(worldX, worldY)) {
        Cell neighbor = mineMap_.getCell(n.first, n.second);
        if (!neighbor.is_revealed && !neighbor.is_flagged) {
            toFlag.push_back(n);
        }
    }

    if (toFlag.size() != static_cast<size_t>(current.adjacent_mines)) {
        throw GameLogicException("Quick flag condition not met: adjacent unrevealed count "
                                 + std::to_string(toFlag.size()) + " != " + std::to_string(current.adjacent_mines));
    }

    // 标记这些格子
    for (auto& pos : toFlag) {
        mineMap_.modifyCell(pos.first, pos.second, userId, [userId](Cell& c) {
            c.is_flagged = true;
            c.owner_id = userId;
        });
    }
}

} // namespace app
