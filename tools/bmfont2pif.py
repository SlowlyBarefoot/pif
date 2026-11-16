#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Turns a BMFont export into a PifFont that pif_graphics draws with.

BMFont (https://www.angelcode.com/products/bmfont/) writes a .fnt descriptor and one page image.
This reads both, cuts each glyph out of the page, turns it into the one bit per pixel columns that
pif_graphics wants and prints C source for it.

  - The .fnt may be in the text, the XML or the binary format. The page must be a PNG or a BMP;
    both are read here, so nothing outside the standard library is needed.
  - A pixel of the page counts as set when its alpha, or its brightness where there is no alpha,
    reaches the threshold. BMFont writes white glyphs on a transparent background by default, which
    the default threshold takes as it is.
  - Only one page is handled. Uncheck "Force square" or raise the texture size in BMFont until the
    font fits one page.
  - The characters are written out as ranges of codes that follow one another, so a font of the
    ASCII characters and the Hangul syllables holds no glyphs for the codes in between.

Usage:
  tools/bmfont2pif.py [-n NAME] [-o OUTPUT] [-t THRESHOLD] [-c CHARS] FONT.fnt

Examples:
  tools/bmfont2pif.py -n my_font_16 -o my_font_16.c dejavu16.fnt
  tools/bmfont2pif.py -n hangul_16 -o hangul_16.c -c 0x20-0x7E,0xAC00-0xD7A3 nanum16.fnt

The output holds the glyph data, the ranges and the PifFont itself. Put the .c in the build and
declare the font where the application can see it:

  extern const PifFont my_font_16;
  pifGraphics_SetFont(pifSsd1306_Graphics(&g_oled), &my_font_16);
"""

import argparse
import os
import re
import struct
import sys
import zlib


# ---------------------------------------------------------------------------- page images


def _read_png(data):
    """Returns (width, height, get_pixel) for a PNG. get_pixel(x, y) gives (r, g, b, a)."""
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('not a PNG')

    pos = 8
    header = None
    palette = b''
    trns = b''
    idat = []
    while pos + 8 <= len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', body)
        elif kind == b'PLTE':
            palette = body
        elif kind == b'tRNS':
            trns = body
        elif kind == b'IDAT':
            idat.append(body)
        elif kind == b'IEND':
            break
    if not header:
        raise ValueError('the PNG has no header')

    width, height, depth, color, compression, filt, interlace = header
    if compression or filt or interlace:
        raise ValueError('only a PNG without interlacing is read')
    if depth not in (8, 16):
        if not (depth in (1, 2, 4) and color == 3):
            raise ValueError('a bit depth of %d is not read' % depth)

    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(color)
    if channels is None:
        raise ValueError('a color type of %d is not read' % color)

    raw = zlib.decompress(b''.join(idat))
    bits_per_pixel = channels * depth
    row_bytes = (width * bits_per_pixel + 7) // 8
    pixel_bytes = max(1, bits_per_pixel // 8)

    # Undo the filter of each row. The rows come one after another, each with its filter byte.
    rows = []
    previous = bytearray(row_bytes)
    pos = 0
    for _ in range(height):
        mode = raw[pos]
        line = bytearray(raw[pos + 1:pos + 1 + row_bytes])
        pos += 1 + row_bytes
        for i in range(row_bytes):
            a = line[i - pixel_bytes] if i >= pixel_bytes else 0
            b = previous[i]
            c = previous[i - pixel_bytes] if i >= pixel_bytes else 0
            if mode == 0:
                value = line[i]
            elif mode == 1:
                value = line[i] + a
            elif mode == 2:
                value = line[i] + b
            elif mode == 3:
                value = line[i] + (a + b) // 2
            elif mode == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                value = line[i] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))
            else:
                raise ValueError('a filter of %d is not read' % mode)
            line[i] = value & 0xFF
        rows.append(line)
        previous = line

    step = depth // 8 if depth >= 8 else 0

    def get_pixel(x, y):
        row = rows[y]
        if depth < 8:
            per_byte = 8 // depth
            value = (row[x // per_byte] >> ((per_byte - 1 - x % per_byte) * depth)) & ((1 << depth) - 1)
            index = value
        elif color == 3:
            index = row[x * step]
        if color == 3:
            r, g, b = palette[index * 3:index * 3 + 3]
            a = trns[index] if index < len(trns) else 255
            return r, g, b, a
        base = x * channels * step
        value = [row[base + i * step] for i in range(channels)]
        if color == 0:
            return value[0], value[0], value[0], 255
        if color == 4:
            return value[0], value[0], value[0], value[1]
        if color == 2:
            return value[0], value[1], value[2], 255
        return value[0], value[1], value[2], value[3]

    return width, height, get_pixel


def _read_bmp(data):
    """Returns (width, height, get_pixel) for an uncompressed BMP of 8, 24 or 32 bits."""
    if data[:2] != b'BM':
        raise ValueError('not a BMP')

    offset = struct.unpack('<I', data[10:14])[0]
    header_size = struct.unpack('<I', data[14:18])[0]
    width, height = struct.unpack('<ii', data[18:26])
    depth = struct.unpack('<H', data[28:30])[0]
    compression = struct.unpack('<I', data[30:34])[0]
    if compression:
        raise ValueError('only an uncompressed BMP is read')
    if depth not in (8, 24, 32):
        raise ValueError('a bit depth of %d is not read' % depth)

    palette = data[14 + header_size:offset]
    bottom_up = height > 0
    height = abs(height)
    row_bytes = ((width * depth + 31) // 32) * 4

    def get_pixel(x, y):
        row = (height - 1 - y) if bottom_up else y
        base = offset + row * row_bytes + x * (depth // 8)
        if depth == 8:
            index = data[base]
            b, g, r = palette[index * 4:index * 4 + 3]
            return r, g, b, 255
        b, g, r = data[base], data[base + 1], data[base + 2]
        a = data[base + 3] if depth == 32 else 255
        return r, g, b, a

    return width, height, get_pixel


def read_image(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:8] == b'\x89PNG\r\n\x1a\n':
        return _read_png(data)
    if data[:2] == b'BM':
        return _read_bmp(data)
    raise ValueError('%s is neither a PNG nor a BMP' % path)


# ---------------------------------------------------------------------------- the .fnt descriptor


class Font(object):
    def __init__(self):
        self.line_height = 0
        self.base = 0
        self.page = ''
        self.chars = {}     # code -> dict(x, y, width, height, xoffset, yoffset, xadvance)


def _parse_fnt_text(text):
    font = Font()
    for line in text.splitlines():
        parts = line.split()
        if not parts:
            continue
        kind = parts[0]
        values = {}
        for token in re.findall(r'(\w+)=("[^"]*"|\S+)', line):
            key, value = token
            values[key] = value.strip('"')
        if kind == 'common':
            font.line_height = int(values['lineHeight'])
            font.base = int(values['base'])
            if int(values.get('pages', 1)) > 1:
                raise ValueError('the font has more than one page')
        elif kind == 'page':
            font.page = values['file']
        elif kind == 'char':
            font.chars[int(values['id'])] = {
                'x': int(values['x']), 'y': int(values['y']),
                'width': int(values['width']), 'height': int(values['height']),
                'xoffset': int(values['xoffset']), 'yoffset': int(values['yoffset']),
                'xadvance': int(values['xadvance']),
            }
    return font


def _parse_fnt_xml(text):
    font = Font()
    common = re.search(r'<common\s([^>]*)>', text)
    if not common:
        raise ValueError('the XML has no common element')

    def attrs(s):
        return dict(re.findall(r'(\w+)="([^"]*)"', s))

    values = attrs(common.group(1))
    font.line_height = int(values['lineHeight'])
    font.base = int(values['base'])
    if int(values.get('pages', 1)) > 1:
        raise ValueError('the font has more than one page')

    page = re.search(r'<page\s([^>]*)/?>', text)
    font.page = attrs(page.group(1))['file']

    for one in re.findall(r'<char\s([^>]*)/?>', text):
        values = attrs(one)
        font.chars[int(values['id'])] = {
            'x': int(values['x']), 'y': int(values['y']),
            'width': int(values['width']), 'height': int(values['height']),
            'xoffset': int(values['xoffset']), 'yoffset': int(values['yoffset']),
            'xadvance': int(values['xadvance']),
        }
    return font


def _parse_fnt_binary(data):
    font = Font()
    if data[:3] != b'BMF' or data[3] != 3:
        raise ValueError('the binary .fnt is not of version 3')

    pos = 4
    while pos < len(data):
        kind = data[pos]
        size = struct.unpack('<I', data[pos + 1:pos + 5])[0]
        body = data[pos + 5:pos + 5 + size]
        pos += 5 + size
        if kind == 2:       # common
            line_height, base, _, _, pages = struct.unpack('<HHHHH', body[:10])
            font.line_height = line_height
            font.base = base
            if pages > 1:
                raise ValueError('the font has more than one page')
        elif kind == 3:     # pages
            font.page = body.split(b'\0')[0].decode('utf-8')
        elif kind == 4:     # chars
            for i in range(0, size, 20):
                (code, x, y, width, height, xoffset, yoffset,
                 xadvance) = struct.unpack('<IHHHHhhh', body[i:i + 18])
                font.chars[code] = {
                    'x': x, 'y': y, 'width': width, 'height': height,
                    'xoffset': xoffset, 'yoffset': yoffset, 'xadvance': xadvance,
                }
    return font


def read_fnt(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:3] == b'BMF':
        return _parse_fnt_binary(data)
    text = data.decode('utf-8-sig', errors='replace')
    if text.lstrip().startswith('<'):
        return _parse_fnt_xml(text)
    return _parse_fnt_text(text)


# ---------------------------------------------------------------------------- conversion


def parse_chars(text):
    """Turns "0x20-0x7E,0xAC00" into a set of codes."""
    codes = set()
    for piece in text.split(','):
        piece = piece.strip()
        if not piece:
            continue
        if '-' in piece[1:]:
            low, high = piece.split('-', 1)
            codes.update(range(int(low, 0), int(high, 0) + 1))
        else:
            codes.add(int(piece, 0))
    return codes


def glyph_columns(info, get_pixel, page_width, page_height, threshold, height, width):
    """Cuts one glyph out of the page as width columns of (height + 7) / 8 bytes."""
    pages = (height + 7) // 8
    out = bytearray()
    for col in range(width):
        column = bytearray(pages)
        for row in range(min(height, info['height'])):
            if col >= info['width']:
                break
            x = info['x'] + col
            y = info['y'] + row
            if x >= page_width or y >= page_height:
                continue
            r, g, b, a = get_pixel(x, y)
            value = a if a != 255 else (r * 299 + g * 587 + b * 114) // 1000
            if value >= threshold:
                column[row >> 3] |= 1 << (row & 7)
        out += column
    return bytes(out)


def build_ranges(codes):
    """Groups the codes into runs that follow one another."""
    ranges = []
    first = previous = None
    for code in codes:
        if first is None:
            first = previous = code
        elif code == previous + 1:
            previous = code
        else:
            ranges.append((first, previous))
            first = previous = code
    if first is not None:
        ranges.append((first, previous))
    return ranges


def find_cell(font, codes):
    """Returns (width, height, advance) when every glyph takes the same cell, otherwise None.

    A glyph of no size, which is what BMFont gives a space, counts in as an empty cell of that size, so
    that a font of a fixed width stays one even with a space in it.
    """
    shapes = set()
    advances = set()
    for code in codes:
        info = font.chars[code]
        advances.add(info['xadvance'])
        if info['width'] and info['height']:
            shapes.add((info['width'], info['height'], info['xoffset'], info['yoffset']))
    if len(shapes) != 1 or len(advances) != 1:
        return None

    width, height, x_offset, y_offset = shapes.pop()
    if x_offset or y_offset:
        return None
    return width, height, advances.pop()


def emit(font, name, codes, get_pixel, page_width, page_height, threshold, out):
    ranges = build_ranges(codes)
    cell = find_cell(font, codes)

    glyphs = []
    data = bytearray()
    for code in codes:
        info = font.chars[code]
        if cell:
            # Every glyph is cut to the one cell, so an empty one takes its share of zeroes and the
            # place of a glyph follows from its index alone.
            columns = glyph_columns(info, get_pixel, page_width, page_height, threshold, cell[1], cell[0])
            height = cell[1]
        else:
            columns = glyph_columns(info, get_pixel, page_width, page_height, threshold, info['height'],
                                    info['width'])
            height = info['height']
        glyphs.append((code, len(data), info, height, columns))
        data += columns

    metrics = 0 if cell else len(codes) * 12
    out.write('// SPDX-License-Identifier: BSD-3-Clause\n')
    out.write('// Made by tools/bmfont2pif.py from %s. Do not edit.\n' % os.path.basename(font.page))
    out.write('//\n')
    out.write('// %d glyphs in %d range(s), %d bytes of glyph data%s.\n'
              % (len(codes), len(ranges), len(data),
                 '' if cell else ', about %d bytes of metrics' % metrics))
    out.write('#include "display/pif_graphics.h"\n\n\n')

    out.write('static const uint8_t %s_data[] = {\n' % name)
    for code, offset, info, height, columns in glyphs:
        if not columns:
            out.write('\t\t\t\t\t\t\t\t// 0x%04X has no glyph of its own\n' % code)
            continue
        line = ', '.join('0x%02X' % b for b in columns)
        out.write('\t%s,\t// 0x%04X\n' % (line, code))
    out.write('};\n\n')

    if not cell:
        out.write('static const PifFontGlyphInfo %s_glyphs[] = {\n' % name)
        out.write('\t// offset, width, height, x_offset, y_offset, advance\n')
        for code, offset, info, height, columns in glyphs:
            out.write('\t{ %6d, %3d, %3d, %4d, %4d, %3d },\t// 0x%04X\n'
                      % (offset, info['width'], height, info['xoffset'], info['yoffset'],
                         info['xadvance'], code))
        out.write('};\n\n')

    out.write('static const PifFontRange %s_ranges[] = {\n' % name)
    index = 0
    for first, last in ranges:
        out.write('\t{ 0x%04X, 0x%04X, %d },\n' % (first, last, index))
        index += last - first + 1
    out.write('};\n\n')

    out.write('const PifFont %s = {\n' % name)
    out.write('\t%d,\t// line_height\n' % font.line_height)
    out.write('\t%d,\t// base\n' % font.base)
    out.write('\t%d,\t// width\n' % (cell[0] if cell else 0))
    out.write('\t%d,\t// height\n' % (cell[1] if cell else 0))
    out.write('\t%d,\t// advance\n' % (cell[2] if cell else 0))
    out.write('\t%d,\t// range_count\n' % len(ranges))
    out.write('\t%s_ranges,\n' % name)
    out.write('\t%s_data,\n' % name)
    if cell:
        out.write('\tNULL,\t// p_glyphs: every glyph takes the same cell\n')
    else:
        out.write('\t%s_glyphs,\n' % name)
    out.write('\tNULL\t// fn_get_glyph\n')
    out.write('};\n')


def main():
    parser = argparse.ArgumentParser(description='Turns a BMFont export into a PifFont.')
    parser.add_argument('fnt', help='the .fnt of the font, in the text, XML or binary format')
    parser.add_argument('-n', '--name', default='app_font', help='name of the PifFont in the output')
    parser.add_argument('-o', '--output', help='file to write, or standard output')
    parser.add_argument('-t', '--threshold', type=int, default=128,
                        help='how bright or opaque a pixel of the page has to be to count as set (1~255)')
    parser.add_argument('-c', '--chars', help='the codes to take, as 0x20-0x7E,0xAC00-0xD7A3')
    parser.add_argument('-p', '--page', help='the page image, when it is not where the .fnt says')
    args = parser.parse_args()

    font = read_fnt(args.fnt)
    if not font.chars:
        sys.exit('%s holds no characters' % args.fnt)

    page = args.page or os.path.join(os.path.dirname(args.fnt), font.page)
    page_width, page_height, get_pixel = read_image(page)

    codes = sorted(font.chars)
    if args.chars:
        wanted = parse_chars(args.chars)
        missing = sorted(wanted - set(codes))
        if missing:
            print('%s holds none of %d of the codes asked for, the first being 0x%04X'
                  % (args.fnt, len(missing), missing[0]), file=sys.stderr)
        codes = [c for c in codes if c in wanted]
    if not codes:
        sys.exit('nothing to write')

    out = open(args.output, 'w', newline='\n') if args.output else sys.stdout
    try:
        emit(font, args.name, codes, get_pixel, page_width, page_height, args.threshold, out)
    finally:
        if args.output:
            out.close()
    if args.output:
        print('%s: %d glyphs of %s' % (args.output, len(codes), args.name), file=sys.stderr)


if __name__ == '__main__':
    main()
