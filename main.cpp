#include "Board.h"
#include "ChooseMode.h"
#include <graphics.h>

int main()
{
    int playerCount = ChooseMode();      // 2/4/6
    int aiMode = chooseAI();        // 0 人人  1 人机
    bool humanFirst = false;
    if (aiMode == 1) {
        int whoFirst = MessageBox(GetHWnd(), L"人类先手？", L"先手选择", MB_YESNO);
        humanFirst = (whoFirst == IDYES);
    }

    int W = 800, H = 900;
    initgraph(W, H);
    setbkcolor(RGB(40, 40, 40));
    Board game(W, H, playerCount, aiMode, humanFirst);
    game.run();
    closegraph();
    return 0;
}