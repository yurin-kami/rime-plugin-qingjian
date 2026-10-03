"""
build_gloss 的单元测试：验证 TSV 解析与二进制写入的格式正确性。

运行：
    python3 -m unittest tests.test_build_gloss
"""

import os
import struct
import tempfile
import unittest

import tools.build_gloss as bg


SAMPLE = """\
# 注释行
开发\tv. develop\tv. exploit
中文\tn. Chinese
你好\tint. hello\tint. hi
苹果\tn. apple
"""


class ParseTsvTest(unittest.TestCase):
    def test_跳过注释并按字节序排序(self):
        items = bg.parse_tsv(SAMPLE)
        words = [w for w, _ in items]
        self.assertEqual(words, ["中文", "你好", "开发", "苹果"])

    def test_去重后保留最后一次(self):
        items = bg.parse_tsv("开发\tv. develop\tv. exploit\n开发\tv. code\n")
        self.assertEqual(dict(items)["开发"], "v. code")

    def test_最多保留两条释义(self):
        items = bg.parse_tsv(SAMPLE)
        self.assertEqual(dict(items)["你好"], "int. hello / int. hi")

    def test_去掉词性前缀(self):
        items = bg.parse_tsv(SAMPLE, strip_pos=True)
        self.assertEqual(dict(items)["开发"], "develop / exploit")
        self.assertEqual(dict(items)["中文"], "Chinese")

    def test_去掉读音部分(self):
        items = bg.parse_tsv("開発\tn. 開発|かいはつ\n")
        self.assertEqual(dict(items)["開発"], "n. 開発")


class BuildBinTest(unittest.TestCase):
    def _build_and_read(self, items):
        fd, path = tempfile.mkstemp(suffix=".bin")
        os.close(fd)
        try:
            bg.build_bin(items, path)
            with open(path, "rb") as f:
                return f.read()
        finally:
            os.remove(path)

    def test_文件头与条目数(self):
        data = self._build_and_read([("开发", "v. develop")])
        self.assertEqual(data[:8], b"QJGLOSS1")
        self.assertEqual(struct.unpack("<Q", data[8:16])[0], 1)

    def test_索引与arena偏移长度正确(self):
        items = [("开发", "v. develop"), ("中文", "n. Chinese")]
        data = self._build_and_read(items)
        count = struct.unpack("<Q", data[8:16])[0]
        self.assertEqual(count, 2)
        arena_start = bg.HEADER_SIZE + count * bg.ENTRY_SIZE
        arena = data[arena_start:]
        key_off, key_len, val_off, val_len = struct.unpack(
            "<IIII", data[16:32]
        )
        self.assertEqual(
            arena[key_off:key_off + key_len].decode("utf-8"), "开发"
        )
        self.assertEqual(
            arena[val_off:val_off + val_len].decode("utf-8"), "v. develop"
        )

    def test_空表可写入(self):
        data = self._build_and_read([])
        self.assertEqual(data[:8], b"QJGLOSS1")
        self.assertEqual(struct.unpack("<Q", data[8:16])[0], 0)
        self.assertEqual(len(data), 16)


if __name__ == "__main__":
    unittest.main()
