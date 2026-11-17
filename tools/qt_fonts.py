#!/usr/bin/env python3
"""Generate static Qt-family fonts. No runtime dependency on Qt or system fonts.

python tools/qt_fonts.py --qt-font-dir D:/share/CarHml_XIAO/font \
    --converter /path/to/node_modules/lv_font_conv/lv_font_conv.js

Unspecified QML default Latin families retain Montserrat until Qt runtime font
resolution has been captured. Explicit Arial uses --arial-dir (Windows Fonts).
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SIZES = (14, 16, 18, 20, 22, 24, 26, 27, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 62, 64)


def sanitize_geely(source, output):
    # This supplied font has an invalid OpenType ClassDef in GPOS. FreeType/Qt
    # accept its outlines; opentype.js rejects it. Remove only shaping tables
    # from a build-cache copy, preserving cmap, outlines and the legacy kern.
    data = source.read_bytes()
    count = struct.unpack_from('>H', data, 4)[0]
    tables = [struct.unpack_from('>4sIII', data, 12 + i * 16) for i in range(count)]
    tables = [t for t in tables if t[0] not in (b'GPOS', b'GSUB', b'GDEF')]
    count = len(tables)
    power = 2 ** (count.bit_length() - 1)
    result = bytearray(data[:4] + struct.pack('>HHHH', count, power * 16,
                                             power.bit_length() - 1, count * 16 - power * 16)
                       + bytes(count * 16))
    for i, (tag, checksum, offset, length) in enumerate(tables):
        result += bytes((-len(result)) % 4)
        dest = len(result)
        result += data[offset:offset + length]
        if tag == b'head':
            head = dest
            result[dest + 8:dest + 12] = bytes(4)
        struct.pack_into('>4sIII', result, 12 + i * 16, tag, checksum, dest, length)
    result += bytes((-len(result)) % 4)
    checksum = sum(struct.unpack(f'>{len(result) // 4}I', result)) & 0xffffffff
    struct.pack_into('>I', result, head + 8, (0xB1B0AFBA - checksum) & 0xffffffff)
    output.write_bytes(result)


def extract_sc(collection, output):
    """Extract the SC face from an SFNT collection, preserving table contents."""
    data = collection.read_bytes()
    if data[:4] != b'ttcf':
        raise ValueError('Expected the original NotoSansCJK TTC collection')
    for index in range(struct.unpack_from('>I', data, 8)[0]):
        start = struct.unpack_from('>I', data, 12 + index * 4)[0]
        count = struct.unpack_from('>H', data, start + 4)[0]
        tables = [struct.unpack_from('>4sIII', data, start + 12 + i * 16) for i in range(count)]
        _, _, off, length = next(t for t in tables if t[0] == b'name')
        names = data[off:off + length]
        _, records, strings = struct.unpack_from('>HHH', names)
        families = []
        for i in range(records):
            platform, _, _, name_id, n, pos = struct.unpack_from('>HHHHHH', names, 6 + 12 * i)
            if name_id in (1, 16):
                families.append(names[strings + pos:strings + pos + n].decode(
                    'utf-16-be' if platform in (0, 3) else 'latin1', errors='replace'))
        if not any('CJK SC' in name for name in families):
            continue
        result = bytearray(data[start:start + 12] + bytes(count * 16))
        head = None
        for i, (tag, checksum, offset, length) in enumerate(tables):
            result += bytes((-len(result)) % 4)
            dest = len(result)
            result += data[offset:offset + length]
            if tag == b'head':
                head = dest
                result[dest + 8:dest + 12] = bytes(4)
            struct.pack_into('>4sIII', result, 12 + i * 16, tag, checksum, dest, length)
        result += bytes((-len(result)) % 4)
        checksum = sum(struct.unpack(f'>{len(result) // 4}I', result)) & 0xffffffff
        struct.pack_into('>I', result, head + 8, (0xB1B0AFBA - checksum) & 0xffffffff)
        output.write_bytes(result)
        return
    raise ValueError('NotoSansCJK collection has no Simplified Chinese face')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qt-font-dir', type=Path, required=True)
    parser.add_argument('--converter', type=Path, required=True)
    parser.add_argument('--arial-dir', type=Path, default=Path('C:/Windows/Fonts'))
    parser.add_argument('--node', default='node')
    args = parser.parse_args()
    dest = ROOT / 'src/ui/fonts'
    cache = ROOT / '.cache/qt-fonts'
    dest.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    cjk = cache / 'NotoSansCJK-SC.otf'
    collection = args.qt_font_dir / 'NotoSansCJK-Regular.ttc'
    extract_sc(collection, cjk)
    geely = cache / 'Geely_Bold.ttf'
    sanitize_geely(args.qt_font_dir / 'Geely_Bold.ttf', geely)
    translation_sources = [ROOT / 'src/ui/ui_i18n.c', ROOT / 'src/ui/settings/ui_settings_language.c']
    symbols = ''.join(sorted(set('℃℉°−–…×％：，。（）') | {
        c for source in translation_sources for c in source.read_text(encoding='utf-8') if ord(c) > 127}))
    medium = ROOT / 'third_party/lvgl/scripts/built_in_font/Montserrat-Medium.ttf'
    bold = ROOT / 'third_party/lvgl/tests/src/test_files/fonts/Montserrat-Bold.ttf'
    specs = [('text', size, False, medium, True, False) for size in SIZES]
    specs += [('text', size, True, bold, True, False)
              for size in (18, 20, 22, 24, 26, 27, 28, 30, 32, 36, 40, 42)]
    specs += [('geely', size, True, geely, True, False)
              for size in (14, 18, 20, 24, 27, 32, 34, 36)]
    specs += [('konexy', size, False, args.qt_font_dir / 'KonexyPersonalUse.otf', False, False)
              for size in (24, 28, 44, 48)]
    specs += [('forgotten', 62, False, args.qt_font_dir / 'ForgottenFuturist.otf', False, False)]
    specs += [('bebas', size, weight, args.qt_font_dir / 'Bebas.ttf', False, weight)
              for size, weight in ((28, False), (36, True))]
    specs += [('arial', size, weight, args.arial_dir / ('arialbd.ttf' if weight else 'arial.ttf'), True, False)
              for size, weight in ((24, False), (24, True))]
    entries, manifest = [], []
    for family, size, weight, source, chinese, synthetic in specs:
        name = f'ui_qt_{family}_{size}' + ('_bold' if weight else '')
        output = dest / f'{name}.c'
        cmd = [args.node, str(ROOT / 'tools/font_convert.cjs'), str(args.converter),
               'bold' if synthetic else ('cjk-bold' if chinese and weight else 'regular'),
               '--size', str(size), '--bpp', '4', '--format', 'lvgl',
               '--lv-include', 'lvgl.h', '--lv-font-name', name,
               '--font', str(source), '--range', '0x20-0x7e']
        if chinese:
            cmd += ['--font', str(cjk), '--symbols', symbols]
        elif family == 'bebas' and size == 28:
            # The original Bebas face has no degree sign. Qt supplies it from
            # its fallback family; merge only that glyph for temperature units.
            cmd += ['--font', str(cjk), '--symbols', '°']
        cmd += ['-o', str(output)]
        run = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace')
        if run.returncode:
            raise RuntimeError(f'{name}: {run.stderr}')
        # Avoid host-specific paths and long symbol lists in generated comments.
        text = output.read_text(encoding='utf-8')
        text = re.sub(r' \* Opts:.*', f' * Source: {source.name}; generated by tools/qt_fonts.py', text)
        output.write_text(text.rstrip() + '\n', encoding='utf-8', newline='\n')
        entries.append((family.upper(), size, weight, name))
        manifest.append(dict(name=name, family=family, pixels=size, bold=weight,
                             synthetic_bold=synthetic, cjk_synthetic_bold=chinese and weight,
                             source=source.name,
                             source_sha256=hashlib.sha256((args.qt_font_dir / source.name
                                 if family == 'geely' else source).read_bytes()).hexdigest(),
                             bytes=output.stat().st_size))
        if family == 'bebas' and size == 28:
            manifest[-1]['symbol_fallback'] = dict(source=collection.name,
                                                  face='Noto Sans CJK SC',
                                                  codepoints=['U+00B0'])
        print(name, flush=True)
    (dest / 'qt_fonts.inc').write_text('/* Generated by tools/qt_fonts.py. */\n' + ''.join(
        f'UI_QT_FONT({family}, {size}, {str(weight).lower()}, {name})\n'
        for family, size, weight, name in entries), encoding='utf-8')
    (dest / 'fonts.cmake').write_text('# Generated by tools/qt_fonts.py.\nset(CARMW_QT_FONT_SOURCES\n' + ''.join(
        f'    src/ui/fonts/{name}.c\n' for _, _, _, name in entries) + ')\n', encoding='utf-8')
    (dest / 'manifest.json').write_text(json.dumps(dict(
        converter_version='1.5.3', cjk_source=collection.name,
        cjk_sha256=hashlib.sha256(collection.read_bytes()).hexdigest(),
        cjk_face='Noto Sans CJK SC', glyph_symbols=symbols, fonts=manifest),
        ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
