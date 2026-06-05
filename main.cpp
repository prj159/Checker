#include "Board.h"
#include "ChooseMode.h"
#include "selfplay.h"
#include "AI_MCTS.h"
#include <graphics.h>
#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[])
{
    // ========== 自对弈模式：checker.exe --selfplay 100 ==========
    if (argc >= 3 && std::string(argv[1]) == "--selfplay") {
        int numGames = std::atoi(argv[2]);
        int playerCount = 2;
        if (argc >= 4) playerCount = std::atoi(argv[3]);

        std::cout << "========== 跳棋自对弈引擎 ==========" << std::endl;
        std::cout << "总局数: " << numGames << ", 玩家数: " << playerCount << std::endl;
        std::cout << "搜索时间: 5秒/步" << std::endl;
        std::cout << "开始对弈..." << std::endl << std::endl;

        // 尝试加载已有开局库
        if (AI::loadOpeningBook("opening_book.dat")) {
            std::cout << "已加载开局库 opening_book.dat" << std::endl;
        }

        SelfPlay::BatchStats stats = SelfPlay::runBatch(numGames, playerCount);

        std::cout << "\n对弈完成。开局库已自动保存。" << std::endl;
        return 0;
    }

    // ========== 参数搜索模式 ==========
    if (argc >= 3 && std::string(argv[1]) == "--tune") {
        int gamesPerConfig = std::atoi(argv[2]);
        SelfPlay::paramGridSearch(gamesPerConfig);
        return 0;
    }

    // ========== 正常游戏模式 ==========
    int playerCount = ChooseMode();      // 2/4/6
    int aiMode = chooseAI();             // 0 人人  1 人机
    bool humanFirst = false;
    if (aiMode == 1) { humanFirst = (chooseFirst() == 1); }

    // 尝试加载开局库
    AI::loadOpeningBook("opening_book.dat");

    int W = 800, H = 900;
    initgraph(W, H);
    setbkcolor(RGB(40, 40, 40));
    Board game(W, H, playerCount, aiMode, humanFirst);
    game.run();
    closegraph();

    // 游戏结束后保存开局库
    AI::saveOpeningBook("opening_book.dat");

    return 0;
}
