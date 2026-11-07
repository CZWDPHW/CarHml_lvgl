#!/usr/bin/env python3
"""
png2bin.py - 把 PNG 批量转成 LVGL bin 图片

为什么要转：LVGL 运行期解码 PNG 需要额外的解码器依赖，且大图每帧解码
在板端 CPU 上不现实。bin 是预解码的原始像素，运行期零解码开销。

输出两种格式：
  bin32/  ARGB8888，PC 模拟器（LV_COLOR_DEPTH=32）使用
  bin16/  RGB565A8，板端 framebuffer（LV_COLOR_DEPTH=16）使用，保留透明

文件布局 = lv_image_header_t(16B) + 像素数据，与 LVGL 9.5 bin 解码器一致：
  magic(8) cf(8) flags(16) w(16) h(16) stride(16) reserved(16)

用法：
  python3 tools/png2bin.py                  # 增量：只转 PNG 比 bin 新的
  python3 tools/png2bin.py --force          # 全部重转
  python3 tools/png2bin.py --only SubaruForester
"""
import argparse
import os
import struct
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("需要 Pillow：pip3 install pillow")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(ROOT, "assets", "src")
OUT_DIRS = {
    "bin32": os.path.join(ROOT, "assets", "bin32"),
    "bin16": os.path.join(ROOT, "assets", "bin16"),
}

MAGIC = 0x19
CF_ARGB8888 = 0x10
CF_RGB565A8 = 0x14

# src 下这些目录里的文件在 bin 里只保留"父目录名 + 文件名"，
# 即去掉中间那一层（SubaruForester/navigation/x.png -> SubaruForester/x.bin），
# 与既有 assets/bin32 的布局保持一致
FLATTEN_DIRS = ("SubaruForester/navigation", "material2/RotateSheet")

# 指针图按非透明像素裁剪后再转 bin：
# 620x620 的指针图若整图参与 lv_image_set_rotation，软件旋转的待绘制包围盒
# 约为 876x876 像素，板端每帧两次旋转会导致明显卡顿。裁到真实笔尖范围
# (64x267) 后可把包围盒降到约 482x482。
# 裁剪后指针在表盘内的摆放偏移与旋转轴心需与 C 代码保持一致：
#   speed_forester1.c / rotate_forester1.c 中的 NEEDLE_* 常量。
# 源 PNG 本身的包围盒必须仍是 (278,71)-(342,338)，否则需要同步改常量。
CROP_TO_ALPHA_BBOX = {
    "SubaruForester/speed_point1.png",
    "SubaruForester/rotate_point1.png",
}


def bin_rel_path(rel):
    """把 src 下的相对路径映射成 bin 输出路径"""
    rel = rel.replace(os.sep, "/")
    for sub in FLATTEN_DIRS:
        prefix = sub + "/"
        if rel.startswith(prefix):
            parent = sub.split("/")[0]
            return parent + "/" + rel[len(prefix):]
    return rel


def pack_header(cf, w, h, stride):
    """组装 16 字节 lv_image_header_t"""
    return struct.pack("<BBHHHHH", MAGIC, cf, 0, w, h, stride, 0)

def encode_argb8888(im):
    """ARGB8888：每像素 4 字节，内存顺序 B,G,R,A（小端）"""
    im = im.convert("RGBA")
    r, g, b, a = im.split()
    return Image.merge("RGBA", (b, g, r, a)).tobytes()

def encode_rgb565a8(im):
    """RGB565A8：先 RGB565 像素区，再 1 字节/像素的 alpha 区"""
    im = im.convert("RGBA")
    w, h = im.size
    r, g, b, a = im.split()

    # RGB565 小端：RRRRRGGG GGGBBBBB
    r5 = r.point(lambda v: v >> 3)
    g6 = g.point(lambda v: v >> 2)
    b5 = b.point(lambda v: v >> 3)

    px = bytearray(w * h * 2)
    i = 0
    for (rr, gg, bb) in zip(r5.getdata(), g6.getdata(), b5.getdata()):
        v = (rr << 11) | (gg << 5) | bb
        px[i] = v & 0xFF
        px[i + 1] = v >> 8
        i += 2

    return bytes(px) + a.tobytes()

JOBS = (
    ("bin32", CF_ARGB8888, 4, encode_argb8888),
    ("bin16", CF_RGB565A8, 2, encode_rgb565a8),
)

def convert(png_path, rel, force):
    """转换单张 PNG；已是最新且未指定 --force 时跳过"""
    im = Image.open(png_path).convert("RGBA")
    if rel.replace(os.sep, "/") in CROP_TO_ALPHA_BBOX:
        bbox = im.getchannel("A").getbbox()
        if bbox is not None:
            im = im.crop(bbox)
    w, h = im.size

    for name, cf, bpp, encode in JOBS:
        out_rel = bin_rel_path(rel)
        out = os.path.join(OUT_DIRS[name], out_rel[:-4] + ".bin")

        if not force and os.path.exists(out):
            if os.path.getmtime(out) >= os.path.getmtime(png_path):
                continue

        os.makedirs(os.path.dirname(out), exist_ok=True)

        with open(out, "wb") as f:
            f.write(pack_header(cf, w, h, w * bpp))
            f.write(encode(im))

        print("  %-6s %s -> %s (%dx%d)" % (name, rel, out_rel, w, h))

def main():
    ap = argparse.ArgumentParser(description="PNG -> LVGL bin")
    ap.add_argument("--force", action="store_true", help="忽略时间戳全部重转")
    ap.add_argument("--only", default=None, help="只转指定子目录，如 SubaruForester")
    args = ap.parse_args()

    if not os.path.isdir(SRC_DIR):
        sys.exit("找不到源目录：%s" % SRC_DIR)

    count = 0
    for dirpath, _, files in os.walk(SRC_DIR):
        for fn in sorted(files):
            if not fn.lower().endswith((".png", ".jpg")):
                continue

            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, SRC_DIR)

            if args.only and not rel.startswith(args.only):
                continue

            convert(full, rel, args.force)
            count += 1

    print("处理 %d 张，输出到 assets/bin32 与 assets/bin16" % count)

if __name__ == "__main__":
    main()