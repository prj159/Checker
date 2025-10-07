#pragma once
#include "Board.h"
#include <vector>

namespace AI {
    // 随机选一个可走位置（能跳优先）
    MoveRecord getMove(const Board& bd, Player who);
}