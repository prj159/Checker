/****************************************************************************
 *  selfplay.cpp  --  无头自对弈引擎 + 开局库构建 + 参数调优
 ***************************************************************************/
#include "selfplay.h"
#include "AI_MCTS.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <cmath>

namespace SelfPlay {

// ======================= 辅助：创建无GUI棋盘 =======================
static std::map<HexCoord, Player> createBoardState(int playerCount) {
    std::map<HexCoord, Player> board;

    // 生成所有空格
    for (int q = -4; q <= 4; ++q)
        for (int r = -4; r <= 4; ++r) {
            int s = -q - r;
            if (std::abs(s) <= 4) board[HexCoord(q, r)] = Player::None;
        }
    for (int q = 5; q <= 8; ++q)
        for (int r = -q + 4; r >= -4; --r) board[HexCoord(r, q)] = Player::None;
    for (int q = -5; q >= -8; --q)
        for (int r = -q - 4; r <= 4; ++r) board[HexCoord(r, q)] = Player::None;
    for (int q = 1; q <= 4; ++q)
        for (int r = 5 - q; r <= 4; ++r) board[HexCoord(r, q)] = Player::None;
    for (int q = -1; q >= -4; --q)
        for (int r = -5 - q; r >= -4; --r) board[HexCoord(r, q)] = Player::None;
    for (int q = 1; q <= 4; ++q)
        for (int r = -4 - q; r <= -5; ++r) board[HexCoord(r, q)] = Player::None;
    for (int q = -1; q >= -4; --q)
        for (int r = 4 - q; r >= 5; --r) board[HexCoord(r, q)] = Player::None;

    // 摆放棋子（与Board.cpp一致）
    const std::vector<HexCoord> bases[3] = {
        /* 2人 */
        {HexCoord(-4,8),HexCoord(-3,7),HexCoord(-4,7),HexCoord(-2,6),HexCoord(-3,6),HexCoord(-4,6),
         HexCoord(-1,5),HexCoord(-2,5),HexCoord(-3,5),HexCoord(-4,5),
         HexCoord(4,-8),HexCoord(3,-7),HexCoord(4,-7),HexCoord(2,-6),HexCoord(3,-6),HexCoord(4,-6),
         HexCoord(1,-5),HexCoord(2,-5),HexCoord(3,-5),HexCoord(4,-5)},
        /* 4人 */
        {HexCoord(-4,8),HexCoord(-3,7),HexCoord(-4,7),HexCoord(-2,6),HexCoord(-3,6),HexCoord(-4,6),
         HexCoord(-1,5),HexCoord(-2,5),HexCoord(-3,5),HexCoord(-4,5),
         HexCoord(4,-8),HexCoord(3,-7),HexCoord(4,-7),HexCoord(2,-6),HexCoord(3,-6),HexCoord(4,-6),
         HexCoord(1,-5),HexCoord(2,-5),HexCoord(3,-5),HexCoord(4,-5),
         HexCoord(4,1),HexCoord(3,2),HexCoord(4,2),HexCoord(2,3),HexCoord(3,3),HexCoord(4,3),
         HexCoord(1,4),HexCoord(2,4),HexCoord(3,4),HexCoord(4,4),
         HexCoord(-4,-1),HexCoord(-3,-2),HexCoord(-4,-2),HexCoord(-2,-3),HexCoord(-3,-3),HexCoord(-4,-3),
         HexCoord(-1,-4),HexCoord(-2,-4),HexCoord(-3,-4),HexCoord(-4,-4)},
        /* 6人 */
        {HexCoord(-4,8),HexCoord(-3,7),HexCoord(-4,7),HexCoord(-2,6),HexCoord(-3,6),HexCoord(-4,6),
         HexCoord(-1,5),HexCoord(-2,5),HexCoord(-3,5),HexCoord(-4,5),
         HexCoord(4,-8),HexCoord(3,-7),HexCoord(4,-7),HexCoord(2,-6),HexCoord(3,-6),HexCoord(4,-6),
         HexCoord(1,-5),HexCoord(2,-5),HexCoord(3,-5),HexCoord(4,-5),
         HexCoord(4,1),HexCoord(3,2),HexCoord(4,2),HexCoord(2,3),HexCoord(3,3),HexCoord(4,3),
         HexCoord(1,4),HexCoord(2,4),HexCoord(3,4),HexCoord(4,4),
         HexCoord(-4,-1),HexCoord(-3,-2),HexCoord(-4,-2),HexCoord(-2,-3),HexCoord(-3,-3),HexCoord(-4,-3),
         HexCoord(-1,-4),HexCoord(-2,-4),HexCoord(-3,-4),HexCoord(-4,-4),
         HexCoord(5,-1),HexCoord(6,-2),HexCoord(5,-2),HexCoord(7,-3),HexCoord(6,-3),HexCoord(5,-3),
         HexCoord(8,-4),HexCoord(7,-4),HexCoord(6,-4),HexCoord(5,-4),
         HexCoord(-5,1),HexCoord(-6,2),HexCoord(-5,2),HexCoord(-7,3),HexCoord(-6,3),HexCoord(-5,3),
         HexCoord(-8,4),HexCoord(-7,4),HexCoord(-6,4),HexCoord(-5,4)}
    };
    int idx = (playerCount == 2 ? 0 : playerCount == 4 ? 1 : 2);
    for (size_t i = 0; i < bases[idx].size(); ++i)
        board[bases[idx][i]] = static_cast<Player>(i / (bases[idx].size() / playerCount));

    return board;
}

// ======================= 胜利检测（与 Board::checkVictory 一致）=======================
static const std::vector<std::vector<HexCoord>>& getVictoryZones(int playerCount) {
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

static Player checkWinner(const std::map<HexCoord, Player>& board, int playerCount) {
    const auto& zones = getVictoryZones(playerCount);
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

// ======================= 单局对弈 ========================
GameResult runOneGame(int playerCount) {
    GameResult result;
    result.winner = Player::None;
    result.totalMoves = 0;

    auto board = createBoardState(playerCount);
    int currentPlayer = 0;
    const int MAX_MOVES = 500;

    AI::resetAI();

    for (int moveNum = 0; moveNum < MAX_MOVES; ++moveNum) {
        Player who = static_cast<Player>(currentPlayer);
        MoveRecord mv = AI::getMoveHeadless(board, playerCount, who);

        if (mv.from == mv.to) {
            // 无合法走法，跳过该玩家
            currentPlayer = (currentPlayer + 1) % playerCount;
            continue;
        }

        // 执行走法
        board[mv.to] = board[mv.from];
        board[mv.from] = Player::None;

        // 记录（修正类型）
        MoveRecord rec;
        rec.from = mv.from;
        rec.to = mv.to;
        rec.fromPlayer = mv.fromPlayer;
        rec.toPlayerBefore = mv.toPlayerBefore;
        result.history.push_back(rec);

        // 检查胜利
        Player winner = checkWinner(board, playerCount);
        if (winner != Player::None) {
            result.winner = winner;
            result.totalMoves = moveNum + 1;
            return result;
        }

        currentPlayer = (currentPlayer + 1) % playerCount;
    }

    result.totalMoves = MAX_MOVES;
    return result;  // 超时未分胜负
}

// ======================= 批量对弈 ========================
BatchStats runBatch(int numGames, int playerCount) {
    BatchStats stats;
    stats.totalGames = numGames;
    stats.draws = 0;
    stats.winsPerPlayer.resize(playerCount, 0);
    double totalMoves = 0.0;

    std::vector<GameResult> allResults;
    allResults.reserve(numGames);

    auto batchStart = std::chrono::steady_clock::now();

    for (int g = 0; g < numGames; ++g) {
        auto gameStart = std::chrono::steady_clock::now();
        GameResult res = runOneGame(playerCount);
        auto gameEnd = std::chrono::steady_clock::now();
        double gameSec = std::chrono::duration<double>(gameEnd - gameStart).count();

        allResults.push_back(res);
        totalMoves += res.totalMoves;

        if (res.winner == Player::None) {
            stats.draws++;
        } else {
            stats.winsPerPlayer[static_cast<int>(res.winner)]++;
        }

        // 进度打印
        if ((g + 1) % 10 == 0 || g == numGames - 1) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - batchStart).count();
            std::cout << "[" << (g + 1) << "/" << numGames << "] "
                      << "胜者分布: ";
            for (int p = 0; p < playerCount; ++p)
                std::cout << "P" << p << "=" << stats.winsPerPlayer[p] << " ";
            std::cout << "平=" << stats.draws
                      << " | 耗时 " << std::fixed << std::setprecision(1)
                      << elapsed << "s (本局 " << gameSec << "s)"
                      << std::endl;
        }

        // 将本局走法录入开局库
        AI::recordGameToOpeningBook(res.history, res.winner, playerCount);
    }

    stats.avgMoves = totalMoves / numGames;

    auto batchEnd = std::chrono::steady_clock::now();
    double totalSec = std::chrono::duration<double>(batchEnd - batchStart).count();
    std::cout << "\n========== 批量对弈完成 ==========" << std::endl;
    std::cout << "总局数: " << numGames << std::endl;
    std::cout << "总耗时: " << std::fixed << std::setprecision(1) << totalSec << "s" << std::endl;
    std::cout << "平均步数: " << std::fixed << std::setprecision(1) << stats.avgMoves << std::endl;
    std::cout << "胜率分布: ";
    for (int p = 0; p < playerCount; ++p)
        std::cout << "P" << p << "=" << (100.0 * stats.winsPerPlayer[p] / numGames) << "% ";
    std::cout << "平=" << (100.0 * stats.draws / numGames) << "%" << std::endl;

    // 保存开局库
    AI::saveOpeningBook("opening_book.dat");
    std::cout << "开局库已保存至 opening_book.dat" << std::endl;

    return stats;
}

// ======================= 棋谱保存 ========================
void saveGameRecords(const std::string& filename,
                     const std::vector<GameResult>& results) {
    std::ofstream out(filename);
    if (!out) { std::cerr << "无法写入文件: " << filename << std::endl; return; }

    out << results.size() << std::endl;
    for (const auto& res : results) {
        out << static_cast<int>(res.winner) << " " << res.totalMoves << std::endl;
        for (const auto& mv : res.history) {
            out << mv.from.q << " " << mv.from.r << " "
                << mv.to.q << " " << mv.to.r << " "
                << static_cast<int>(mv.fromPlayer) << std::endl;
        }
        out << "END" << std::endl;
    }
    std::cout << "棋谱已保存至 " << filename << std::endl;
}

std::vector<GameResult> loadGameRecords(const std::string& filename) {
    std::vector<GameResult> results;
    std::ifstream in(filename);
    if (!in) { std::cerr << "无法读取文件: " << filename << std::endl; return results; }

    int count; in >> count;
    for (int i = 0; i < count; ++i) {
        GameResult res;
        int winnerInt;
        in >> winnerInt >> res.totalMoves;
        res.winner = static_cast<Player>(winnerInt);

        while (true) {
            int fq, fr, tq, tr, fp;
            in >> fq;
            if (in.fail()) break;
            in >> fr >> tq >> tr >> fp;

            MoveRecord mv;
            mv.from = HexCoord(fq, fr);
            mv.to = HexCoord(tq, tr);
            mv.fromPlayer = static_cast<Player>(fp);
            mv.toPlayerBefore = Player::None;
            res.history.push_back(mv);

            // 检查是否是 END 标记
            if (in.peek() == 'E') {
                std::string end; in >> end;
                break;
            }
        }
        results.push_back(res);
    }
    return results;
}

// ======================= 参数网格搜索 ========================
void paramGridSearch(int gamesPerConfig) {
    std::cout << "========== 参数网格搜索 ==========" << std::endl;
    std::cout << "每种配置测试 " << gamesPerConfig << " 局" << std::endl;

    // 测试不同参数组合（这里只演示概念，实际需暴露参数接口）
    // 当前版本：运行基线测试
    std::cout << "\n--- 基线配置 (当前默认参数) ---" << std::endl;
    BatchStats baseline = runBatch(gamesPerConfig, 2);

    std::cout << "\n参数搜索完成。" << std::endl;
}

} // namespace SelfPlay
