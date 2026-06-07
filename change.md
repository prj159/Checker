# Change Log

编译代码的方法

& 'D:\VS\MSBuild\Current\Bin\MSBuild.exe' .\checker.sln /m /p:Configuration=Debug /p:Platform=x64

记录每个日期所做的更改：

## 2025-10-07
- Initial commit: Chinese Checker game implementation
- 添加了人机对战

## 2025-11-10
- 修复了胜利判定

## 2025-12-18
- Add MCTS AI and backup AI, update board logic

## 2026-05-25
- 提高了AI的能力
- 增强MCTS算法，增加自博弈学习

## 2026-06-05
- 初始化 `change.md` 文件，用于记录后续的更改。
- 更改ignore以确保有些可以上传

## 2026-06-05a
- 修改中文注释代码出现乱码

## 2026-06-06
- 修改6名玩家的胜利判定
- 修改README.md文件

## 2026-06-06a
- 在该文件开头增加了每次可以编译代码的方法
- 添加了ai的训练数据，这是测试50轮的

## 2026-06-07
- 修复 vcxproj 文件，在编译器选项中加上 /utf-8，以此解决因修改中文注释代码出现乱码产生的编译错误