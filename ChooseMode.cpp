#include "ChooseMode.h"
#include <graphics.h>
#include <conio.h>

int ChooseMode()
{
    const int wid = 400, hei = 300;
    initgraph(wid, hei);
    setbkcolor(RGB(40, 40, 40));
    cleardevice();
    settextstyle(24, 0, _T("微软雅黑"));
    settextcolor(WHITE);
    outtextxy(120, 40, _T("请选择人数"));
    const wchar_t* txt[3] = { _T("2 人"), _T("4 人"), _T("6 人") };
    for (int i = 0; i < 3; ++i)
    {
        outtextxy(160, 110 + i * 50, txt[i]);
        rectangle(150, 105 + i * 50, 250, 145 + i * 50);
    }
    int choice = 0;
    while (!choice)
    {
        if (MouseHit())
        {
            MOUSEMSG m = GetMouseMsg();
            if (m.uMsg == WM_LBUTTONDOWN)
                for (int i = 0; i < 3; ++i)
                    if (m.x >= 150 && m.x <= 250 &&
                        m.y >= 105 + i * 50 && m.y <= 145 + i * 50)
                        choice = 2 + i * 2;   // 2,4,6
        }
    }
    closegraph();
    return choice;
}