#include "Board.h"
#include "ChooseMode.h"
#include <graphics.h>

int main()
{
    int playerCount = ChooseMode();   // 2¡¢4¡¢6

    int W = 800, H = 900;
    initgraph(W, H);
    setbkcolor(RGB(40, 40, 40));
    setlinecolor(WHITE);

    Board game(W, H, playerCount);
    game.run();

    closegraph();
    return 0;
}