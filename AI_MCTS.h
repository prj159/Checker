#pragma once
#include "Board.h"
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <cstdint>

namespace AI {

// ========================== RAVE 统计 ==========================
// 跨树节点共享同一走法的统计信息
struct RAVEStats {
    int    visits = 0;
    double wins  = 0.0;
};

// 全局 RAVE 表：key = (from.q, from.r, to.q, to.r) 的打包值
inline uint64_t packMove(int fq, int fr, int tq, int tr) {
    return (static_cast<uint64_t>(static_cast<uint16_t>(fq + 16)) << 48)
         | (static_cast<uint64_t>(static_cast<uint16_t>(fr + 16)) << 32)
         | (static_cast<uint64_t>(static_cast<uint16_t>(tq + 16)) << 16)
         | (static_cast<uint64_t>(static_cast<uint16_t>(tr + 16)));
}
inline uint64_t packMove(const HexCoord& f, const HexCoord& t) {
    return packMove(f.q, f.r, t.q, t.r);
}

// ========================== 置换表 ==========================
// 棋盘哈希：简单 Zobrist 风格
struct TTEntry {
    int    visits = 0;
    double wins  = 0.0;
    int    depth = 0;        // 该条目是搜索到多深的结果
};

// 棋盘状态 -> TT条目
using TranspositionTable = std::unordered_map<uint64_t, TTEntry>;

// ========================== MCTS 树节点 ==========================
struct MCTSNode {
    HexCoord from;
    HexCoord to;
    Player  player;                          // 此节点代表的"下一步轮到谁"
    MCTSNode* parent = nullptr;
    std::vector<std::unique_ptr<MCTSNode>> children;
    std::vector<std::pair<HexCoord, HexCoord>> untriedMoves;

    int    visits = 0;
    double wins  = 0.0;                      // 从当前节点决策者的视角累积胜场

    MCTSNode(HexCoord f, HexCoord t, Player p, MCTSNode* par)
        : from(f), to(t), player(p), parent(par) {}
};

// ========================== 开局库 ==========================
struct OpeningEntry {
    HexCoord from;
    HexCoord to;
    int      wins   = 0;
    int      total  = 0;
};

// ========================== 对外接口 ==========================
MoveRecord getMove(const Board& bd, Player who);

// 无头模式：纯逻辑AI走棋（不依赖图形），返回走法
MoveRecord getMoveHeadless(const std::map<HexCoord, Player>& boardState,
                           int playerCount, Player who);

// 重置AI状态（新一局开始时）
void resetAI();

// 加载开局库文件
bool loadOpeningBook(const std::string& filename);

// 保存开局库文件
bool saveOpeningBook(const std::string& filename);

// 记录一局结果到开局库
void recordGameToOpeningBook(const std::vector<MoveRecord>& history,
                             Player winner, int playerCount);

// 访问/修改RAVE表（自对弈用）
std::unordered_map<uint64_t, RAVEStats>& getRAVETable();

// 保留上次搜索的根节点（调试用）
extern std::unique_ptr<MCTSNode> lastRoot;

// AI 等级 (1-3)
int& AI_LEVEL();

} // namespace AI
