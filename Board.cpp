#include "Board.h"
#include <cmath>
#include <vector>
#include <map>
#include <windows.h>
#include <easyx.h>
#include <conio.h>
#include <string>

Board::Board(int w, int h, int _playerCount)
    : windowWidth(w), windowHeight(h), playerCount(_playerCount),
    currentPlayer(0), hasSelection(false), selectedHex(0, 0)
{
    originX = w / 2; originY = h / 2;

    const COLORREF allColors[6] = { RED, GREEN, BLUE, YELLOW, MAGENTA, CYAN };
    const wchar_t* allNames[6] = { L"Red",L"Green",L"Blue",L"Yellow",L"Magenta",L"Cyan" };
    for (int i = 0;i < playerCount;++i) {
        playerColors.push_back(allColors[i]);
        playerNames.push_back(allNames[i]);
    }
    initializeBoard();
}

void Board::initializeBoard()
{
    boardState.clear();
    /* 1. 把 121 个 hex 全置空 -------------------- */
    for (int q = -4;q <= 4;++q)for (int r = -4;r <= 4;++r) {
        int s = -q - r; if (std::abs(s) <= 4) boardState[HexCoord(q, r)] = Player::None;
    }
    /* 上下左右四个三角补齐 */
    for (int q = 5;q <= 8;++q)for (int r = -q + 4;r >= -4;--r) boardState[HexCoord(r, q)] = Player::None;
    for (int q = -5;q >= -8;--q)for (int r = -q - 4;r <= 4;++r) boardState[HexCoord(r, q)] = Player::None;
    for (int q = 1;q <= 4;++q)for (int r = 5 - q;r <= 4;++r)   boardState[HexCoord(r, q)] = Player::None;
    for (int q = -1;q >= -4;--q)for (int r = -5 - q;r >= -4;--r)boardState[HexCoord(r, q)] = Player::None;
    for (int q = 1;q <= 4;++q)for (int r = -4 - q;r <= -5;++r) boardState[HexCoord(r, q)] = Player::None;
    for (int q = -1;q >= -4;--q)for (int r = 4 - q;r >= 5;--r) boardState[HexCoord(r, q)] = Player::None;

    /* 2. 按人数放棋子 */
    const std::vector<HexCoord> bases[3] = {
        /* 2 人 */
        {HexCoord(-4, 8),
         HexCoord(-3, 7), HexCoord(-4, 7),
         HexCoord(-2, 6), HexCoord(-3, 6), HexCoord(-4, 6),
         HexCoord(-1, 5), HexCoord(-2, 5), HexCoord(-3, 5), HexCoord(-4, 5),
         HexCoord(4, -8),
        HexCoord(3, -7), HexCoord(4, -7),
        HexCoord(2, -6), HexCoord(3, -6), HexCoord(4, -6),
        HexCoord(1, -5), HexCoord(2, -5), HexCoord(3, -5), HexCoord(4, -5)},
         /* 4 人 左上+右下 */
         {HexCoord(-4, 8),
         HexCoord(-3, 7), HexCoord(-4, 7),
         HexCoord(-2, 6), HexCoord(-3, 6), HexCoord(-4, 6),
         HexCoord(-1, 5), HexCoord(-2, 5), HexCoord(-3, 5), HexCoord(-4, 5),

         HexCoord(4, -8),
        HexCoord(3, -7), HexCoord(4, -7),
        HexCoord(2, -6), HexCoord(3, -6), HexCoord(4, -6),
        HexCoord(1, -5), HexCoord(2, -5), HexCoord(3, -5), HexCoord(4, -5),

        HexCoord(4, 1),
         HexCoord(3,2), HexCoord(4,2),
         HexCoord(2,3), HexCoord(3,3), HexCoord(4,3),
         HexCoord(1,4), HexCoord(2,4), HexCoord(3,4), HexCoord(4,4),

         HexCoord(-4,-1),
        HexCoord(-3,-2), HexCoord(-4,-2),
        HexCoord(-2,-3), HexCoord(-3,-3), HexCoord(-4,-3),
        HexCoord(-1,-4), HexCoord(-2,-4), HexCoord(-3,-4), HexCoord(-4,-4)},
          /* 6 人 六芒星 */
          {HexCoord(-4, 8),
         HexCoord(-3, 7), HexCoord(-4, 7),
         HexCoord(-2, 6), HexCoord(-3, 6), HexCoord(-4, 6),
         HexCoord(-1, 5), HexCoord(-2, 5), HexCoord(-3, 5), HexCoord(-4, 5),

         HexCoord(4, -8),
        HexCoord(3, -7), HexCoord(4, -7),
        HexCoord(2, -6), HexCoord(3, -6), HexCoord(4, -6),
        HexCoord(1, -5), HexCoord(2, -5), HexCoord(3, -5), HexCoord(4, -5),

        HexCoord(4, 1),
         HexCoord(3,2), HexCoord(4,2),
         HexCoord(2,3), HexCoord(3,3), HexCoord(4,3),
         HexCoord(1,4), HexCoord(2,4), HexCoord(3,4), HexCoord(4,4),

         HexCoord(-4,-1),
        HexCoord(-3,-2), HexCoord(-4,-2),
        HexCoord(-2,-3), HexCoord(-3,-3), HexCoord(-4,-3),
        HexCoord(-1,-4), HexCoord(-2,-4), HexCoord(-3,-4), HexCoord(-4,-4),
    
    HexCoord(-5,1),
        HexCoord(-6,2), HexCoord(-5,2),
        HexCoord(-7,3), HexCoord(-6,3), HexCoord(-5,3),
        HexCoord(-8,4), HexCoord(-7,4), HexCoord(-6,4), HexCoord(-5,4),
    
    HexCoord(5,-1),
        HexCoord(6,-2), HexCoord(5,-2),
        HexCoord(7,-3), HexCoord(6,-3), HexCoord(5,-3),
        HexCoord(8,-4), HexCoord(7,-4), HexCoord(6,-4), HexCoord(5,-4)}
    };
    int idx = (playerCount == 2 ? 0 : playerCount == 4 ? 1 : 2);
    for (size_t i = 0;i < bases[idx].size();++i)
        boardState[bases[idx][i]] = static_cast<Player>(i / (bases[idx].size() / playerCount));
}

void Board::checkVictory()
{
    const std::vector<HexCoord> targets[3] = {
        /* 2 人：红要占绿角，绿要占红角 */
        { HexCoord(4, -8),
        HexCoord(3, -7), HexCoord(4, -7),
        HexCoord(2, -6), HexCoord(3, -6), HexCoord(4, -6),
        HexCoord(1, -5), HexCoord(2, -5), HexCoord(3, -5), HexCoord(4, -5),

         HexCoord(-4, 8),
         HexCoord(-3, 7), HexCoord(-4, 7),
         HexCoord(-2, 6), HexCoord(-3, 6), HexCoord(-4, 6),
         HexCoord(-1, 5), HexCoord(-2, 5), HexCoord(-3, 5), HexCoord(-4, 5)},

         /* 4 人 */
         {HexCoord(4, -8),
        HexCoord(3, -7), HexCoord(4, -7),
        HexCoord(2, -6), HexCoord(3, -6), HexCoord(4, -6),
        HexCoord(1, -5), HexCoord(2, -5), HexCoord(3, -5), HexCoord(4, -5),

         HexCoord(-4, 8),
         HexCoord(-3, 7), HexCoord(-4, 7),
         HexCoord(-2, 6), HexCoord(-3, 6), HexCoord(-4, 6),
         HexCoord(-1, 5), HexCoord(-2, 5), HexCoord(-3, 5), HexCoord(-4, 5),

          HexCoord(-4,-1),
        HexCoord(-3,-2), HexCoord(-4,-2),
        HexCoord(-2,-3), HexCoord(-3,-3), HexCoord(-4,-3),
        HexCoord(-1,-4), HexCoord(-2,-4), HexCoord(-3,-4), HexCoord(-4,-4),

        HexCoord(4, 1),
         HexCoord(3,2), HexCoord(4,2),
         HexCoord(2,3), HexCoord(3,3), HexCoord(4,3),
         HexCoord(1,4), HexCoord(2,4), HexCoord(3,4), HexCoord(4,4)},

          /* 6 人 */
          {HexCoord(4, -8),
        HexCoord(3, -7), HexCoord(4, -7),
        HexCoord(2, -6), HexCoord(3, -6), HexCoord(4, -6),
        HexCoord(1, -5), HexCoord(2, -5), HexCoord(3, -5), HexCoord(4, -5),

         HexCoord(-4, 8),
         HexCoord(-3, 7), HexCoord(-4, 7),
         HexCoord(-2, 6), HexCoord(-3, 6), HexCoord(-4, 6),
         HexCoord(-1, 5), HexCoord(-2, 5), HexCoord(-3, 5), HexCoord(-4, 5),

         HexCoord(-4,-1),
         HexCoord(-3,-2), HexCoord(-4,-2),
         HexCoord(-2,-3), HexCoord(-3,-3), HexCoord(-4,-3),
         HexCoord(-1,-4), HexCoord(-2,-4), HexCoord(-3,-4), HexCoord(-4,-4),

         HexCoord(4, 1),
         HexCoord(3,2), HexCoord(4,2),
         HexCoord(2,3), HexCoord(3,3), HexCoord(4,3),
         HexCoord(1,4), HexCoord(2,4), HexCoord(3,4), HexCoord(4,4),

        HexCoord(5,-1),
        HexCoord(6,-2), HexCoord(5,-2),
        HexCoord(7,-3), HexCoord(6,-3), HexCoord(5,-3),
        HexCoord(8,-4), HexCoord(7,-4), HexCoord(6,-4), HexCoord(5,-4),

        HexCoord(-5,1),
        HexCoord(-6,2), HexCoord(-5,2),
        HexCoord(-7,3), HexCoord(-6,3), HexCoord(-5,3),
        HexCoord(-8,4), HexCoord(-7,4), HexCoord(-6,4), HexCoord(-5,4)}
    };

    int tidx = (playerCount == 2 ? 0 : playerCount == 4 ? 1 : 2);
    for (int pl = 0;pl < playerCount;++pl) {
        bool ok = true;
        Player enemy = static_cast<Player>((pl + playerCount / 2) % playerCount);
        for (const auto& h : targets[tidx])
            if (boardState[h] != enemy) { ok = false; break; }
        if (ok) {
            std::wstring msg = playerNames[pl] + L" Player Wins!";
            display(); FlushBatchDraw();
            MessageBox(GetHWnd(), msg.c_str(), L"Game Over", MB_OK);
            exit(0);
        }
    }
}

POINT Board::hexToPixel(const HexCoord& hex) const
{
    double x = originX + HEX_RADIUS * (sqrt(3.0) * (hex.q + hex.r / 2.0));
    double y = originY + HEX_RADIUS * (3.0 / 2.0 * hex.r);
    return { static_cast<long>(x), static_cast<long>(y) };
}

HexCoord Board::pixelToHex(const POINT& pt) const
{
    double x = (pt.x - originX) / double(HEX_RADIUS);
    double y = (pt.y - originY) / double(HEX_RADIUS);
    double q = (sqrt(3.0) / 3.0 * x - 1.0 / 3.0 * y);
    double r = (2.0 / 3.0 * y);
    double s = -q - r;
    int rq = int(round(q)), rr = int(round(r)), rs = int(round(s));
    double dq = fabs(rq - q), dr = fabs(rr - r), ds = fabs(rs - s);
    if (dq > dr && dq > ds) rq = -rr - rs;
    else if (dr > ds)   rr = -rq - rs;
    return HexCoord(rq, rr);
}

void Board::findValidMoves(const HexCoord& from)
{
    validMoves.clear();
    int dirs[6][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,-1},{-1,1} };
    for (auto& d : dirs) {
        HexCoord n(from.q + d[0], from.r + d[1]);
        if (boardState.count(n) && boardState[n] == Player::None)
            validMoves.push_back(n);
    }
    std::map<HexCoord, bool> vis;
    findJumps(from, validMoves, vis);
}

void Board::findJumps(const HexCoord& from, std::vector<HexCoord>& moves,
    std::map<HexCoord, bool>& visited)
{
    visited[from] = true;
    int dirs[6][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,-1},{-1,1} };
    for (auto& d : dirs) {
        HexCoord m(from.q + d[0], from.r + d[1]);
        HexCoord t(from.q + 2 * d[0], from.r + 2 * d[1]);
        if (boardState.count(m) && boardState[m] != Player::None &&
            boardState.count(t) && boardState[t] == Player::None && !visited[t])
        {
            moves.push_back(t);
            findJumps(t, moves, visited);
        }
    }
}

void Board::display()
{
    cleardevice();
    for (const auto& kv : boardState) {
        if (kv.second == Player::None) {
            setfillcolor(RGB(60, 60, 60));
            POINT p = hexToPixel(kv.first);
            solidcircle(p.x, p.y, int(HEX_RADIUS * 0.4));
        }
        else {
            setfillcolor(getPlayerColor(kv.second));
            POINT p = hexToPixel(kv.first);
            solidcircle(p.x, p.y, int(HEX_RADIUS * 0.8));
        }
    }
    if (hasSelection) {
        setlinecolor(YELLOW); setlinestyle(PS_SOLID, 3);
        POINT p = hexToPixel(selectedHex);
        circle(p.x, p.y, int(HEX_RADIUS * 0.9));
    }
    setfillcolor(WHITE);
    for (const auto& m : validMoves) {
        POINT p = hexToPixel(m);
        solidcircle(p.x, p.y, int(HEX_RADIUS * 0.3));
    }
    drawUI();
}

void Board::drawUI()
{
    settextcolor(getPlayerColor(static_cast<Player>(currentPlayer)));
    settextstyle(24, 0, _T("微软雅黑")); setbkmode(TRANSPARENT);
    std::wstring txt = L"Current: " + playerNames[currentPlayer];
    outtextxy(10, 10, txt.c_str());
    settextcolor(WHITE); settextstyle(16, 0, _T("Arial"));
    outtextxy(10, windowHeight - 30, _T("Right-click cancel,  'U' undo"));
}

void Board::switchPlayer()
{
    currentPlayer = (currentPlayer + 1) % playerCount;
}

void Board::undoMove()
{
    if (moveHistory.empty()) return;
    MoveRecord last = moveHistory.back();
    moveHistory.pop_back();
    boardState[last.from] = last.fromPlayer;
    boardState[last.to] = last.toPlayerBefore;
    currentPlayer = static_cast<int>(last.fromPlayer);
    hasSelection = false; validMoves.clear();
}

COLORREF Board::getPlayerColor(Player p)
{
    if (p == Player::None) return DARKGRAY;
    return playerColors[static_cast<int>(p)];
}
std::wstring Board::getPlayerName(Player p)
{
    if (p == Player::None) return L"None";
    return playerNames[static_cast<int>(p)];
}

void Board::drawHexagonOutline(const HexCoord& hex)
{
    POINT pts[6]; POINT c = hexToPixel(hex);
    for (int i = 0;i < 6;++i) {
        pts[i].x = long(c.x + HEX_RADIUS * cos(PI / 3 * i));
        pts[i].y = long(c.y + HEX_RADIUS * sin(PI / 3 * i));
    }
    setlinecolor(DARKGRAY); setlinestyle(PS_SOLID, 1);
    polygon(pts, 6);
}

void Board::handleMouseClick(const MOUSEMSG& msg)
{
    POINT p = { msg.x,msg.y };
    HexCoord clicked = pixelToHex(p);
    if (boardState.find(clicked) == boardState.end()) {
        hasSelection = false; validMoves.clear(); return;
    }
    if (hasSelection) {
        bool ok = false;
        for (const auto& m : validMoves) if (m == clicked) { ok = true; break; }
        if (ok) {
            moveHistory.push_back({ selectedHex,clicked,
                                  static_cast<Player>(currentPlayer),boardState[clicked] });
            boardState[clicked] = boardState[selectedHex];
            boardState[selectedHex] = Player::None;
            hasSelection = false; validMoves.clear();
            switchPlayer(); checkVictory();
        }
        else if (boardState[clicked] == static_cast<Player>(currentPlayer)) {
            selectedHex = clicked; findValidMoves(clicked);
        }
        else {
            hasSelection = false; validMoves.clear();
        }
    }
    else {
        if (boardState[clicked] == static_cast<Player>(currentPlayer)) {
            hasSelection = true; selectedHex = clicked; findValidMoves(clicked);
        }
    }
}

void Board::run()
{
    BeginBatchDraw();
    while (true) {
        display(); FlushBatchDraw();
        if (MouseHit()) {
            MOUSEMSG msg = GetMouseMsg();
            if (msg.uMsg == WM_LBUTTONDOWN) handleMouseClick(msg);
            else if (msg.uMsg == WM_RBUTTONDOWN) { hasSelection = false; validMoves.clear(); }
        }
        if (_kbhit()) {
            char c = _getch(); if (c == 'u' || c == 'U') undoMove();
        }
    }
    EndBatchDraw();
}