"""
把青简释义表 TSV 转成二进制索引 gloss.bin。

源格式（见 qingjian assets/glossary/README.md）：
    词\\t[词性. ]译词[|读音]\\t...
    # 开头为注释行；按码点排序；UTF-8 无 BOM。

产物格式（小端序，见 arch.md）：
    [0,8)   magic "QJGLOSS1"
    [8,16)  条目数 N（u64 LE）
    之后    N 条定宽索引，每条 16 字节：key_off/key_len/val_off/val_len（u32 LE）
    之后    字符串 arena：key 字节后接 value 字节，由偏移与长度分隔

用法：
    python3 tools/build_gloss.py <in.tsv> <out.bin> [选项]
"""

import argparse
import re
import struct

MAGIC = b"QJGLOSS1"
HEADER_SIZE = 16
ENTRY_SIZE = 16

_POS_PREFIX = re.compile(r"^[a-z]+\.\s+")


def _strip_pos(sense):
    """去掉词性前缀（如 'v. develop' -> 'develop'）。"""
    return _POS_PREFIX.sub("", sense)


def parse_tsv(text, max_senses=2, strip_pos=False, separator=" / ",
              lowercase_key=False):
    """
    解析 TSV 文本，返回按 UTF-8 字节序排好、去重后的 (词, 释义) 列表。

    任一行缺词或缺释义会跳过该行；释义取前 max_senses 条，
    去掉读音（| 之后的部分），按 separator 连接。lowercase_key 为真时
    把词转小写（英→中词典的键为小写）。
    """
    entries = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        word = fields[0].strip()
        if not word:
            continue
        if lowercase_key:
            word = word.lower()
        senses = []
        for field in fields[1:]:
            sense = field.strip()
            if not sense:
                continue
            if "|" in sense:
                sense = sense.split("|", 1)[0].strip()
            if strip_pos:
                sense = _strip_pos(sense)
            if sense:
                senses.append(sense)
            if len(senses) >= max_senses:
                break
        if not senses:
            continue
        entries[word] = separator.join(senses)
    return sorted(entries.items(), key=lambda kv: kv[0].encode("utf-8"))


def build_bin(items, path):
    """按格式写 gloss.bin；items 为已排好序的 (词, 释义) 列表。"""
    arena = bytearray()
    index = bytearray()
    for word, value in items:
        word_bytes = word.encode("utf-8")
        value_bytes = value.encode("utf-8")
        key_off = len(arena)
        arena += word_bytes
        val_off = len(arena)
        arena += value_bytes
        index += struct.pack(
            "<IIII", key_off, len(word_bytes), val_off, len(value_bytes)
        )
    with open(path, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<Q", len(items)))
        f.write(index)
        f.write(arena)


def main():
    parser = argparse.ArgumentParser(
        description="把青简释义表 TSV 转成二进制索引 gloss.bin"
    )
    parser.add_argument("input", help="输入 TSV 文件")
    parser.add_argument("output", help="输出 gloss.bin 文件")
    parser.add_argument("--max-senses", type=int, default=2, help="最多保留几条释义")
    parser.add_argument("--strip-pos", action="store_true", help="去掉词性前缀")
    parser.add_argument(
        "--separator", default=" / ", help="多条释义之间的连接符"
    )
    parser.add_argument(
        "--lowercase-key", action="store_true", help="把词转小写（英→中词典）"
    )
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as f:
        text = f.read()
    items = parse_tsv(
        text, max_senses=args.max_senses, strip_pos=args.strip_pos,
        separator=args.separator, lowercase_key=args.lowercase_key,
    )
    build_bin(items, args.output)
    print(f"已写入 {args.output}：{len(items)} 条")


if __name__ == "__main__":
    main()
