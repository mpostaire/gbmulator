import imageio as iio
import os.path as path

img = iio.imread(path.join(path.dirname(__file__), "font.png"))

def parse_char(c, endl=', '):
    x = ord(c) - 32
    y = (x // 16) * 8
    x = (x % 16) * 8

    r = "    { "
    for i in range(y, y + 8):
        byte = ""
        for j in range(x, x + 8):
            pixel = img[i][j]
            if tuple(pixel) == (255, 255, 255):
                byte += "0"
            else:
                byte += "1"
        if i < y + 7:
            r += f'0x{int(byte, 2):02X}, '
        else:
            r += f'0x{int(byte, 2):02X} }}{endl}'
    return r + "\n"

CHARS = " !\"#$%&'()*+,-./0123456789:;>=<?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
code = f"const byte_t font[0x{len(CHARS):X}][0x8] = {{\n"
for c in CHARS:
    code += parse_char(c, endl='' if c == '~' else ',')
code += "};"
print(code)
