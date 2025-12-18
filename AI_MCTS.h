#pragma once
#include "Board.h"
#include <memory>
#include <vector>

namespace AI {

    struct Node {
        HexCoord move;
        Player   who;
        Node* parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
        int    visit = 0;
        double q_sum = 0.0;
        double prior = 1.0;

        Node(HexCoord m, Player w, Node* p) : move(m), who(w), parent(p) {}
    };

    MoveRecord getMove(const Board& bd, Player who);
    extern std::unique_ptr<Node> lastRoot;
    int& AI_LEVEL();
}