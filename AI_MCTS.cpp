/****************************************************************************
 *  AI_MCTS.cpp  --  MCTS AI v4：渐进加宽 + RAVE + 置换表 + softmax rollout
 *
 *  增强：
 *    1. 渐进加宽 (Progressive Widening)
 *    2. 启发式 PUCT 先验
 *    3. Softmax rollout（替代硬 ε-greedy）
 *    4. RAVE 跨节点走法统计
 *    5. 置换表 (Transposition Table)
 *    6. 增强启发式评估 v2
 *    7. 自适应 C_PUCT
 *    8. 开局库支持
 ***************************************************************************/
#include "AI_MCTS.h"
#include <set>
#include <random>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

namespace AI {

// ======================= 参数 =======================
static constexpr double TIME_BUDGET_SEC   = 5.0;     // 每次走棋最多 5 秒
static constexpr int    MIN_SIMULATIONS   = 200;      // 保底模拟次数
static constexpr double C_PUCT_BASE       = 2.0;      // 基础探索常数
static constexpr double ROLLOUT_TEMP      = 0.5;      // softmax 温度（越小越贪婪）
static constexpr double RAVE_EQUIV        = 500.0;    // RAVE 等效样本数

// ======================= 随机数 =======================
static std::mt19937& rng() {
    static std::mt19937 mt(
        static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())
    );
    return mt;
}
static std::uniform_real_distribution<double> uni01(0.0, 1.0);

// ======================= 全局状态 =======================
static std::unordered_map<uint64_t, RAVEStats> g_raveTable;
static std::vector<OpeningEntry> g_openingBook;  // 开局库
static bool g_openingLoaded = false;

std::unordered_map<uint64_t, RAVEStats>& getRAVETable() { return g_raveTable; }

// ======================= 目标区域表 =======================
static const std::vector<std::vector<HexCoord>>& getTargetZones(int playerCount) {
    static bool init = false;
    static std::vector<std::vector<HexCoord>> zones2, zones4, zones6;
    if (!init) {
        zones2 = {
            {{4,-8},{3,-7},{4,-7},{2,-6},{3,-6},{4,-6},{1,-5},{2,-5},{3,-5},{4,-5}},
            {{-4,8},{-3,7},{-4,7},{-2,6},{-3,6},{-4,6},{-1,5},{-2,5},{-3,5},{-4,5}}
        };
        zones4 = {
            {{4,-8},{3,-7},{4,-7},{2,-6},{3,-6},{4,-6},{1,-5},{2,-5},{3,-5},{4,-5}},
            {{-4,-1},{-3,-2},{-4,-2},{-2,-3},{-3,-3},{-4,-3},{-1,-4},{-2,-4},{-3,-4},{-4,-4}},
            {{-4,8},{-3,7},{-4,7},{-2,6},{-3,6},{-4,6},{-1,5},{-2,5},{-3,5},{-4,5}},
            {{4,1},{3,2},{4,2},{2,3},{3,3},{4,3},{1,4},{2,4},{3,4},{4,4}}
        };
        zones6 = {
            {{4,-8},{3,-7},{4,-7},{2,-6},{3,-6},{4,-6},{1,-5},{2,-5},{3,-5},{4,-5}},
            {{-4,-1},{-3,-2},{-4,-2},{-2,-3},{-3,-3},{-4,-3},{-1,-4},{-2,-4},{-3,-4},{-4,-4}},
            {{-4,8},{-3,7},{-4,7},{-2,6},{-3,6},{-4,6},{-1,5},{-2,5},{-3,5},{-4,5}},
            {{4,1},{3,2},{4,2},{2,3},{3,3},{4,3},{1,4},{2,4},{3,4},{4,4}},
            {{5,-1},{6,-2},{5,-2},{7,-3},{6,-3},{5,-3},{8,-4},{7,-4},{6,-4},{5,-4}},
            {{-5,1},{-6,2},{-5,2},{-7,3},{-6,3},{-5,3},{-8,4},{-7,4},{-6,4},{-5,4}}
        };
        init = true;
    }
    if (playerCount == 2) return zones2;
    if (playerCount == 4) return zones4;
    return zones6;
}

// 目标区域中心
static HexCoord targetCentroid(int playerCount, int pid) {
    static HexCoord cache[3][6]; static bool cached = false;
    if (!cached) {
        for (int pc : {2, 4, 6}) {
            int idx = (pc == 2 ? 0 : pc == 4 ? 1 : 2);
            const auto& zones = getTargetZones(pc);
            for (int p = 0; p < pc; ++p) {
                double sq = 0, sr = 0;
                for (const auto& h : zones[p]) { sq += h.q; sr += h.r; }
                cache[idx][p] = HexCoord(
                    static_cast<int>(std::round(sq / zones[p].size())),
                    static_cast<int>(std::round(sr / zones[p].size())));
            }
        }
        cached = true;
    }
    int idx = (playerCount == 2 ? 0 : playerCount == 4 ? 1 : 2);
    return cache[idx][pid];
}

// ======================= 六角距离 =======================
static inline int hexDist(const HexCoord& a, const HexCoord& b) {
    int dq = a.q - b.q, dr = a.r - b.r, ds = -dq - dr;
    return (std::abs(dq) + std::abs(dr) + std::abs(ds)) / 2;
}

// ======================= 棋盘哈希 =======================
static uint64_t hashBoard(const std::map<HexCoord, Player>& board, int playerCount) {
    uint64_t h = 0x9e3779b97f4a7c15ULL;
    for (const auto& kv : board) {
        uint64_t pos = (static_cast<uint64_t>(kv.first.q + 16) << 16)
                     | static_cast<uint64_t>(kv.first.r + 16);
        uint64_t val = static_cast<uint64_t>(static_cast<int>(kv.second) + 1);
        h ^= pos * 0x9e3779b97f4a7c15ULL + val + (h << 6) + (h >> 2);
    }
    h ^= static_cast<uint64_t>(playerCount) * 0x517cc1b727220a95ULL;
    return h;
}

// ======================= 走法生成 =======================
static void findJumpsRecursive(const std::map<HexCoord, Player>& board,
                                const HexCoord& from, std::vector<HexCoord>& moves,
                                std::map<HexCoord, bool>& visited) {
    visited[from] = true;
    const int dirs[6][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,-1},{-1,1}};
    for (auto& d : dirs) {
        HexCoord mid(from.q + d[0], from.r + d[1]);
        HexCoord dst(from.q + 2*d[0], from.r + 2*d[1]);
        auto it_mid = board.find(mid); auto it_dst = board.find(dst);
        if (it_mid != board.end() && it_mid->second != Player::None &&
            it_dst != board.end() && it_dst->second == Player::None && !visited[dst]) {
            moves.push_back(dst);
            findJumpsRecursive(board, dst, moves, visited);
        }
    }
}

static std::vector<HexCoord> getMovesForPiece(
    const std::map<HexCoord, Player>& board, const HexCoord& from) {
    std::vector<HexCoord> moves;
    const int dirs[6][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,-1},{-1,1}};
    for (auto& d : dirs) {
        HexCoord n(from.q + d[0], from.r + d[1]);
        auto it = board.find(n);
        if (it != board.end() && it->second == Player::None) moves.push_back(n);
    }
    std::map<HexCoord, bool> visited; visited[from] = true;
    for (auto& d : dirs) {
        HexCoord mid(from.q + d[0], from.r + d[1]);
        HexCoord dst(from.q + 2*d[0], from.r + 2*d[1]);
        auto it_mid = board.find(mid); auto it_dst = board.find(dst);
        if (it_mid != board.end() && it_mid->second != Player::None &&
            it_dst != board.end() && it_dst->second == Player::None && !visited[dst]) {
            moves.push_back(dst);
            findJumpsRecursive(board, dst, moves, visited);
        }
    }
    return moves;
}

static inline bool isJump(const HexCoord& from, const HexCoord& to) {
    return std::abs(from.q - to.q) > 1 || std::abs(from.r - to.r) > 1 ||
           std::abs((from.q - from.r) - (to.q - to.r)) > 1;
}

static std::vector<std::pair<HexCoord, HexCoord>> getAllMoves(
    const std::map<HexCoord, Player>& board, Player who) {
    std::vector<std::pair<HexCoord, HexCoord>> all;
    for (const auto& kv : board) {
        if (kv.second != who) continue;
        auto moves = getMovesForPiece(board, kv.first);
        for (const auto& dst : moves) all.emplace_back(kv.first, dst);
    }
    return all;
}

static void applyMove(std::map<HexCoord, Player>& board,
                       const HexCoord& from, const HexCoord& to) {
    board[to] = board[from];
    board[from] = Player::None;
}

// ======================= 终局检测 =======================
static bool isGameOver(const std::map<HexCoord, Player>& board, int playerCount) {
    const auto& zones = getTargetZones(playerCount);
    for (int pl = 0; pl < playerCount; ++pl) {
        bool allInTarget = true;
        for (const auto& kv : board) {
            if (static_cast<int>(kv.second) != pl) continue;
            bool inTarget = false;
            for (const auto& t : zones[pl]) if (t == kv.first) { inTarget = true; break; }
            if (!inTarget) { allInTarget = false; break; }
        }
        if (allInTarget) return true;
    }
    return false;
}

static Player getWinner(const std::map<HexCoord, Player>& board, int playerCount) {
    const auto& zones = getTargetZones(playerCount);
    for (int pl = 0; pl < playerCount; ++pl) {
        bool allInTarget = true;
        for (const auto& kv : board) {
            if (static_cast<int>(kv.second) != pl) continue;
            bool inTarget = false;
            for (const auto& t : zones[pl]) if (t == kv.first) { inTarget = true; break; }
            if (!inTarget) { allInTarget = false; break; }
        }
        if (allInTarget) return static_cast<Player>(pl);
    }
    return Player::None;
}

// ======================= Softmax Rollout =======================
static void selectRolloutMove(
    const std::vector<std::pair<HexCoord, HexCoord>>& moves,
    const HexCoord& targetCenter,
    std::pair<HexCoord, HexCoord>& chosen) {
    if (moves.empty()) return;
    const int n = static_cast<int>(moves.size());
    std::vector<double> scores(n);
    double maxScore = -1e9;
    for (int i = 0; i < n; ++i) {
        int d = hexDist(moves[i].second, targetCenter);
        bool jump = isJump(moves[i].first, moves[i].second);
        scores[i] = (jump ? 30.0 : 0.0) - static_cast<double>(d);
        if (scores[i] > maxScore) maxScore = scores[i];
    }
    double sumW = 0.0;
    std::vector<double> weights(n);
    for (int i = 0; i < n; ++i) {
        weights[i] = std::exp((scores[i] - maxScore) / ROLLOUT_TEMP);
        sumW += weights[i];
    }
    double r = uni01(rng()) * sumW, acc = 0.0;
    for (int i = 0; i < n; ++i) {
        acc += weights[i];
        if (r <= acc) { chosen = moves[i]; return; }
    }
    chosen = moves.back();
}

static Player rolloutWinner(const std::map<HexCoord, Player>& boardState,
                             int playerCount, int startPlayerIdx, int maxSteps = 120) {
    auto board = boardState; int cur = startPlayerIdx;
    for (int step = 0; step < maxSteps; ++step) {
        if (isGameOver(board, playerCount)) return getWinner(board, playerCount);
        Player who = static_cast<Player>(cur);
        auto moves = getAllMoves(board, who);
        if (moves.empty()) { cur = (cur + 1) % playerCount; continue; }
        HexCoord tc = targetCentroid(playerCount, cur);
        std::pair<HexCoord, HexCoord> pick;
        selectRolloutMove(moves, tc, pick);
        applyMove(board, pick.first, pick.second);
        cur = (cur + 1) % playerCount;
    }
    return Player::None;
}

// ======================= 增强启发式评估 v2 =======================
static double heuristicEval(const std::map<HexCoord, Player>& board,
                             Player aiPlayer, int playerCount) {
    const int aiPid = static_cast<int>(aiPlayer);
    auto evalPlayer = [&](int pid) -> double {
        const HexCoord tc = targetCentroid(playerCount, pid);
        const auto& targetZone = getTargetZones(playerCount)[pid];
        std::set<HexCoord> targetSet(targetZone.begin(), targetZone.end());
        int total = 0, maxDist = 0, blocked = 0, inTarget = 0;
        double sumDist = 0.0;
        for (const auto& kv : board) {
            if (static_cast<int>(kv.second) != pid) continue;
            ++total;
            int d = hexDist(kv.first, tc); sumDist += d;
            if (d > maxDist) maxDist = d;
            if (targetSet.count(kv.first)) ++inTarget;
            const int dirs[6][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,-1},{-1,1}};
            int free = 0;
            for (auto& dd : dirs) {
                HexCoord n(kv.first.q + dd[0], kv.first.r + dd[1]);
                auto it = board.find(n);
                if (it != board.end() && it->second == Player::None) ++free;
            }
            if (free <= 1) ++blocked;
        }
        if (total == 0) return 1.0;
        double avgDist = sumDist / total;
        double progress = 1.0 - (avgDist / 16.0);
        if (progress < 0.0) progress = 0.0; if (progress > 1.0) progress = 1.0;
        double targetBonus = static_cast<double>(inTarget) / total * 0.15;
        double lagPenalty  = static_cast<double>(maxDist) / 16.0 * 0.25;
        double blockPenalty = static_cast<double>(blocked) / total * 0.20;
        return progress + targetBonus - lagPenalty - blockPenalty;
    };
    double aiScore = evalPlayer(aiPid);
    double worstOpp = 1.0;
    for (int p = 0; p < playerCount; ++p) {
        if (p == aiPid) continue;
        double oppScore = evalPlayer(p);
        if (oppScore < worstOpp) worstOpp = oppScore;
    }
    double diff = aiScore - worstOpp;
    if (diff > 1.0) diff = 1.0; if (diff < -1.0) diff = -1.0;
    return diff;
}

// ======================= 开局库 =======================
bool loadOpeningBook(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) return false;
    g_openingBook.clear();
    size_t count; in.read(reinterpret_cast<char*>(&count), sizeof(count));
    for (size_t i = 0; i < count; ++i) {
        OpeningEntry e;
        in.read(reinterpret_cast<char*>(&e.from.q), sizeof(int));
        in.read(reinterpret_cast<char*>(&e.from.r), sizeof(int));
        in.read(reinterpret_cast<char*>(&e.to.q), sizeof(int));
        in.read(reinterpret_cast<char*>(&e.to.r), sizeof(int));
        in.read(reinterpret_cast<char*>(&e.wins), sizeof(int));
        in.read(reinterpret_cast<char*>(&e.total), sizeof(int));
        g_openingBook.push_back(e);
    }
    g_openingLoaded = true;
    return true;
}

bool saveOpeningBook(const std::string& filename) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;
    size_t count = g_openingBook.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const auto& e : g_openingBook) {
        out.write(reinterpret_cast<const char*>(&e.from.q), sizeof(int));
        out.write(reinterpret_cast<const char*>(&e.from.r), sizeof(int));
        out.write(reinterpret_cast<const char*>(&e.to.q), sizeof(int));
        out.write(reinterpret_cast<const char*>(&e.to.r), sizeof(int));
        out.write(reinterpret_cast<const char*>(&e.wins), sizeof(int));
        out.write(reinterpret_cast<const char*>(&e.total), sizeof(int));
    }
    return true;
}

// 在开局库中匹配当前局面的走法
static bool queryOpeningBook(const std::vector<MoveRecord>& history,
    HexCoord& from, HexCoord& to) {
    if (!g_openingLoaded || g_openingBook.empty()) return false;
    int moveNum = static_cast<int>(history.size());
    for (const auto& e : g_openingBook) {
        // 简化：按走法序号匹配
        // 完整实现需按棋盘状态匹配，这里先做基本版本
        if (e.total >= 5 && static_cast<double>(e.wins) / e.total >= 0.55) {
            from = e.from; to = e.to; return true;
        }
    }
    return false;
}

void recordGameToOpeningBook(const std::vector<MoveRecord>& history,
                              Player winner, int playerCount) {
    if (history.empty()) return;
    // 取每步走法，归入对应玩家的开局库
    for (const auto& mv : history) {
        if (mv.fromPlayer != winner) continue; // 只记录胜者走法
        bool found = false;
        for (auto& e : g_openingBook) {
            if (e.from == mv.from && e.to == mv.to) {
                e.wins++; e.total++; found = true; break;
            }
        }
        if (!found) {
            OpeningEntry e;
            e.from = mv.from; e.to = mv.to; e.wins = 1; e.total = 1;
            g_openingBook.push_back(e);
        }
    }
    // 败者走法也记录（不计胜场）
    for (const auto& mv : history) {
        if (mv.fromPlayer == winner) continue;
        bool found = false;
        for (auto& e : g_openingBook) {
            if (e.from == mv.from && e.to == mv.to) {
                e.total++; found = true; break;
            }
        }
        if (!found) {
            OpeningEntry e;
            e.from = mv.from; e.to = mv.to; e.wins = 0; e.total = 1;
            g_openingBook.push_back(e);
        }
    }
}

// ======================= 自适应 C_PUCT =======================
static double adaptiveCPuct(double avgProgress) {
    // 开局（进度低）→ 高探索；终局（进度高）→ 多利用
    // avgProgress: 0(开始) ~ 1(快赢了)
    return C_PUCT_BASE * (1.5 - 0.8 * avgProgress);
}

// ======================= 启发式先验 =======================
static double heuristicPrior(const HexCoord& from, const HexCoord& to,
                              const HexCoord& targetCenter) {
    int dBefore = hexDist(from, targetCenter);
    int dAfter  = hexDist(to, targetCenter);
    double progressGain = static_cast<double>(dBefore - dAfter);
    double jumpBonus = isJump(from, to) ? 5.0 : 0.0;
    double h = (progressGain + jumpBonus) / 10.0;
    if (h < 0.05) h = 0.05;  // 最低先验避免零概率
    if (h > 1.0) h = 1.0;
    return h;
}

// ======================= MCTS 主搜索 =======================
MoveRecord getMove(const Board& bd, Player who) {
    return getMoveHeadless(bd.getBoardState(), bd.getPlayerCount(), who);
}

MoveRecord getMoveHeadless(const std::map<HexCoord, Player>& origBoard,
                            int playerCount, Player who) {
    const int aiPid = static_cast<int>(who);

    // ---- 计算平均进度（用于自适应C_PUCT）----
    double totalProgress = 0.0;
    for (int p = 0; p < playerCount; ++p) {
        HexCoord tc = targetCentroid(playerCount, p);
        double sum = 0; int cnt = 0;
        for (const auto& kv : origBoard) {
            if (static_cast<int>(kv.second) == p) {
                sum += hexDist(kv.first, tc); ++cnt;
            }
        }
        if (cnt > 0) totalProgress += (1.0 - (sum / cnt) / 16.0);
    }
    double avgProgress = totalProgress / playerCount;

    // ---- 构造根节点 ----
    auto root = std::make_unique<MCTSNode>(HexCoord(0,0), HexCoord(0,0), who, nullptr);
    {
        auto moves = getAllMoves(origBoard, who);
        for (auto& m : moves) root->untriedMoves.push_back(m);
    }
    if (root->untriedMoves.empty()) return {};

    // ---- 置换表（本局搜索）----
    TranspositionTable tt;

    // ---- 时间预算循环 ----
    auto startTime = std::chrono::steady_clock::now();
    int simCount = 0;
    double cPUCT = adaptiveCPuct(avgProgress);

    while (true) {
        ++simCount;

        // == ① Selection (渐进加宽 + 启发式PUCT) ==
        MCTSNode* node = root.get();
        auto board = origBoard;
        std::vector<std::pair<MCTSNode*, uint64_t>> path; // (node, moveKey) for RAVE backprop

        while (node->untriedMoves.empty() && !node->children.empty()) {
            int nodePid = static_cast<int>(node->player);
            HexCoord centroid = targetCentroid(playerCount, nodePid);
            MCTSNode* best = nullptr;
            double bestScore = -1e9;
            const double logN = std::log(static_cast<double>(node->visits + 1));

            for (auto& c : node->children) {
                double Q = (c->visits > 0)
                         ? c->wins / static_cast<double>(c->visits) : 0.0;

                // RAVE: 全局走法统计
                uint64_t mKey = packMove(c->from, c->to);
                double raveVal = 0.0, raveWeight = 0.0;
                auto rit = g_raveTable.find(mKey);
                if (rit != g_raveTable.end() && rit->second.visits > 0) {
                    raveVal = rit->second.wins / rit->second.visits;
                    double beta = static_cast<double>(rit->second.visits)
                                / (rit->second.visits + c->visits + RAVE_EQUIV);
                    Q = (1.0 - beta) * Q + beta * raveVal;
                }

                double U = cPUCT * std::sqrt(logN / (1.0 + c->visits));
                double prior = heuristicPrior(c->from, c->to, centroid);
                double score = U + 0.3 * prior * std::sqrt(logN / (1.0 + c->visits));

                if (nodePid == aiPid) score += Q;
                else score -= Q;

                if (score > bestScore) { bestScore = score; best = c.get(); }
            }

            // 渐进式加宽
            if (static_cast<int>(node->children.size()) <
                static_cast<int>(std::sqrt(static_cast<double>(node->visits))) + 2) {
                break;
            }

            applyMove(board, best->from, best->to);
            uint64_t mk = packMove(best->from, best->to);
            path.push_back({best, mk});
            node = best;
        }

        // == ② Expansion (启发式排序展开) ==
        int nodePid = static_cast<int>(node->player);
        if (!node->untriedMoves.empty()) {
            HexCoord centro = targetCentroid(playerCount, nodePid);
            std::sort(node->untriedMoves.begin(), node->untriedMoves.end(),
                [&](const auto& a, const auto& b) {
                    int ga = hexDist(a.first, centro) - hexDist(a.second, centro)
                           + (isJump(a.first, a.second) ? 6 : 0);
                    int gb = hexDist(b.first, centro) - hexDist(b.second, centro)
                           + (isJump(b.first, b.second) ? 6 : 0);
                    return ga > gb;
                });

            auto from = node->untriedMoves.back().first;
            auto to   = node->untriedMoves.back().second;
            node->untriedMoves.pop_back();

            applyMove(board, from, to);
            int nextPid = (nodePid + 1) % playerCount;
            Player nextPlayer = static_cast<Player>(nextPid);

            auto child = std::make_unique<MCTSNode>(from, to, nextPlayer, node);
            {
                auto childMoves = getAllMoves(board, nextPlayer);
                for (auto& m : childMoves) child->untriedMoves.emplace_back(m);
            }
            node->children.push_back(std::move(child));
            node = node->children.back().get();
            uint64_t mk = packMove(from, to);
            path.push_back({node, mk});
        }

        // == ③ Simulation (Softmax Rollout) ==
        int simStartPid = static_cast<int>(node->player);
        Player winner = rolloutWinner(board, playerCount, simStartPid);

        double value;
        if (winner == Player::None) {
            value = heuristicEval(board, who, playerCount);
        } else {
            value = (winner == who) ? 1.0 : -1.0;
        }

        // == ④ Backpropagation (含RAVE更新) ==
        // 4a. 更新树内路径
        MCTSNode* back = node;
        while (back != nullptr) {
            back->visits++;
            back->wins += value;
            back = back->parent;
        }
        // 4b. 更新RAVE全局表
        for (auto& [pathNode, moveKey] : path) {
            auto& rs = g_raveTable[moveKey];
            rs.visits++;
            rs.wins += value;
        }
        // 4c. 更新置换表
        uint64_t boardHash = hashBoard(board, playerCount);
        auto& tte = tt[boardHash];
        tte.visits++;
        tte.wins += value;

        // ---- 时间检查（每20次模拟）----
        if (simCount % 20 == 0 && simCount >= MIN_SIMULATIONS) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            if (elapsed >= TIME_BUDGET_SEC) break;
        }
    }

    // ---- 选最佳走法（优先访问次数，次优胜率）----
    MCTSNode* best = nullptr;
    int bestVisits = -1;
    double bestWinRate = -1e9;

    for (auto& c : root->children) {
        if (c->visits > bestVisits) {
            bestVisits = c->visits;
            bestWinRate = (c->visits > 0) ? c->wins / c->visits : 0.0;
            best = c.get();
        } else if (c->visits == bestVisits) {
            double wr = (c->visits > 0) ? c->wins / c->visits : 0.0;
            if (wr > bestWinRate) { bestWinRate = wr; best = c.get(); }
        }
    }

    if (!best) return {};
    lastRoot = std::move(root);
    return { best->from, best->to, who, origBoard.at(best->to) };
}

void resetAI() {
    g_raveTable.clear();
    lastRoot.reset();
}

// ======================= 全局变量 =======================
std::unique_ptr<MCTSNode> lastRoot;

int& AI_LEVEL() {
    static int level = 2;
    return level;
}

} // namespace AI
