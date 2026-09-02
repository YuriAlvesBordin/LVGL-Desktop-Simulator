#!/usr/bin/env python3
"""Convert normal PC fonts (TTF/OTF/WOFF) into LVGL 9 font C sources.

Pure-Python replacement for the Node.js based ``lv_font_conv`` tool: the
glyphs are rasterised with freetype-py (FreeType bindings) and emitted as C
files in the ``lv_font_fmt_txt`` format that LVGL reads natively - the same
format used by the built-in ``lv_font_montserrat_*`` files.

Usage (normally driven by cmake/Fonts.cmake, not by hand):

    python3 scripts/convert_fonts.py --plan
    python3 scripts/convert_fonts.py --convert
    python3 scripts/convert_fonts.py --verify

Fonts are configured through ``assets/fonts/fonts.toml`` (optional). The
output goes to ``src/app/fonts/``: one ``lv_font_<name>_<size>.c`` per font
plus a generated ``uiFontUser.h``/``uiFontUser.c`` pair that declares every
font and exposes a small name -> font registry.
"""

import argparse
import math
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SOURCE_DIR = REPO_ROOT / "assets" / "fonts"
DEFAULT_OUT_DIR = REPO_ROOT / "src" / "app" / "fonts"

VERIFY_FONT = REPO_ROOT / "external" / "lvgl" / "scripts" / "built_in_font" / "Montserrat-Medium.ttf"
VERIFY_REFERENCE = REPO_ROOT / "external" / "lvgl" / "src" / "font" / "lv_font_montserrat_16.c"

DEFAULT_SIZE = 16
DEFAULT_BPP = 4
DEFAULT_RANGES = ["0x20-0x7F", "0xA0-0xFF"]

VALID_EXTENSIONS = (".ttf", ".otf", ".woff")
CONFIG_FILE_NAME = "fonts.toml"

# LV_FONT_FMT_TXT_LARGE is 0, so bitmap_index is a 20 bit field: 1 MB of bitmaps.
MAX_BITMAP_BYTES = 1 << 20
# lv_font_fmt_txt_glyph_dsc_t field limits (large format disabled).
MAX_ADV_W = 1 << 12
MAX_BOX = 1 << 8
MIN_OFS = -(1 << 7)
MAX_OFS = (1 << 7) - 1

# Font symbols already owned by LVGL built-ins; user fonts must not shadow them.
BUILTIN_FONT_SYMBOLS = set()
for _size in (8, 10, 12, 14, 16, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48):
    BUILTIN_FONT_SYMBOLS.add(f"lv_font_montserrat_{_size}")
BUILTIN_FONT_SYMBOLS.update((
    "lv_font_unscii_8", "lv_font_unscii_16", "lv_font_unscii_8_format1",
    "lv_font_simsun_16_cjk", "lv_font_source_han_sans_sc_16_1bit",
    "lv_font_source_han_sans_sc_16_2bit", "lv_font_source_han_sans_sc_16_4bit",
    "lv_font_dejavu_16_persian_hebrew",
))


def log(message):
    print(f"[convert-fonts] {message}")


def warn(message):
    print(f"[convert-fonts] warning: {message}", file=sys.stderr)


def fail(message):
    print(f"[convert-fonts] error: {message}", file=sys.stderr)
    raise SystemExit(1)


def half_up(value):
    """Round to nearest, half away from zero (FreeType/lv_font_conv style)."""
    if value >= 0:
        return int(math.floor(value + 0.5))
    return -int(math.floor(-value + 0.5))


# ---------------------------------------------------------------------------
# fonts.toml handling
# ---------------------------------------------------------------------------

def load_config(source_dir):
    """Read fonts.toml if present. Returns {"defaults": {...}, "files": {...}}."""
    config_path = source_dir / CONFIG_FILE_NAME
    if not config_path.exists():
        return {"defaults": {}, "files": {}}
    try:
        import tomllib
    except ImportError:
        fail(f"{config_path} requires Python 3.11+ (tomllib) to parse. "
             f"Either upgrade Python or delete the file to use the built-in defaults.")
    with open(config_path, "rb") as handle:
        data = tomllib.load(handle)
    defaults = data.get("defaults", {})
    files = data.get("files", {})
    if not isinstance(defaults, dict) or not isinstance(files, dict):
        fail(f"{config_path}: [defaults] and [files] must be tables")
    return {"defaults": defaults, "files": files}


def parse_codepoints(specs, context):
    """Parse range/codepoint/character specs into a sorted codepoint list."""
    codepoints = set()
    for spec in specs:
        text = spec.strip()
        if not text:
            continue
        range_match = re.fullmatch(r"(0[xX][0-9A-Fa-f]+|\d+)\s*-\s*(0[xX][0-9A-Fa-f]+|\d+)", text)
        single_match = re.fullmatch(r"(0[xX][0-9A-Fa-f]+|\d+)", text)
        if range_match:
            start = int(range_match.group(1), 0)
            end = int(range_match.group(2), 0)
            if not (0 <= start <= end <= 0x10FFFF):
                fail(f"{context}: invalid range '{spec}'")
            codepoints.update(range(start, end + 1))
        elif single_match:
            codepoint = int(text, 0)
            if not (0 <= codepoint <= 0x10FFFF):
                fail(f"{context}: codepoint {text} out of range")
            codepoints.add(codepoint)
        else:
            for char in text:
                codepoints.add(ord(char))
    if not codepoints:
        fail(f"{context}: no codepoints to include")
    return sorted(codepoints)


def describe_ranges(codepoints):
    """Compact description of a codepoint list for file headers and logs."""
    parts = []
    start = previous = None
    for codepoint in codepoints:
        if start is None:
            start = previous = codepoint
        elif codepoint == previous + 1:
            previous = codepoint
        else:
            parts.append(f"0x{start:X}" if start == previous else f"0x{start:X}-0x{previous:X}")
            start = previous = codepoint
    parts.append(f"0x{start:X}" if start == previous else f"0x{start:X}-0x{previous:X}")
    return ",".join(parts)


# ---------------------------------------------------------------------------
# Conversion plan
# ---------------------------------------------------------------------------

@dataclass
class FontJob:
    source: Path
    size: int
    bpp: int
    codepoints: list
    symbol: str
    ranges_description: str


def sanitize_symbol_part(name):
    """Turn a file base name into a valid, lowercase C identifier part."""
    part = re.sub(r"[^0-9A-Za-z]+", "_", name).strip("_").lower()
    if not part:
        part = "font"
    if part[0].isdigit():
        part = "font_" + part
    return part


def build_jobs(source_dir):
    """Resolve fonts.toml + the source folder into the list of FontJob."""
    config = load_config(source_dir)
    defaults = config["defaults"]
    per_file = config["files"]

    for key in per_file:
        if not (source_dir / key).exists():
            warn(f"{CONFIG_FILE_NAME}: [files.\"{key}\"] does not match any file in {source_dir}")

    default_size = defaults.get("size", DEFAULT_SIZE)
    default_bpp = defaults.get("bpp", DEFAULT_BPP)
    default_ranges = defaults.get("ranges", DEFAULT_RANGES)
    if not isinstance(default_bpp, int) or default_bpp not in (1, 2, 4, 8):
        fail(f"{CONFIG_FILE_NAME}: [defaults] bpp must be 1, 2, 4 or 8")

    jobs = []
    symbols = {}
    for source in sorted(source_dir.iterdir()):
        if not source.is_file() or source.suffix.lower() not in VALID_EXTENSIONS:
            continue
        file_settings = per_file.get(source.name, {})
        settings = {**defaults, **file_settings}
        if "sizes" in file_settings and "size" in file_settings:
            fail(f"{source.name}: use either 'size' or 'sizes', not both")
        sizes = file_settings.get("sizes")
        if sizes is None:
            sizes = [settings.get("size", default_size)]
        if not sizes:
            fail(f"{source.name}: 'sizes' must not be empty")
        for size in sizes:
            if not isinstance(size, int) or not (4 <= size <= 128):
                fail(f"{source.name}: font size {size!r} must be an integer between 4 and 128")
        bpp = settings.get("bpp", default_bpp)
        if not isinstance(bpp, int) or bpp not in (1, 2, 4, 8):
            fail(f"{source.name}: bpp must be 1, 2, 4 or 8")
        ranges = settings.get("ranges", default_ranges)
        if isinstance(ranges, str):
            ranges = [ranges]
        context = source.name
        codepoints = set(parse_codepoints(ranges, context))
        symbols_extra = settings.get("symbols", "")
        if isinstance(symbols_extra, str) and symbols_extra:
            codepoints.update(parse_codepoints(list(symbols_extra), context))
        codepoints = sorted(codepoints)

        base_part = sanitize_symbol_part(source.stem)
        for size in sizes:
            symbol = f"lv_font_{base_part}_{size}"
            if symbol in BUILTIN_FONT_SYMBOLS:
                fail(f"'{source.name}' would generate '{symbol}', which is an LVGL built-in font; "
                     f"rename the file")
            if symbol in symbols:
                fail(f"'{source.name}' collides with '{symbols[symbol]}' "
                     f"(both generate '{symbol}')")
            symbols[symbol] = source.name
            jobs.append(FontJob(source=source, size=size, bpp=bpp, codepoints=codepoints,
                                symbol=symbol, ranges_description=describe_ranges(codepoints)))

    jobs.sort(key=lambda job: job.symbol)
    return jobs


# ---------------------------------------------------------------------------
# Rasterisation (freetype-py)
# ---------------------------------------------------------------------------

@dataclass
class Glyph:
    codepoint: int
    bitmap_index: int
    adv_w: int
    box_w: int
    box_h: int
    ofs_x: int
    ofs_y: int
    data: bytes


@dataclass
class ConvertedFont:
    symbol: str
    bpp: int
    size: int
    line_height: int
    base_line: int
    cap_height: int
    x_height: int
    underline_position: int
    underline_thickness: int
    glyphs: list = field(default_factory=list)
    skipped: list = field(default_factory=list)


def load_flags_for(hinting):
    import freetype
    flags = freetype.FT_LOAD_RENDER
    if hinting == "none":
        flags |= freetype.FT_LOAD_NO_HINTING
    elif hinting == "auto":
        flags |= freetype.FT_LOAD_NO_AUTOHINT
    elif hinting == "light":
        target = getattr(freetype, "FT_LOAD_TARGET_LIGHT", None)
        if target is None:
            raise SystemExit(1)
        flags |= target
    return flags


def quantize(alpha, bpp):
    """Map an 8 bit alpha level onto the bpp grid used by the LVGL runtime."""
    if bpp == 8:
        return alpha
    max_level = (1 << bpp) - 1
    return min(max_level, (alpha * max_level + 127) // 255)


def pack_stream(levels, bpp):
    """Pack levels into bytes, MSB first, as one continuous stream.

    Matches the LVGL reader: two horizontally adjacent pixels share a byte
    (high nibble first at bpp 4) and rows are NOT byte aligned (stride 0).
    """
    accumulator = 0
    bits = 0
    output = bytearray()
    for level in levels:
        accumulator = (accumulator << bpp) | level
        bits += bpp
        if bits == 8:
            output.append(accumulator)
            accumulator = 0
            bits = 0
    if bits:
        output.append(accumulator << (8 - bits))
    return bytes(output)


def glyph_height(face, char):
    """Height of a glyph's bounding box in px, used for cap/x height."""
    import freetype
    if face.get_char_index(ord(char)) == 0:
        return 0
    face.load_char(char, flags=freetype.FT_LOAD_NO_BITMAP | freetype.FT_LOAD_NO_HINTING)
    return half_up(face.glyph.metrics.height / 64)


def convert_job(job, hinting="none"):
    """Rasterise one FontJob into a ConvertedFont."""
    import freetype

    face = freetype.Face(str(job.source))
    scalable_flag = getattr(freetype, "FT_FACE_FLAG_SCALABLE", 1)
    if not (getattr(face, "face_flags", 0) & scalable_flag):
        fail(f"{job.source.name}: font has no scalable outlines (bitmap font?)")
    if face.units_per_EM == 0:
        fail(f"{job.source.name}: font has no units per EM value")
    face.set_pixel_sizes(0, job.size)

    # freetype-py exposes the FT_Size_Metrics directly through face.size.
    # Line metrics are derived from the fractional hhea values (font units
    # scaled by size/upem) instead of FreeType's pixel-rounded metrics, so
    # line heights scale proportionally with the requested size.
    units_per_em = face.units_per_EM
    ascent = half_up(face.ascender / units_per_em * job.size)
    descent = half_up(-face.descender / units_per_em * job.size)

    underline_position = -max(1, half_up(job.size / 8))
    underline_thickness = max(1, half_up(job.size / 16))
    try:
        scale = job.size / face.units_per_EM
        raw_position = half_up(face.underline_position * scale)
        raw_thickness = half_up(face.underline_thickness * scale)
        if raw_thickness > 0:
            underline_position = max(MIN_OFS, min(MAX_OFS, raw_position))
            underline_thickness = max(MIN_OFS, min(MAX_OFS, raw_thickness))
    except AttributeError:
        pass

    converted = ConvertedFont(
        symbol=job.symbol,
        bpp=job.bpp,
        size=job.size,
        line_height=ascent + descent,
        base_line=descent,
        cap_height=glyph_height(face, "H"),
        x_height=glyph_height(face, "x"),
        underline_position=underline_position,
        underline_thickness=underline_thickness,
    )

    flags = load_flags_for(hinting)
    for codepoint in job.codepoints:
        if face.get_char_index(codepoint) == 0:
            converted.skipped.append(codepoint)
            continue

        # Advance widths come from the unhinted (linear) metrics so letter
        # spacing keeps the fractional widths designed into the font; hinted
        # advances would be grid-fitted to whole pixels.
        face.load_char(chr(codepoint), flags=freetype.FT_LOAD_RENDER | freetype.FT_LOAD_NO_HINTING)
        adv_w = half_up(face.glyph.metrics.horiAdvance / 4)

        if hinting != "none":
            # The bitmap itself comes from the hinted render (default: light,
            # vertical-only grid fitting, which matches the LVGL built-ins).
            face.load_char(chr(codepoint), flags=flags)
        slot = face.glyph
        bitmap = slot.bitmap
        if bitmap.rows and bitmap.width and bitmap.pixel_mode != freetype.FT_PIXEL_MODE_GRAY:
            fail(f"{job.source.name}: U+{codepoint:04X} rendered in unsupported "
                 f"pixel mode {bitmap.pixel_mode} (color font?)")

        box_w = bitmap.width
        box_h = bitmap.rows
        ofs_x = slot.bitmap_left
        # LVGL positions glyphs as y1 = baseline - box_h - ofs_y
        # (lv_draw_label.c), so ofs_y is the distance from the glyph's bitmap
        # bottom up to the baseline: ofs_y = bitmap_top - box_h.
        ofs_y = slot.bitmap_top - box_h

        if not (0 <= adv_w < MAX_ADV_W):
            fail(f"{job.source.name}: U+{codepoint:04X} advance width {adv_w} out of range")
        if box_w >= MAX_BOX or box_h >= MAX_BOX:
            fail(f"{job.source.name}: U+{codepoint:04X} box {box_w}x{box_h} exceeds 255 px; "
                 f"lower the size or split the font")
        if not (MIN_OFS <= ofs_x <= MAX_OFS) or not (MIN_OFS <= ofs_y <= MAX_OFS):
            fail(f"{job.source.name}: U+{codepoint:04X} offsets ({ofs_x}, {ofs_y}) do not fit "
                 f"int8_t; lower the size or split the font")

        levels = []
        if box_w and box_h:
            for alpha in bytes(bitmap.buffer):
                levels.append(quantize(alpha, job.bpp))
        converted.glyphs.append(Glyph(codepoint=codepoint, bitmap_index=0, adv_w=adv_w,
                                      box_w=box_w, box_h=box_h, ofs_x=ofs_x, ofs_y=ofs_y,
                                      data=pack_stream(levels, job.bpp)))

    if not converted.glyphs:
        fail(f"{job.source.name}: none of the requested codepoints exist in the font")

    offset = 0
    for glyph in converted.glyphs:
        glyph.bitmap_index = offset
        offset += len(glyph.data)
        if offset >= MAX_BITMAP_BYTES:
            fail(f"{job.source.name}: bitmaps exceed the 1 MB lv_font_fmt_txt limit; "
                 f"reduce the size, bpp or the number of codepoints")
    return converted


# ---------------------------------------------------------------------------
# cmap construction
# ---------------------------------------------------------------------------

def build_cmaps(glyphs):
    """Split the glyphs into LVGL cmaps.

    Contiguous runs (2+ codepoints) become FORMAT0_TINY tables; isolated
    codepoints are collected into a single SPARSE_TINY table. Glyph ids are
    assigned in codepoint order starting at 1 (id 0 is the reserved entry).
    """
    runs = []
    run_start = None
    run_end = None
    for glyph in glyphs:
        if run_start is None:
            run_start = run_end = glyph.codepoint
        elif glyph.codepoint == run_end + 1:
            run_end = glyph.codepoint
        else:
            runs.append((run_start, run_end))
            run_start = run_end = glyph.codepoint
    if run_start is not None:
        runs.append((run_start, run_end))

    glyph_id = {glyph.codepoint: index + 1 for index, glyph in enumerate(glyphs)}
    cmaps = []
    sparse_codepoints = []
    for start, end in runs:
        if end > start:
            cmaps.append({
                "range_start": start,
                "range_length": end - start + 1,
                "glyph_id_start": glyph_id[start],
                "type": "LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY",
                "unicode_list": None,
            })
        else:
            sparse_codepoints.append(start)
    if sparse_codepoints:
        base = sparse_codepoints[0]
        cmaps.append({
            "range_start": base,
            "range_length": sparse_codepoints[-1] - base + 1,
            "glyph_id_start": glyph_id[base],
            "type": "LV_FONT_FMT_TXT_CMAP_SPARSE_TINY",
            "unicode_list": [codepoint - base for codepoint in sparse_codepoints],
        })

    cmaps.sort(key=lambda cmap: cmap["range_start"])
    list_counter = 0
    for cmap in cmaps:
        if cmap["unicode_list"] is not None:
            list_counter += 1
            cmap["list_name"] = f"unicode_list_{list_counter}"
    return cmaps


# ---------------------------------------------------------------------------
# C emission
# ---------------------------------------------------------------------------

def glyph_comment(codepoint):
    char = chr(codepoint)
    if codepoint in (0x22, 0x5C):
        char = "\\" + char
    if 0x20 <= codepoint <= 0x7E or codepoint >= 0xA0:
        quoted = f' "{char}"'
    else:
        quoted = ""
    label = f"U+{codepoint:04X}" if codepoint <= 0xFFFF else f"U+{codepoint:05X}"
    return f"/* {label}{quoted} */"


def emit_font_c(job, font):
    cmaps = build_cmaps(font.glyphs)
    out = []
    out.append("/*****************************************************************************\n")
    out.append(f" * Size: {font.size} px\n")
    out.append(f" * Bpp: {font.bpp}\n")
    out.append(f" * Opts: generated by scripts/convert_fonts.py from {job.source.name}\n")
    out.append(f" *       ranges: {job.ranges_description}\n")
    out.append(" *****************************************************************************/\n")
    out.append("\n")
    out.append("#ifdef __has_include\n")
    out.append("    #if __has_include(\"lvgl.h\")\n")
    out.append("        #ifndef LV_LVGL_H_INCLUDE_SIMPLE\n")
    out.append("            #define LV_LVGL_H_INCLUDE_SIMPLE\n")
    out.append("        #endif\n")
    out.append("    #endif\n")
    out.append("#endif\n")
    out.append("\n")
    out.append("#ifdef LV_LVGL_H_INCLUDE_SIMPLE\n")
    out.append("    #include \"lvgl.h\"\n")
    out.append("#else\n")
    out.append("    #include \"../../lvgl.h\"\n")
    out.append("#endif\n")
    out.append("\n\n")
    out.append("/*-----------------\n")
    out.append(" *    BITMAPS\n")
    out.append(" *----------------*/\n")
    out.append("\n")
    out.append("/*Store the image of the glyphs*/\n")
    out.append("static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {\n")

    for glyph in font.glyphs:
        out.append(f"    {glyph_comment(glyph.codepoint)}\n")
        if glyph.data:
            for index in range(0, len(glyph.data), 8):
                chunk = ", ".join(f"0x{byte:02x}" for byte in glyph.data[index:index + 8])
                out.append(f"    {chunk},\n")
        else:
            out.append("\n")
    out.append("};\n")
    out.append("\n")
    out.append("/*---------------------\n")
    out.append(" *  GLYPH DESCRIPTION\n")
    out.append(" *--------------------*/\n")
    out.append("\n")
    out.append("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {\n")
    out.append("    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,\n")
    for glyph in font.glyphs:
        out.append(f"    {{.bitmap_index = {glyph.bitmap_index}, .adv_w = {glyph.adv_w}, "
                   f".box_w = {glyph.box_w}, .box_h = {glyph.box_h}, "
                   f".ofs_x = {glyph.ofs_x}, .ofs_y = {glyph.ofs_y}}},\n")
    out.append("};\n")
    out.append("\n")
    out.append("/*---------------------\n")
    out.append(" *  CHARACTER MAPPING\n")
    out.append(" *--------------------*/\n")
    out.append("\n")
    for cmap in cmaps:
        if cmap["unicode_list"] is None:
            continue
        out.append(f"static const uint16_t {cmap['list_name']}[] = {{\n")
        values = cmap["unicode_list"]
        for index in range(0, len(values), 8):
            chunk = ", ".join(f"0x{value:04x}" for value in values[index:index + 8])
            comma = "," if index + 8 < len(values) else ""
            out.append(f"    {chunk}{comma}\n")
        out.append("};\n")
        out.append("\n")
    out.append("/*Collect the unicode lists and glyph_id offsets*/\n")
    out.append("static const lv_font_fmt_txt_cmap_t cmaps[] = {\n")
    for cmap in cmaps:
        has_list = cmap["unicode_list"] is not None
        list_field = cmap["list_name"] if has_list else "NULL"
        length = len(cmap["unicode_list"]) if has_list else 0
        out.append("    {\n")
        out.append(f"        .range_start = {cmap['range_start']}, .range_length = {cmap['range_length']}, "
                   f".glyph_id_start = {cmap['glyph_id_start']},\n")
        out.append(f"        .unicode_list = {list_field}, .glyph_id_ofs_list = NULL, "
                   f".list_length = {length}, .type = {cmap['type']}\n")
        out.append("    },\n")
    out.append("};\n")
    out.append("\n")
    out.append("/*--------------------\n")
    out.append(" *  ALL CUSTOM DATA\n")
    out.append(" *--------------------*/\n")
    out.append("\n")
    out.append("#if LVGL_VERSION_MAJOR >= 8\n")
    out.append("static const lv_font_fmt_txt_dsc_t font_dsc = {\n")
    out.append("#else\n")
    out.append("static lv_font_fmt_txt_dsc_t font_dsc = {\n")
    out.append("#endif\n")
    out.append("    .glyph_bitmap = glyph_bitmap,\n")
    out.append("    .glyph_dsc = glyph_dsc,\n")
    out.append("    .cmaps = cmaps,\n")
    out.append("    .kern_dsc = NULL,\n")
    out.append("    .kern_scale = 16,\n")
    out.append(f"    .cmap_num = {len(cmaps)},\n")
    out.append(f"    .bpp = {font.bpp},\n")
    out.append("    .kern_classes = 0,\n")
    out.append("    .bitmap_format = 0,\n")
    out.append("\n")
    out.append("};\n")
    out.append("\n\n")
    out.append("/*-----------------\n")
    out.append(" *  PUBLIC FONT\n")
    out.append(" *----------------*/\n")
    out.append("\n")
    out.append("/*Initialize a public general font descriptor*/\n")
    out.append("#if LVGL_VERSION_MAJOR >= 8\n")
    out.append(f"const lv_font_t {font.symbol} = {{\n")
    out.append("#else\n")
    out.append(f"lv_font_t {font.symbol} = {{\n")
    out.append("#endif\n")
    out.append("    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/\n")
    out.append("    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/\n")
    out.append(f"    .line_height = {font.line_height},          /*The maximum line height required by the font*/\n")
    out.append(f"    .base_line = {font.base_line},             /*Baseline measured from the bottom of the line*/\n")
    out.append("#if LV_VERSION_CHECK(9, 6, 0) || LVGL_VERSION_MAJOR >= 10\n")
    out.append(f"    .cap_height = {font.cap_height},           /*Cap height of the font*/\n")
    out.append(f"    .x_height = {font.x_height},               /*x-height of the font*/\n")
    out.append("#endif\n")
    out.append("#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)\n")
    out.append("    .subpx = LV_FONT_SUBPX_NONE,\n")
    out.append("#endif\n")
    out.append("#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8\n")
    out.append(f"    .underline_position = {font.underline_position},\n")
    out.append(f"    .underline_thickness = {font.underline_thickness},\n")
    out.append("#endif\n")
    out.append("\n")
    out.append("#if LV_VERSION_CHECK(9, 3, 0)\n")
    out.append("    .static_bitmap = 1,    /*Bitmaps are stored as const so they are always static if not compressed */\n")
    out.append("#endif\n")
    out.append("\n")
    out.append("    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */\n")
    out.append("#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9\n")
    out.append("    .fallback = NULL,\n")
    out.append("#endif\n")
    out.append("    .user_data = NULL,\n")
    out.append("};\n")
    return "".join(out)


REGISTRY_HEADER_TEMPLATE = """\
/**
 * @file uiFontUser.h
 *
 * @brief Fonts converted from assets/fonts by scripts/convert_fonts.py.
 *
 * GENERATED FILE - do not edit by hand. Drop fonts into assets/fonts/ and
 * rebuild (or build the "fonts" target) to regenerate it.
 */

#ifndef UI_FONT_USER_H
#define UI_FONT_USER_H

#ifdef __cplusplus
extern "C" {{
#endif

#include <stddef.h>

#include <lvgl.h>

{declares}/**
 * @brief One entry of the user font registry.
 */
typedef struct {{
    const char * name;               /**< Short name, e.g. "montserrat_medium_16" */
    const lv_font_t * font;          /**< The LVGL font */
}} ui_font_user_entry_t;

/**
 * @brief All user fonts, sorted by name.
 */
extern const ui_font_user_entry_t ui_font_user_registry[];
extern const size_t ui_font_user_registry_count;

/**
 * @brief Look up a user font by its short name.
 * @param name short name, e.g. "montserrat_medium_16"
 * @return the font, or NULL when there is no such font
 */
const lv_font_t * ui_font_user_find(const char * name);

#ifdef __cplusplus
}}
#endif

#endif /* UI_FONT_USER_H */
"""

REGISTRY_SOURCE_TEMPLATE = """\
/**
 * @file uiFontUser.c
 *
 * @brief Registry of the fonts converted from assets/fonts.
 *
 * GENERATED FILE - do not edit by hand. Drop fonts into assets/fonts/ and
 * rebuild (or build the "fonts" target) to regenerate it.
 */

#include "uiFontUser.h"

#include <string.h>

/*All user fonts, sorted by name*/
const ui_font_user_entry_t ui_font_user_registry[] = {{
{entries}}};

const size_t ui_font_user_registry_count = sizeof(ui_font_user_registry) / sizeof(ui_font_user_registry[0]);

const lv_font_t * ui_font_user_find(const char * name)
{{
    if(name == NULL) {{
        return NULL;
    }}

    for(size_t i = 0; i < ui_font_user_registry_count; i++) {{
        if(strcmp(name, ui_font_user_registry[i].name) == 0) {{
            return ui_font_user_registry[i].font;
        }}
    }}

    return NULL;
}}
"""


def short_name(symbol):
    return symbol[len("lv_font_"):]


def emit_registry(jobs):
    jobs_sorted = sorted(jobs, key=lambda job: short_name(job.symbol))
    declares = "".join(f"LV_FONT_DECLARE({job.symbol});\n\n" for job in jobs_sorted)
    entries = "".join(f'    {{ "{short_name(job.symbol)}", &{job.symbol} }},\n' for job in jobs_sorted)
    return REGISTRY_HEADER_TEMPLATE.format(declares=declares), REGISTRY_SOURCE_TEMPLATE.format(entries=entries)


# ---------------------------------------------------------------------------
# Commands: plan / convert / verify
# ---------------------------------------------------------------------------

def output_paths(out_dir, jobs):
    paths = [out_dir / f"{job.symbol}.c" for job in jobs]
    if jobs:
        paths.append(out_dir / "uiFontUser.c")
        paths.append(out_dir / "uiFontUser.h")
    return paths


def command_plan(args):
    for path in output_paths(args.out_dir, build_jobs(args.source_dir)):
        print(path.name)
    return 0


def command_convert(args):
    try:
        import freetype
    except ImportError:
        fail("freetype-py is required: install it with 'pip3 install freetype-py'")

    jobs = build_jobs(args.source_dir)
    if not jobs:
        fail(f"no {', '.join(VALID_EXTENSIONS)} fonts found in {args.source_dir}")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    for job in jobs:
        font = convert_job(job, hinting=args.hinting)
        output_path = args.out_dir / f"{job.symbol}.c"
        output_path.write_text(emit_font_c(job, font), encoding="utf-8")
        bitmap_bytes = sum(len(glyph.data) for glyph in font.glyphs)
        skipped = (f", {len(font.skipped)} requested codepoint(s) missing from the font"
                   if font.skipped else "")
        log(f"wrote {output_path}: {len(font.glyphs)} glyphs, {bitmap_bytes} bitmap bytes{skipped}")

    header, source = emit_registry(jobs)
    (args.out_dir / "uiFontUser.h").write_text(header, encoding="utf-8")
    (args.out_dir / "uiFontUser.c").write_text(source, encoding="utf-8")
    log(f"wrote {args.out_dir / 'uiFontUser.h'} and {args.out_dir / 'uiFontUser.c'} "
        f"({len(jobs)} fonts registered)")

    planned = {path.name for path in output_paths(args.out_dir, jobs)}
    for existing in sorted(args.out_dir.glob("lv_font_*.c")):
        if existing.name not in planned:
            existing.unlink()
            log(f"removed stale {existing}")
    return 0


# ---------------------------------------------------------------------------
# verify: compare against the shipped lv_font_montserrat_16.c reference
# ---------------------------------------------------------------------------

def parse_reference(path):
    text = path.read_text(encoding="utf-8")
    dsc_match = re.search(r"static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc\[\] = \{(.*?)\n\};", text, re.S)
    bitmap_match = re.search(r"const uint8_t glyph_bitmap\[\] = \{(.*?)\n\};", text, re.S)
    line_match = re.search(r"\.line_height = (\d+)", text)
    base_match = re.search(r"\.base_line = (-?\d+)", text)
    if not (dsc_match and bitmap_match and line_match and base_match):
        fail(f"could not parse reference font {path}")
    entries = re.findall(
        r"\{\s*\.bitmap_index = (\d+),\s*\.adv_w = (\d+),\s*\.box_w = (\d+),\s*\.box_h = (\d+),"
        r"\s*\.ofs_x = (-?\d+),\s*\.ofs_y = (-?\d+)\s*\}", dsc_match.group(1))
    body = re.sub(r"/\*.*?\*/", "", bitmap_match.group(1), flags=re.S)
    return {
        "entries": [tuple(int(value) for value in entry) for entry in entries],
        "bitmap": [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", body)],
        "line_height": int(line_match.group(1)),
        "base_line": int(base_match.group(1)),
    }


def compare_with_reference(font, reference):
    """Compare converted ASCII glyphs against the reference font data.

    The reference (lv_font_montserrat_16.c) was produced by lv_font_conv from
    a possibly different Montserrat revision, so small per-glyph differences
    are expected; the comparison is a smoke test for systematic errors.
    """
    counts = {"adv": 0, "adv1": 0, "box": 0, "box1": 0, "top": 0, "top1": 0,
              "ofs_x": 0, "accuracy_nibbles": 0, "accuracy_diff": 0}
    bad = {"adv": [], "box": [], "top": []}

    for index, glyph in enumerate(font.glyphs):
        entry_id = index + 1  # glyph ids follow codepoint order, entry 0 is reserved
        if entry_id + 1 >= len(reference["entries"]):
            break
        bitmap_index, adv_w, box_w, box_h, ofs_x, ofs_y = reference["entries"][entry_id]
        next_index = reference["entries"][entry_id + 1][0]

        counts["adv"] += adv_w == glyph.adv_w
        counts["adv1"] += abs(adv_w - glyph.adv_w) <= 1
        counts["box"] += (box_w, box_h) == (glyph.box_w, glyph.box_h)
        counts["box1"] += abs(box_w - glyph.box_w) <= 1 and abs(box_h - glyph.box_h) <= 1
        counts["ofs_x"] += ofs_x == glyph.ofs_x

        # Glyph top above the baseline (lv_draw_label.c: baseline - box_h - ofs_y).
        top_reference = ofs_y + box_h
        top_ours = glyph.ofs_y + glyph.box_h
        counts["top"] += top_reference == top_ours
        counts["top1"] += abs(top_reference - top_ours) <= 1
        if abs(top_reference - top_ours) > 1:
            bad["top"].append(glyph.codepoint)

        if (box_w, box_h) == (glyph.box_w, glyph.box_h):
            reference_data = bytes(reference["bitmap"][bitmap_index:next_index])
            counts["accuracy_nibbles"] += max(1, glyph.box_w * glyph.box_h)
            for left, right in zip(reference_data, glyph.data):
                if left != right:
                    counts["accuracy_diff"] += (left >> 4) != (right >> 4)
                    counts["accuracy_diff"] += (left & 0xF) != (right & 0xF)
        elif abs(box_w - glyph.box_w) > 1 or abs(box_h - glyph.box_h) > 1:
            bad["box"].append(glyph.codepoint)
        if abs(adv_w - glyph.adv_w) > 1:
            bad["adv"].append(glyph.codepoint)

    return counts, bad


def command_verify(args):
    if not args.font.exists():
        fail(f"font not found: {args.font}")
    if not args.reference.exists():
        fail(f"reference not found: {args.reference}")

    reference = parse_reference(args.reference)
    log(f"verify: {args.font.name} @{args.size}px {args.bpp}bpp vs {args.reference.name}")
    log(f"reference metrics: line_height={reference['line_height']} base_line={reference['base_line']}")

    codepoints = list(range(0x20, 0x7F))
    glyph_count = len(codepoints)
    results = []
    for mode in ("light", "none", "normal", "auto"):
        job = FontJob(source=args.font, size=args.size, bpp=args.bpp,
                      codepoints=codepoints, symbol="verify", ranges_description="verify")
        try:
            font = convert_job(job, hinting=mode)
        except SystemExit:
            log(f"hinting={mode:6s} unavailable")
            continue
        counts, bad = compare_with_reference(font, reference)
        nibbles = counts["accuracy_nibbles"]
        accuracy = 1.0 - counts["accuracy_diff"] / nibbles if nibbles else 0.0
        log(f"hinting={mode:6s} advances {counts['adv']}/{glyph_count} (+{counts['adv1'] - counts['adv']} off-by-1), "
            f"boxes {counts['box']}/{glyph_count} (+{counts['box1'] - counts['box']} off-by-1), "
            f"ofs_x {counts['ofs_x']}/{glyph_count}, "
            f"top {counts['top']}/{glyph_count} (+{counts['top1'] - counts['top']} off-by-1), "
            f"bitmap accuracy {accuracy * 100:.3f}% (matching boxes)")
        score = (-counts["box"], -counts["adv"], -counts["top"], -counts["ofs_x"], -accuracy)
        results.append((score, mode, font, counts, bad, accuracy))

    if not results:
        fail("no hinting mode could be tested")
    _, mode, font, counts, bad, accuracy = min(results, key=lambda item: item[0])
    log(f"best hinting mode: {mode}; "
        f"ours: line_height={font.line_height} base_line={font.base_line} "
        f"cap_height={font.cap_height} x_height={font.x_height}")

    passed = (counts["adv"] >= 0.85 * glyph_count and counts["adv1"] == glyph_count
              and counts["box"] >= 0.90 * glyph_count and counts["box1"] == glyph_count
              and counts["top"] >= 0.85 * glyph_count and counts["top1"] == glyph_count
              and accuracy >= 0.80)
    if passed:
        log("VERIFY PASS")
        return 0
    log("VERIFY FAIL")
    for kind in ("adv", "box", "top"):
        if bad[kind]:
            shown = [f"U+{cp:04X}" for cp in bad[kind][:10]]
            log(f"{kind} mismatches beyond tolerance: {shown}")
    return 1


# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source-dir", type=Path, default=DEFAULT_SOURCE_DIR,
                        help=f"folder with user fonts (default: {DEFAULT_SOURCE_DIR})")
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR,
                        help=f"folder for generated C files (default: {DEFAULT_OUT_DIR})")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--plan", action="store_true",
                      help="print the names of all files that --convert would generate")
    mode.add_argument("--convert", action="store_true",
                      help="convert every font and rewrite the registry")
    mode.add_argument("--verify", action="store_true",
                      help="self-test against the shipped Montserrat reference font")
    parser.add_argument("--hinting", choices=("none", "normal", "auto", "light"), default="light",
                        help="with --convert: FreeType hinting mode (default: light)")
    parser.add_argument("--font", type=Path, default=VERIFY_FONT, help="with --verify: font to convert")
    parser.add_argument("--reference", type=Path, default=VERIFY_REFERENCE,
                        help="with --verify: reference C file")
    parser.add_argument("--size", type=int, default=16, help="with --verify: size in px")
    parser.add_argument("--bpp", type=int, default=4, choices=(1, 2, 4, 8), help="with --verify: bpp")
    args = parser.parse_args()

    if args.convert:
        return command_convert(args)
    if args.plan:
        return command_plan(args)
    return command_verify(args)


if __name__ == "__main__":
    sys.exit(main())
