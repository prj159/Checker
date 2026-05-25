#pragma once
#include "Board.h"
#include <memory>
#include <vector>
#include <map>

namespace AI {

// MCTS 树节点：存储完整走法 + 统计信息
struct MCTSNode {
    HexCoord from;                          // 走法起点
    HexCoord to;                            // 走法终点
    Player  player;                         // 此节点代表的"下一步轮到谁"
    MCTSNode* parent = nullptr;
    std::vector<std::unique_ptr<MCTSNode>> children;
    std::vector<std::pair<HexCoord, HexCoord>> untriedMoves;  // 尚未展开的走法池
    int    visits = 0;
    double wins  = 0.0;                     // 从当前节点决策者的视角累积胜场

    MCTSNode(HexCoord f, HexCoord t, Player p, MCTSNode* par)
        : from(f), to(t), player(p), parent(par) {}
};

// 对外接口：返回 AI 选定的走法
MoveRecord getMove(const Board& bd, Player who);

// 保留上次搜索的根节点（可用于调试/显示）
extern std::unique_ptr<MCTSNode> lastRoot;

// AI 等级 (1-3)，控制模拟次数
int& AI_LEVEL();

} // namespace AI
