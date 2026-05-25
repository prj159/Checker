#pragma once
#include "Board.h"
#include <string>
#include <vector>
#include <map>

namespace SelfPlay {

// 单局结果
struct GameResult {
    Player winner;
    int totalMoves;
    std::vector<MoveRecord> history;
};

// 批量对弈统计
struct BatchStats {
    int totalGames;
    int draws;       // 未分胜负
    std::vector<int> winsPerPlayer;  // 每位玩家的胜场
    double avgMoves;
};

// 运行一局无头对弈（所有玩家由AI控制）
// playerCount: 2/4/6
// 返回对局结果
GameResult runOneGame(int playerCount);

// 批量自对弈
// numGames: 总局数
// playerCount: 玩家数
// 打印进度并返回统计
BatchStats runBatch(int numGames, int playerCount);

// 保存自对弈棋谱到文件
void saveGameRecords(const std::string& filename,
                     const std::vector<GameResult>& results);

// 加载自对弈棋谱从文件
std::vector<GameResult> loadGameRecords(const std::string& filename);

// 参数网格搜索：用自对弈评估不同参数组合
void paramGridSearch(int gamesPerConfig);

} // namespace SelfPlay
