# Generate C string constants for Chinese text
strings = [
    ('S_HUB_TITLE', '游 戏 大 厅'),
    ('S_PLANE_NAME', '飞机大战'),
    ('S_MAZE_NAME', '迷宫'),
    ('S_GOMOKU_NAME', '三子棋'),
    ('S_TETRIS_NAME', '俄罗斯方块'),
    ('S_PAUSED', '暂停'),
    ('S_CONTINUE', 'K4继续'),
    ('S_EXIT', 'K1退出'),
    ('S_END', 'K1结束'),
    ('S_GAME_OVER', '游戏结束'),
    ('S_SCORE', '得分'),
    ('S_HIGH', '最高'),
    ('S_NEW_RECORD', '新纪录!'),
    ('S_REPLAY', 'K4重玩'),
    ('S_START', 'K4开始'),
    ('S_CTRL_TETRIS', '左 右 旋 停'),
    ('S_FIND_EXIT', '寻找出口!'),
    ('S_MOVE_HINT', 'K1-4:移动'),
    ('S_YOU_WIN', '你赢了!'),
    ('S_ESCAPED', '成功逃脱!'),
    ('S_NEW_MAZE', 'K4新迷宫'),
    ('S_LEVEL', '等级'),
    ('S_GOMOKU_BLACK', '黑方'),
    ('S_GOMOKU_WHITE', '白方'),
    ('S_GOMOKU_NEW', 'K4新局'),
    ('S_GOMOKU_MOVE', '移动'),
    ('S_GOMOKU_PUT', '落子'),
    ('S_BLACK_WINS', '黑方胜!'),
    ('S_WHITE_WINS', '白方胜!'),
]

for name, text in strings:
    b = text.encode('utf-8')
    hex_str = ''.join('\\x%02X' % x for x in b)
    print('#define %-22s "%s"' % (name, hex_str))
