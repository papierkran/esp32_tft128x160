from PIL import Image, ImageFont, ImageDraw

# 需要的汉字
chars = "长沙深圳邵东大理晴多云阴小雨中大雨阵雪雾霾温度湿风力级正在获取失败重启功能名称地址信号强弱时间连接天气息首次使用请配网蓝牙状已等待保存清除向东西南北"

# 加载字体
font = ImageFont.truetype("MiSans-Medium.woff2", 12)

output = []

for ch in chars:
    # 创建16x16的图像
    img = Image.new('1', (16, 16), 0)  # 黑色背景
    draw = ImageDraw.Draw(img)
    # 居中绘制汉字
    bbox = draw.textbbox((0, 0), ch, font=font)
    x = (16 - (bbox[2] - bbox[0])) // 2
    y = (16 - (bbox[3] - bbox[1])) // 2
    draw.text((x, y), ch, font=font, fill=1)  # 白色前景

    # 转换为点阵数据
    byte_arr = []
    for row in range(16):
        val = 0
        for col in range(8):
            if img.getpixel((col, row)):
                val |= (0x80 >> col)
        byte_arr.append(f"0x{val:02X}")

        val = 0
        for col in range(8, 16):
            if img.getpixel((col, row)):
                val |= (0x80 >> (col - 8))
        byte_arr.append(f"0x{val:02X}")

    output.append(f"  // U+{ord(ch):04X} {ch}\n  " + ", ".join(byte_arr))

# 生成C代码
c_code = "// Auto-generated 16x16 bitmap font data\n"
c_code += "// Characters: " + chars + "\n\n"
c_code += "#ifndef CN_FONT_H\n#define CN_FONT_H\n\n"
c_code += "#include <Arduino.h>\n\n"

c_code += f"const uint8_t cn_font_data[][32] PROGMEM = {{\n"
for i, o in enumerate(output):
    c_code += f"  {{{o}}},\n"
c_code += "};\n\n"

c_code += "const uint16_t cn_font_chars[] PROGMEM = {\n"
for ch in chars:
    c_code += f"  0x{ord(ch):04X}, // {ch}\n"
c_code += "  0\n};\n\n"

c_code += "#endif\n"

with open("cn_font.h", "w", encoding="utf-8") as f:
    f.write(c_code)

print("Generated cn_font.h with", len(chars), "characters")
print(c_code)