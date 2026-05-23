#pragma once
#include "mine_map_manager.h"
#include "exceptions.h"
#include <random>
#include <vector>
#include <cstdint>

namespace app {

class UserManager;

class GameLogicManager {
public:
    GameLogicManager(MineMapManager& mineMap, UserManager& userMgr);

    // 翻开格子，返回 true 表示踩雷
    bool revealCell(int worldX, int worldY, int userId);

    // 切换标记（旗帜）
    void toggleFlag(int worldX, int worldY, int userId);

    // 快速标记：若当前格已翻开且数字等于周围未翻开未标记格子数，则将这些格子标记
    void quickFlag(int worldX, int worldY, int userId);

private:
    MineMapManager& mineMap_;
    UserManager& userMgr_;

    // 确保一个格子及其周围8格都已经初始化（布雷）
    void ensureAreaInitialized(int worldX, int worldY);

    // 为一个格子进行随机布雷（仅当未初始化时），并设置已初始化
    void initializeCell(int worldX, int worldY);

    // 计算周围雷数，并更新格子的 adjacent_mines（要求周围均已初始化）
    void calcAdjacentMines(int worldX, int worldY);

    // 获取邻居坐标列表（8个方向）
    std::vector<std::pair<int,int>> getNeighbors(int worldX, int worldY) const;

    // 随机数发生器，基于坐标种子
    std::mt19937 rng_;
    void seedRngForCoord(int worldX, int worldY);
};

} // namespace app
