#include "AI_MCTS.h"
#include <vector>
#include <random>
#include <memory>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <thread>

namespace AI {

    static int MCTS_SIMULATION() {
        constexpr int table[] = { 0, 500, 2000, 8000 };
        return table[AI_LEVEL()];
    }

    static constexpr double C_PUCT = 1.414;

    static std::mt19937& rng() {
        static std::mt19937 mt(std::random_device{}());
        return mt;
    }

    // 已经在 .h 中定义，这里直接使用
    // struct Node { ... };

    static double rollout(const Board& bd, Player who) {
        const auto& st = bd.getBoardState();
        double sum = 0;
        int  cnt = 0;
        for (const auto& [h, p] : st) {
            if (static_cast<int>(p) != static_cast<int>(who)) continue;
            int dq = h.q - 4, dr = h.r + 8;
            sum += 1.0 / (1 + std::abs(dq) + std::abs(dr));
            ++cnt;
        }
        return cnt ? sum / cnt : 0.0;
    }

    static Node* select(Node* node) {
        while (!node->children.empty()) {
            Node* best = nullptr;
            double best_score = -1e9;
            int N = node->visit;
            for (auto& c : node->children) {
                double uct = c->q_sum / (1 + c->visit) +
                    C_PUCT * c->prior * std::sqrt(N) / (1 + c->visit);
                if (uct > best_score) { best_score = uct; best = c.get(); }
            }
            node = best;
        }
        return node;
    }

    static void expand(Node* leaf, const Board& bd) {
        std::vector<HexCoord> moves;
        const auto& st = bd.getBoardState();
        for (const auto& [h, p] : st) {
            if (static_cast<int>(p) != static_cast<int>(leaf->who)) continue;
            std::vector<HexCoord> legal;
            bd.findLegalMovesFor(h, legal);
            for (const auto& m : legal) moves.push_back(m);
        }
        std::sort(moves.begin(), moves.end());
        moves.erase(std::unique(moves.begin(), moves.end()), moves.end());

        Player nxt = static_cast<Player>((static_cast<int>(leaf->who) + 1) % 6);
        for (const auto& m : moves)
            leaf->children.push_back(std::make_unique<Node>(m, nxt, leaf));
    }

    static double simulate(Board bd, Player who) {
        return rollout(bd, who);
    }

    static void backprop(Node* node, double value) {
        while (node) {
            node->visit++;
            node->q_sum += value;
            node = node->parent;
        }
    }

    // 定义全局变量
    std::unique_ptr<Node> lastRoot;
    int& AI_LEVEL() {
        static int level = 2;
        return level;
    }

    MoveRecord getMove(const Board& bd, Player who) {
        auto root = std::make_unique<Node>(HexCoord(0, 0), who, nullptr);

        const int simulations = MCTS_SIMULATION();
        for (int i = 0; i < simulations; ++i) {
            Node* leaf = select(root.get());
            if (leaf->visit && leaf->children.empty()) expand(leaf, bd);
            if (!leaf->children.empty()) {
                leaf = leaf->children[rng()() % leaf->children.size()].get();
            }
            double value = simulate(bd, leaf->who);
            backprop(leaf, value);
        }

        Node* best = nullptr;
        int bestN = -1;
        for (auto& c : root->children)
            if (static_cast<int>(c->visit) > bestN) { bestN = c->visit; best = c.get(); }
        if (!best) return {};

        HexCoord to = best->move;
        HexCoord from{};
        const auto& st = bd.getBoardState();
        for (const auto& [h, p] : st) {
            if (static_cast<int>(p) != static_cast<int>(who)) continue;
            std::vector<HexCoord> legal;
            bd.findLegalMovesFor(h, legal);
            if (std::find(legal.begin(), legal.end(), to) != legal.end()) {
                from = h;
                break;
            }
        }
        lastRoot = std::move(root); // 保存根节点供 UI 显示
        return { from, to, who, st.at(to) };
    }

} // namespace AI