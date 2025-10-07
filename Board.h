#pragma once
#include <graphics.h>   // EasyX
#include <conio.h>
#include <string>
#include <map>
#include <vector>
#include <cmath>
#include <algorithm>

const double PI = 3.14159265358979323846;
const int  HEX_RADIUS = 30;

struct HexCoord {
    int q, r;
    HexCoord(int q_ = 0, int r_ = 0) :q(q_), r(r_) {}
    bool operator==(const HexCoord& o) const { return q == o.q && r == o.r; }
    bool operator<(const HexCoord& o)  const { return q < o.q || (q == o.q && r < o.r); }
};

enum class Player { None = -1, Red = 0, Green = 1, Blue = 2, Yellow = 3, Magenta = 4, Cyan = 5 };

struct MoveRecord {
    HexCoord from;
    HexCoord to;
    Player   fromPlayer;
    Player   toPlayerBefore;
};

class Board {
private:
    int windowWidth, windowHeight;
    int originX, originY;
    int currentPlayer;                  // 0..playerCount-1
    int playerCount;
    bool hasSelection;
    HexCoord selectedHex;
    std::map<HexCoord, Player> boardState;
    std::vector<HexCoord> validMoves;
    std::vector<MoveRecord> moveHistory;

    std::vector<COLORREF> playerColors;
    std::vector<std::wstring> playerNames;

    void initializeBoard();
    void drawHexagonOutline(const HexCoord& hex);
    POINT hexToPixel(const HexCoord& hex) const;
    HexCoord pixelToHex(const POINT& p) const;
    void findValidMoves(const HexCoord& from);
    void findJumps(const HexCoord& from, std::vector<HexCoord>& moves,
        std::map<HexCoord, bool>& visited);
    void checkVictory();
    void drawUI();
    COLORREF getPlayerColor(Player p);
    std::wstring getPlayerName(Player p);

public:
    Board(int w, int h, int _playerCount);
    void run();
    void handleMouseClick(const MOUSEMSG& msg);
    void switchPlayer();
    void undoMove();
    void display();
};