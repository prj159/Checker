/****************************************************************************
 *  AI_MCTS.cpp  --  完整 MCTS AI（v3：有偏 rollout + 时间预算 + 增强评估）
 *
 *  核心改进：
 *    1. 有偏好 rollout：70% 概率选最接近目标的走法，30% 随机探索
 *    2. 时间预算制：每次走棋最多搜索 2 秒，而非固定次数
 *    3. 增强启发式：考虑进度、跳跃链、阻塞、对手进度
 *    4. 跳跃优先：rollout 中跳跃走法获得加权
 ***************************************************************************/
#include "AI_MCTS.h"
#include <set>
#include <random>
#include <cmath>
#include <algorithm>
#include <chrono>

namespace AI {

// ======================= 时间预算 =======================
static constexpr double TIME_BUDGET_SEC = 2.0;   // 每次走棋最多 2 秒

// 保底：即使超时也要跑的最少模拟次数
static constexpr int MIN_SIMULATIONS = 100;

static constexpr double C_PUCT = 1.414;

// ======================= 随机数 =======================
static std::mt19937& rng() {
    static std::mt19937 mt(
        static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())
    );
    return mt;
}

static std::uniform_real_distribution<double> uni01(0.0, 1.0);

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

// 预计算目标区域中心（六角坐标平均）
static HexCoord targetCentroid(int playerCount, int pid) {
    static HexCoord cache[3][6];
    static bool cached = false;
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

// ======================= 走法生成 =======================
static void findJumpsRecursive(const std::map<HexCoord, Player>& board,
                                const HexCoord& from,
                                std::vector<HexCoord>& moves,
                                std::map<HexCoord, bool>& visited) {
    visited[from] = true;
    const int dirs[6][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,-1},{-1,1}};
    for (auto& d : dirs) {
        HexCoord mid(from.q + d[0], from.r + d[1]);
        HexCoord dst(from.q + 2*d[0], from.r + 2*d[1]);
        auto it_mid = board.find(mid);
        auto it_dst = board.find(dst);
        if (it_mid != board.end() && it_mid->second != Player::None &&
            it_dst != board.end() && it_dst->second == Player::None &&
            !visited[dst]) {
            moves.push_back(dst);
            findJumpsRecursive(board, dst, moves, visited);
        }
    }
}

static std::vector<HexCoord> getMovesForPiece(
    const std::map<HexCoord, Player>& board, const HexCoord& from) {
    std::vector<HexCoord> out;
    const int dirs[6][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,-1},{-1,1}};
    for (auto& d : dirs) {
        HexCoord n(from.q + d[0], from.r + d[1]);
        auto it = board.find(n);
        if (it != board.end() && it->second == Player::None)
            out.push_back(n);
    }
    std::map<HexCoord, bool> vis;
    findJumpsRecursive(board, from, out, vis);
    return out;
}

// 判断一次走法是否为跳跃（from 到 to 的距离 > 1）
static inline bool isJump(const HexCoord& from, const HexCoord& to) {
    return hexDist(from, to) > 1;
}

static std::vector<std::pair<HexCoord, HexCoord>> getAllMoves(
    const std::map<HexCoord, Player>& board, Player who) {
    std::vector<std::pair<HexCoord, HexCoord>> result;
    for (const auto& kv : board) {
        if (kv.second != who) continue;
        auto moves = getMovesForPiece(board, kv.first);
        for (const auto& to : moves)
            result.emplace_back(kv.first, to);
    }
    return result;
}

// ======================= 走子 =======================
static inline void applyMove(std::map<HexCoord, Player>& board,
                              HexCoord from, HexCoord to) {
    board[to]   = board[from];
    board[from] = Player::None;
}

// ======================= 胜负判定 =======================
static bool isGameOver(const std::map<HexCoord, Player>& board, int playerCount) {
    const auto& zones = getTargetZones(playerCount);
    for (int pl = 0; pl < playerCount; ++pl) {
        bool allInTarget = true;
        for (const auto& kv : board) {
            if (static_cast<int>(kv.second) != pl) continue;
            bool inTarget = false;
            for (const auto& t : zones[pl])
                if (t == kv.first) { inTarget = true; break; }
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
            for (const auto& t : zones[pl])
                if (t == kv.first) { inTarget = true; break; }
            if (!inTarget) { allInTarget = false; break; }
        }
        if (allInTarget) return static_cast<Player>(pl);
    }
    return Player::None;
}

// ======================= 有偏 Rollout（核心改进） =======================
// ε-greedy: 70% 概率选最优前进走法，30% 随机探索
// 跳跃走法在比较中自动获胜（因为 hexDist 差异 > 1）
static constexpr double GREEDY_PROB = 0.70;

static void selectRolloutMove(
    const std::vector<std::pair<HexCoord, HexCoord>>& moves,
    const HexCoord& targetCenter,
    std::pair<HexCoord, HexCoord>& chosen) {
    if (moves.empty()) return;

    // ε-greedy: 随机探索
    if (uni01(rng()) > GREEDY_PROB) {
        chosen = moves[rng()() % moves.size()];
        return;
    }

    // 贪婪：选距离目标中心最近的走法
    int bestIdx = 0;
    int bestDist = hexDist(moves[0].second, targetCenter);
    int bestJumpBonus = isJump(moves[0].first, moves[0].second) ? 0 : 1; // 0=跳跃优先
    for (size_t i = 1; i < moves.size(); ++i) {
        int d = hexDist(moves[i].second, targetCenter);
        int jb = isJump(moves[i].first, moves[i].second) ? 0 : 1;
        // 优先跳跃；若同为跳跃/单步，选距离近的
        if (jb < bestJumpBonus || (jb == bestJumpBonus && d < bestDist)) {
            bestDist = d;
            bestJumpBonus = jb;
            bestIdx = static_cast<int>(i);
        }
    }
    chosen = moves[bestIdx];
}

static Player rolloutWinner(const std::map<HexCoord, Player>& boardState,
                             int playerCount, int startPlayerIdx,
                             int maxSteps = 80) {
    auto board = boardState;
    int cur = startPlayerIdx;

    for (int step = 0; step < maxSteps; ++step) {
        if (isGameOver(board, playerCount))
            return getWinner(board, playerCount);

        Player who = static_cast<Player>(cur);
        auto moves = getAllMoves(board, who);
        if (moves.empty()) {
            cur = (cur + 1) % playerCount;
            continue;
        }

        // 有偏选择！
        HexCoord tc = targetCentroid(playerCount, cur);
        std::pair<HexCoord, HexCoord> pick;
        selectRolloutMove(moves, tc, pick);
        applyMove(board, pick.first, pick.second);
        cur = (cur + 1) % playerCount;
    }
    return Player::None;
}

// ======================= 增强启发式评估 =======================
// 综合考虑：己方进度、敌方进度、跳跃潜力、棋子阻塞
static double heuristicEval(const std::map<HexCoord, Player>& board,
                             Player aiPlayer, int playerCount) {
    const int aiPid = static_cast<int>(aiPlayer);

    auto evalPlayer = [&](int pid) -> double {
        const HexCoord tc = targetCentroid(playerCount, pid);
        int total = 0;
        double sumDist = 0.0;
        int blocked = 0;

        for (const auto& kv : board) {
            if (static_cast<int>(kv.second) != pid) continue;
            ++total;
            int d = hexDist(kv.first, tc);
            sumDist += d;

            // 检查是否被阻塞（六个方向都被占据）
            const int dirs[6][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,-1},{-1,1}};
            int free = 0;
            for (auto& dd : dirs) {
                HexCoord n(kv.first.q + dd[0], kv.first.r + dd[1]);
                auto it = board.find(n);
                if (it != board.end() && it->second == Player::None) ++free;
            }
            if (free == 0) ++blocked;
        }
        if (total == 0) return 1.0;  // 已在目标（获胜条件）

        double avgDist = sumDist / total;
        double progress = 1.0 - (avgDist / 16.0);  // 归一化
        if (progress < 0.0) progress = 0.0;
        if (progress > 1.0) progress = 1.0;

        // 阻塞惩罚
        double blockPenalty = static_cast<double>(blocked) / total * 0.3;
        return progress - blockPenalty;
    };

    double aiScore = evalPlayer(aiPid);

    // 综合敌方进度（取最威胁者）
    double worstOpp = 1.0;
    for (int p = 0; p < playerCount; ++p) {
        if (p == aiPid) continue;
        double oppScore = evalPlayer(p);
        if (oppScore < worstOpp) worstOpp = oppScore;
    }

    // 差值映射到 [-1, 1]
    double diff = aiScore - worstOpp;
    if (diff > 1.0) diff = 1.0;
    if (diff < -1.0) diff = -1.0;
    return diff;
}

// ======================= MCTS 主搜索 =======================
MoveRecord getMove(const Board& bd, Player who) {
    const auto& origBoard = bd.getBoardState();
    const int playerCount = bd.getPlayerCount();
    const int aiPid       = static_cast<int>(who);

    // 构造根节点
    auto root = std::make_unique<MCTSNode>(
        HexCoord(0,0), HexCoord(0,0), who, nullptr);

    {
        auto moves = getAllMoves(origBoard, who);
        for (auto& m : moves)
            root->untriedMoves.push_back(m);
    }

    if (root->untriedMoves.empty())
        return {};

    // ---- 时间预算循环 ----
    auto startTime = std::chrono::steady_clock::now();
    int simCount = 0;

    while (true) {
        ++simCount;

        // == ① Selection ==
        MCTSNode* node = root.get();
        auto board = origBoard;

        while (node->untriedMoves.empty() && !node->children.empty()) {
            MCTSNode* best = nullptr;
            double bestScore = -1e9;

            for (auto& c : node->children) {
                double Q = (c->visits > 0)
                         ? c->wins / static_cast<double>(c->visits)
                         : 0.0;
                double U = C_PUCT
                         * std::sqrt(std::log(static_cast<double>(node->visits + 1))
                                     / (1.0 + c->visits));

                double score = U;
                if (static_cast<int>(node->player) == aiPid)
                    score += Q;
                else
                    score -= Q;

                if (score > bestScore) { bestScore = score; best = c.get(); }
            }

            applyMove(board, best->from, best->to);
            node = best;
        }

        // == ② Expansion ==
        int nodePid = static_cast<int>(node->player);
        if (!node->untriedMoves.empty()) {
            size_t idx = rng()() % node->untriedMoves.size();
            auto from = node->untriedMoves[idx].first;
            auto to = node->untriedMoves[idx].second;
            std::swap(node->untriedMoves[idx], node->untriedMoves.back());
            node->untriedMoves.pop_back();

            applyMove(board, from, to);
            int nextPid = (nodePid + 1) % playerCount;
            Player nextPlayer = static_cast<Player>(nextPid);

            auto child = std::make_unique<MCTSNode>(from, to, nextPlayer, node);
            {
                auto childMoves = getAllMoves(board, nextPlayer);
                for (auto& m : childMoves)
                    child->untriedMoves.emplace_back(m);
            }
            node->children.push_back(std::move(child));
            node = node->children.back().get();
        }

        // == ③ Simulation (有偏 Rollout) ==
        int simStartPid = static_cast<int>(node->player);
        Player winner = rolloutWinner(board, playerCount, simStartPid);

        double value;
        if (winner == Player::None) {
            value = heuristicEval(board, who, playerCount);
        } else {
            value = (winner == who) ? 1.0 : -1.0;
        }

        // == ④ Backpropagation ==
        while (node != nullptr) {
            node->visits++;
            node->wins += value;
            node = node->parent;
        }

        // ---- 时间检查（每 50 次模拟检查一次以降低开销） ----
        if (simCount % 50 == 0 && simCount >= MIN_SIMULATIONS) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            if (elapsed >= TIME_BUDGET_SEC) break;
        }
    }

    // ---- 选最佳走法 ----
    MCTSNode* best = nullptr;
    int    bestVisits = -1;
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

// ======================= 全局变量 =======================
std::unique_ptr<MCTSNode> lastRoot;

int& AI_LEVEL() {
    static int level = 2;
    return level;
}

} // namespace AI
