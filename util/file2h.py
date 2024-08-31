# file2h.py  Ver.0.1
#
# バイナリファイルをArduinoの.hファイルに変換する
#
# 使い方
#   python file2h.py input.wav output.h
#
# Copyright (c) 2023 kaz  (https://akibabara.com/blog/)
# Released under the MIT license.
# see https://opensource.org/licenses/MIT
import sys

## バイナリファイルを.hファイルに変換する
def bin_to_progmem(input_file, output_file):
    # バイナリファイルを開く
    with open(input_file, 'rb') as f:
        data = f.read()
    
    # 16バイトごとに区切っていく
    cpp_data = [f"0x{byte:02x}" for byte in data]
    cpp_data_chunks = [', '.join(cpp_data[i:i+16]) for i in range(0, len(cpp_data), 16)]
    cpp_data_string = ',\n'.join(cpp_data_chunks)
    cpp_program = f"const unsigned char sound000[{len(data)}] PROGMEM = {{\n{cpp_data_string}\n}};"
    
    # 保存
    with open(output_file, 'w') as f:
        f.write(cpp_program)

## メイン
if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python file2h.py input.wav output.h")
    else:
        bin_to_progmem(sys.argv[1], sys.argv[2])

