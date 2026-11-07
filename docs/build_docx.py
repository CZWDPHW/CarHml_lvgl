"""Build a DOCX handbook from the Markdown source.

Usage:
    python build_docx.py <input.md> <output.docx>

Supports the subset of Markdown used by lvgl_two/docs/manual.md:
headings, fenced code, pipe tables, bullet lists, numbered lines,
bold **text** and inline `code`.
"""

import re
import sys

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

BODY_EA = "宋体"
HEAD_EA = "微软雅黑"
LATIN = "Calibri"
CODE_FONT = "Consolas"

BODY_SIZE = 10.5
CODE_SIZE = 8.5
TABLE_SIZE = 9.0
ACCENT = RGBColor(0x1F, 0x3B, 0x57)

INLINE = re.compile(r"(\*\*.+?\*\*|`[^`]+`)")


# --------------------------------------------------------------------------
# low level helpers
# --------------------------------------------------------------------------

def set_run_font(run, latin, east_asia, size=None, bold=None, color=None):
    run.font.name = latin
    rpr = run._element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.append(rfonts)
    rfonts.set(qn("w:ascii"), latin)
    rfonts.set(qn("w:hAnsi"), latin)
    rfonts.set(qn("w:eastAsia"), east_asia)
    if size is not None:
        run.font.size = Pt(size)
    if bold is not None:
        run.bold = bold
    if color is not None:
        run.font.color.rgb = color
    return run


def shade_run(run, fill):
    rpr = run._element.get_or_add_rPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), fill)
    rpr.append(shd)


def shade_paragraph(par, fill):
    ppr = par._element.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), fill)
    ppr.append(shd)


def style_east_asia(style, east_asia):
    rpr = style.element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.append(rfonts)
    rfonts.set(qn("w:eastAsia"), east_asia)


def add_page_field(paragraph):
    run = paragraph.add_run()
    begin = OxmlElement("w:fldChar")
    begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = "PAGE"
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    run._r.append(begin)
    run._r.append(instr)
    run._r.append(end)


# --------------------------------------------------------------------------
# inline text
# --------------------------------------------------------------------------

def add_inline(par, text, size=BODY_SIZE, color=None):
    pos = 0
    for match in INLINE.finditer(text):
        if match.start() > pos:
            run = par.add_run(text[pos:match.start()])
            set_run_font(run, LATIN, BODY_EA, size, color=color)
        token = match.group(0)
        if token.startswith("**"):
            run = par.add_run(token[2:-2])
            set_run_font(run, LATIN, BODY_EA, size, bold=True, color=color)
        else:
            run = par.add_run(token[1:-1])
            set_run_font(run, CODE_FONT, BODY_EA, size - 1.0, color=color)
            shade_run(run, "F0F2F5")
        pos = match.end()
    if pos < len(text):
        run = par.add_run(text[pos:])
        set_run_font(run, LATIN, BODY_EA, size, color=color)


def plain(text):
    return re.sub(r"`|\*\*", "", text)


# --------------------------------------------------------------------------
# block builders
# --------------------------------------------------------------------------

def add_code_block(doc, lines):
    if not lines:
        return

    par = doc.add_paragraph()
    fmt = par.paragraph_format
    fmt.space_before = Pt(4)
    fmt.space_after = Pt(8)
    fmt.left_indent = Cm(0.3)
    fmt.line_spacing = 1.0
    shade_paragraph(par, "F5F6F8")

    for index, line in enumerate(lines):
        if index:
            par.add_run().add_break()
        run = par.add_run(line if line.strip() else " ")
        set_run_font(run, CODE_FONT, BODY_EA, CODE_SIZE)


def add_table(doc, rows):
    if len(rows) < 2:
        return

    def split_row(row):
        return [c.strip() for c in row.strip().strip("|").split("|")]

    header = split_row(rows[0])
    body = []
    for row in rows[1:]:
        if re.fullmatch(r"[\|\-\: ]+", row):
            continue
        cells = split_row(row)
        if len(cells) < len(header):
            cells += [""] * (len(header) - len(cells))
        body.append(cells[: len(header)])

    size = TABLE_SIZE if len(header) <= 4 else 8.0

    table = doc.add_table(rows=1, cols=len(header))
    table.style = "Table Grid"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = True

    for index, text in enumerate(header):
        cell = table.rows[0].cells[index]
        par = cell.paragraphs[0]
        par.paragraph_format.space_after = Pt(1)
        add_inline(par, text, size, color=ACCENT)
        for run in par.runs:
            run.bold = True
        shade_paragraph(par, "E8EDF3")

    for cells in body:
        row = table.add_row()
        for index, text in enumerate(cells):
            par = row.cells[index].paragraphs[0]
            par.paragraph_format.space_after = Pt(1)
            add_inline(par, text, size)

    doc.add_paragraph().paragraph_format.space_after = Pt(2)


def smart_join(parts):
    out = parts[0]
    for part in parts[1:]:
        prev = out[-1] if out else ""
        nxt = part[:1]
        if prev.isascii() and nxt.isascii() and (prev.isalnum() and nxt.isalnum()):
            out += " " + part
        else:
            out += part
    return out


def is_block_start(line):
    stripped = line.strip()
    return (
        stripped.startswith("#")
        or stripped.startswith("```")
        or stripped.startswith("|")
        or stripped.startswith("- ")
        or stripped == "---"
        or re.match(r"^\d+\.\s", stripped) is not None
    )


# --------------------------------------------------------------------------
# document setup
# --------------------------------------------------------------------------

def setup_document():
    doc = Document()

    section = doc.sections[0]
    section.page_width = Cm(21.0)
    section.page_height = Cm(29.7)
    section.top_margin = Cm(2.2)
    section.bottom_margin = Cm(2.0)
    section.left_margin = Cm(2.2)
    section.right_margin = Cm(2.2)

    normal = doc.styles["Normal"]
    normal.font.name = LATIN
    normal.font.size = Pt(BODY_SIZE)
    style_east_asia(normal, BODY_EA)
    normal.paragraph_format.line_spacing = 1.35
    normal.paragraph_format.space_after = Pt(4)

    for name, size in (("Title", 26), ("Heading 1", 17),
                       ("Heading 2", 13.5), ("Heading 3", 11.5)):
        style = doc.styles[name]
        style.font.name = LATIN
        style.font.size = Pt(size)
        style.font.color.rgb = ACCENT
        style.font.bold = True
        style_east_asia(style, HEAD_EA)
        style.paragraph_format.space_before = Pt(14 if name != "Title" else 0)
        style.paragraph_format.space_after = Pt(6)

    doc.styles["List Bullet"].font.name = LATIN
    doc.styles["List Bullet"].font.size = Pt(BODY_SIZE)
    style_east_asia(doc.styles["List Bullet"], BODY_EA)

    header = section.header.paragraphs[0]
    header.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    run = header.add_run("lvgl_two · 设计与使用手册")
    set_run_font(run, LATIN, HEAD_EA, 8.5, color=RGBColor(0x80, 0x88, 0x94))

    footer = section.footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    add_page_field(footer)

    return doc


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

def build(source, target):
    with open(source, encoding="utf-8") as handle:
        lines = handle.read().splitlines()

    doc = setup_document()
    index = 0
    seen_title = False
    stats = {"tables": 0, "code": 0, "headings": 0}

    while index < len(lines):
        line = lines[index]
        stripped = line.strip()

        if stripped.startswith("```"):
            index += 1
            buf = []
            while index < len(lines) and not lines[index].strip().startswith("```"):
                buf.append(lines[index])
                index += 1
            index += 1
            add_code_block(doc, buf)
            stats["code"] += 1
            continue

        if stripped.startswith("|"):
            rows = []
            while index < len(lines) and lines[index].strip().startswith("|"):
                rows.append(lines[index])
                index += 1
            add_table(doc, rows)
            stats["tables"] += 1
            continue

        if stripped == "---" or stripped == "":
            index += 1
            continue

        if stripped.startswith("#### "):
            heading = doc.add_heading(plain(stripped[5:]), level=3)
            stats["headings"] += 1
        elif stripped.startswith("### "):
            heading = doc.add_heading(plain(stripped[4:]), level=3)
            stats["headings"] += 1
        elif stripped.startswith("## "):
            heading = doc.add_heading(plain(stripped[3:]), level=2)
            stats["headings"] += 1
        elif stripped.startswith("# "):
            level = 0 if not seen_title else 1
            seen_title = True
            heading = doc.add_heading(plain(stripped[2:]), level=level)
            stats["headings"] += 1
        else:
            heading = None

        if heading is not None:
            for run in heading.runs:
                set_run_font(run, LATIN, HEAD_EA, None, bold=True, color=ACCENT)
            index += 1
            continue

        if stripped.startswith("- "):
            par = doc.add_paragraph(style="List Bullet")
            par.paragraph_format.space_after = Pt(2)
            add_inline(par, stripped[2:])
            index += 1
            continue

        if re.match(r"^\d+\.\s", stripped):
            par = doc.add_paragraph()
            par.paragraph_format.left_indent = Cm(0.6)
            par.paragraph_format.space_after = Pt(2)
            add_inline(par, stripped)
            index += 1
            continue

        buf = [stripped]
        index += 1
        while (index < len(lines) and lines[index].strip()
               and not is_block_start(lines[index])):
            buf.append(lines[index].strip())
            index += 1

        par = doc.add_paragraph()
        add_inline(par, smart_join(buf))

    doc.save(target)
    return stats


if __name__ == "__main__":
    src = sys.argv[1] if len(sys.argv) > 1 else "manual.md"
    dst = sys.argv[2] if len(sys.argv) > 2 else "manual.docx"
    result = build(src, dst)
    print(f"saved {dst}: {result}")
