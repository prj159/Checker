# checker - 跳棋AI项目

## 项目概述
C++ 跳棋游戏，支持 2/4/6 人对战，AI 使用 MCTS (蒙特卡洛树搜索) + RAVE + 开局库。

## 构建
```
& "D:\VS\MSBuild\Current\Bin\MSBuild.exe" .\checker.sln /m /p:Configuration=Debug /p:Platform=x64
```

## 关键文件
| 文件 | 作用 |
|------|------|
| `main.cpp` | 入口：支持 --selfplay N [playerCount] 自对弈训练、--tune N 参数搜索、普通游戏 |
| `Board.h/cpp` | 六边形棋盘逻辑，EasyX 图形渲染 |
| `AI_MCTS.h/cpp` | MCTS AI 引擎：RAVE、置换表、开局库、对外接口 |
| `ChooseMode.h/cpp` | 游戏模式/先后手选择菜单 |
| `selfplay.h/cpp` | 自对弈训练：批量对局、开局库自动保存、参数网格搜索 |

## AI 接口 (namespace AI)
- `getMove(bd, who)` → MoveRecord
- `getMoveHeadless(boardState, playerCount, who)` → MoveRecord (无图形)
- `loadOpeningBook(filename)` / `saveOpeningBook(filename)`
- `recordGameToOpeningBook(history, winner, playerCount)`
- `resetAI()`
- `AI_LEVEL()` → 难度 1-3

## 依赖
- EasyX (图形库): vcpkg x64-windows
- C++17

## 注意事项
- **源文件含 UTF-8 中文注释，vcxproj 必须加 /utf-8 编译选项**，否则 GBK 解析导致语法错误
- 开局库 `opening_book.dat` 放在 exe 同级目录自动加载
