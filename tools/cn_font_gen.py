"""
Chinese Font Bitmap Generator for STM32 OLED (SSD1306)
Uses PIL/Pillow for clean 16x16 Chinese character bitmaps.

Usage: python cn_font_gen.py
"""

from PIL import Image, ImageFont, ImageDraw
import os

RENDER_SIZE = 20   # Render slightly larger for better centering
OUT_SIZE = 16

CHARSET = (
    "飞机大战迷宫三子棋俄罗斯方块"
    "游戏厅暂停继续退出开始重玩结束"
    "得分最高新纪录等级"
    "左右移动旋转落子上下黑白发射弹"
    "胜成功逃脱寻找口局按键盘操作提示你赢了"
)


def find_font():
    candidates = [
        ("C:/Windows/Fonts/simhei.ttf", "SimHei"),
        ("C:/Windows/Fonts/msyh.ttc", "Microsoft YaHei"),
        ("C:/Windows/Fonts/simsun.ttc", "SimSun"),
        ("C:/Windows/Fonts/simkai.ttf", "KaiTi"),
    ]
    for path, name in candidates:
        if os.path.exists(path):
            return path, name
    return None, None


def render_char(ch, font, out_size=16):
    """Render a single character and extract 16x16 bitmap."""
    # Create image slightly larger for centering
    img = Image.new('L', (out_size, out_size), 0)
    draw = ImageDraw.Draw(img)

    # Get bounding box
    bbox = draw.textbbox((0, 0), ch, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]

    # Center the character
    x = (out_size - tw) // 2 - bbox[0]
    y = (out_size - th) // 2 - bbox[1]

    # Ensure we don't go negative
    if x < 0: x = 0
    if y < 0: y = 0

    draw.text((x, y), ch, fill=255, font=font)

    # Convert to SSD1306 column-major page format
    bitmap = bytearray(32)
    for col in range(16):
        byte_top = 0
        byte_bot = 0
        for bit in range(8):
            if img.getpixel((col, bit)) > 128:
                byte_top |= (1 << bit)
            if img.getpixel((col, bit + 8)) > 128:
                byte_bot |= (1 << bit)
        bitmap[col] = byte_top
        bitmap[col + 16] = byte_bot

    return bytes(bitmap)


def generate_font(charset):
    font_path, font_name = find_font()
    if font_path is None:
        print("ERROR: No Chinese font found!")
        return None

    print(f"Using font: {font_name} ({font_path})")

    font = ImageFont.truetype(font_path, OUT_SIZE)

    results = []
    for i, ch in enumerate(charset):
        bitmap = render_char(ch, font)
        results.append((ch, bitmap))
        if (i + 1) % 10 == 0:
            print(f"  ... {i + 1} chars")

    return results


def write_files(results, output_dir):
    seen = set()
    unique = []
    for ch, bitmap in results:
        if ch not in seen:
            seen.add(ch)
            unique.append((ch, bitmap))
    unique.sort(key=lambda x: ord(x[0]))

    count = len(unique)

    # Header file
    h_lines = [
        '/**',
        ' * @file    cn_font.h',
        f' * @brief   Chinese 16x16 font ({count} chars, PIL-generated)',
        ' */',
        '',
        '#ifndef __CN_FONT_H',
        '#define __CN_FONT_H',
        '',
        '#include "stm32f10x.h"',
        '',
        'extern const uint8_t CN_Font[][32];',
        '',
        'typedef struct {',
        '    uint16_t code;',
        '    uint8_t  idx;',
        '} cn_map_entry_t;',
        '',
        f'#define CN_FONT_COUNT {count}',
        '',
        'extern const cn_map_entry_t CN_Map[CN_FONT_COUNT];',
        '',
        'static uint8_t cn_font_lookup(uint16_t code)',
        '{',
        '    uint8_t lo = 0, hi = CN_FONT_COUNT;',
        '    while (lo < hi) {',
        '        uint8_t mid = (lo + hi) >> 1;',
        '        if (CN_Map[mid].code < code)',
        '            lo = mid + 1;',
        '        else',
        '            hi = mid;',
        '    }',
        '    if (lo < CN_FONT_COUNT && CN_Map[lo].code == code)',
        '        return CN_Map[lo].idx;',
        '    return 0xFF;',
        '}',
        '',
        '#endif /* __CN_FONT_H */',
    ]

    with open(os.path.join(output_dir, 'cn_font.h'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(h_lines) + '\n')

    # Source file
    c_lines = [
        '/**',
        ' * @file    cn_font.c',
        ' * @brief   Chinese 16x16 font data (PIL-generated)',
        ' */',
        '#include "cn_font.h"',
        '',
        f'const uint8_t CN_Font[{count}][32] = {{',
    ]

    for i, (ch, bitmap) in enumerate(unique):
        hx = ','.join(f'0x{b:02X}' for b in bitmap)
        c_lines.append(f'    /* {i:3d} U+{ord(ch):04X} {ch} */')
        c_lines.append(f'    {{{hx}}},')

    c_lines.append('};')
    c_lines.append('')
    c_lines.append(f'const cn_map_entry_t CN_Map[{count}] = {{')

    for i, (ch, _) in enumerate(unique):
        c_lines.append(f'    {{0x{ord(ch):04X}, {i}}},  /* {ch} */')

    c_lines.append('};')

    with open(os.path.join(output_dir, 'cn_font.c'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(c_lines) + '\n')

    print(f'\nWritten cn_font.h + cn_font.c ({count} chars, {count * 32} bytes)')


def main():
    print(f"Characters: {CHARSET}")
    print(f"Total: {len(CHARSET)} characters")
    print()

    results = generate_font(CHARSET)
    if results:
        output_dir = os.path.normpath(
            os.path.join(os.path.dirname(__file__), '..', 'Hardware'))
        write_files(results, output_dir)
        print("Done.")


if __name__ == '__main__':
    main()
