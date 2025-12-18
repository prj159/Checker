/****************************************************************************
 *  AI.cpp  –  随机合法走法生成器（可直接编译）
 ***************************************************************************/
#include "AI.h"
#include "Board.h"
#include <vector>
#include <cstdlib>
#include <ctime>

namespace AI {

    /* 把“一步”抽象成内部结构，方便随机挑选 */
    struct Step {
        HexCoord from;
        HexCoord to;
    };

    MoveRecord getMove(const Board& bd, Player who)
    {
        /* 1. 收集当前玩家的所有合法走法 *************************/
        std::vector<Step> steps;

        for (const auto& kv : bd.getBoardState())          // kv:  HexCoord -> Player
        {
            if (kv.second != who) continue;                // 只关心自己的棋子

            std::vector<HexCoord> legal;
            bd.findLegalMovesFor(kv.first, legal);         // Board 提供的接口
            for (const HexCoord& dst : legal)
                steps.push_back({ kv.first, dst });
        }

        /* 2. 无子可动，返回空记录 *******************************/
        if (steps.empty())
            return {};   // MoveRecord 默认构造函数会把 from/to 设为 (0,0)

        /* 3. 随机挑一步 *****************************************/
        std::srand(static_cast<unsigned>(std::time(nullptr)));   // 每次重新播种
        const Step& pick = steps[std::rand() % steps.size()];

        /* 4. 组装成 MoveRecord 返回 *****************************/
        return {
            pick.from,
            pick.to,
            who,
            bd.getBoardState().at(pick.to)   // 目标格子原来的玩家（None 或敌方）
        };
    }

} // namespace AI