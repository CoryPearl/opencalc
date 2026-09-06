#!/usr/bin/env python3
"""Generate the OpenCalc OS UI tour GIF used by the project documentation."""

from __future__ import annotations

import math
from functools import lru_cache
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


LOGICAL_W = 320
LOGICAL_H = 240
SCALE = 2
WIDTH = LOGICAL_W * SCALE
HEIGHT = LOGICAL_H * SCALE
FPS = 10
FRAME_COUNT = 300

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "open_calc_ui_calculator_demo.gif"

FONT_REGULAR = Path("/System/Library/Fonts/Supplemental/Verdana.ttf")
FONT_BOLD = Path("/System/Library/Fonts/Supplemental/Verdana Bold.ttf")
FONT_MONO = Path("/System/Library/Fonts/Supplemental/Andale Mono.ttf")

BG = "#0b0d10"
SURFACE = "#171b21"
SURFACE_2 = "#242a32"
HEADER = "#30363f"
ACCENT = "#4aa3ff"
ACCENT_2 = "#2d4258"
TEXT = "#f4f7fb"
MUTED = "#9aa8b6"
BORDER = "#586575"
GRID = "#1b2027"
GREEN = "#33d17a"
YELLOW = "#f5c542"


APPS = [
    ("Calculator", "Calc", "calculator", "#4aa3ff"),
    ("Graph", "Graph", "graph", "#7ab8ff"),
    ("Table", "Table", "table", "#9aa8ff"),
    ("Python", "Python", "python", "#b0bfd0"),
    ("Statistics", "Stats", "stats", "#6f9ee8"),
    ("Lists", "Lists", "lists", "#8f9db4"),
    ("Matrices", "Matrix", "matrix", "#c1a6ff"),
    ("Solver", "Solver", "solver", "#5d8fd6"),
    ("Settings", "Settings", "settings", "#6d7d91"),
    ("Finance", "Finance", "finance", "#8aa4c4"),
    ("Conics", "Conics", "conics", "#d0d7e2"),
    ("Inequality", "Ineq", "inequality", "#a9b4c4"),
]


@lru_cache(maxsize=None)
def get_font(size: int, bold: bool = False, mono: bool = False) -> ImageFont.FreeTypeFont:
    path = FONT_MONO if mono else (FONT_BOLD if bold and FONT_BOLD.exists() else FONT_REGULAR)
    return ImageFont.truetype(str(path), size * SCALE)


class Canvas:
    def __init__(self) -> None:
        self.image = Image.new("RGB", (WIDTH, HEIGHT), BG)
        self.draw = ImageDraw.Draw(self.image)

    @staticmethod
    def p(value: float) -> int:
        return int(round(value * SCALE))

    def rect(self, xy: tuple[float, float, float, float], fill: str, outline: str | None = None,
             width: int = 1, radius: int = 0) -> None:
        box = tuple(self.p(v) for v in xy)
        if radius:
            self.draw.rounded_rectangle(box, radius=self.p(radius), fill=fill, outline=outline,
                                        width=self.p(width) if outline else 1)
        else:
            self.draw.rectangle(box, fill=fill, outline=outline,
                                width=self.p(width) if outline else 1)

    def line(self, xy: tuple[float, ...], fill: str, width: int = 1) -> None:
        self.draw.line(tuple(self.p(v) for v in xy), fill=fill, width=self.p(width), joint="curve")

    def ellipse(self, xy: tuple[float, float, float, float], fill: str | None = None,
                outline: str | None = None, width: int = 1) -> None:
        self.draw.ellipse(tuple(self.p(v) for v in xy), fill=fill, outline=outline,
                          width=self.p(width) if outline else 1)

    def polygon(self, xy: tuple[float, ...], fill: str, outline: str | None = None) -> None:
        self.draw.polygon(tuple(self.p(v) for v in xy), fill=fill, outline=outline)

    def text(self, xy: tuple[float, float], value: str, fill: str = TEXT, size: int = 8,
             bold: bool = False, mono: bool = False, anchor: str | None = None) -> None:
        self.draw.text((self.p(xy[0]), self.p(xy[1])), value, fill=fill,
                       font=get_font(size, bold, mono), anchor=anchor)

    def text_width(self, value: str, size: int = 8, bold: bool = False, mono: bool = False) -> float:
        box = self.draw.textbbox((0, 0), value, font=get_font(size, bold, mono))
        return (box[2] - box[0]) / SCALE


def draw_icon(c: Canvas, kind: str, x: float, y: float, size: float, color: str) -> None:
    cx = x + size / 2
    cy = y + size / 2
    pad = 3
    if kind == "calculator":
        arm = size / 2 - 3
        c.rect((cx - 1.2, cy - arm, cx + 1.2, cy + arm), color)
        c.rect((cx - arm, cy - 1.2, cx + arm, cy + 1.2), color)
    elif kind == "graph":
        c.line((x + 3, y + size - 4, x + size - 2, y + size - 4), color)
        c.line((x + 4, y + size - 2, x + 4, y + 2), color)
        points = []
        for i in range(12):
            px = x + 5 + i * (size - 10) / 11
            py = cy + math.sin(i / 11 * math.pi * 1.7) * (size * .24)
            points.extend((px, py))
        c.line(tuple(points), color, 1)
    elif kind in {"table", "matrix"}:
        c.rect((x + pad, y + pad, x + size - pad, y + size - pad), BG, color)
        c.line((cx, y + pad, cx, y + size - pad), color)
        c.line((x + pad, cy, x + size - pad, cy), color)
    elif kind == "python":
        c.line((x + 7, y + 4, x + 3, cy, x + 7, y + size - 4), color, 1)
        c.line((x + size - 7, y + 4, x + size - 3, cy, x + size - 7, y + size - 4), color, 1)
    elif kind == "stats":
        c.rect((x + 4, y + size - 9, x + 7, y + size - 2), color)
        c.rect((cx - 1.5, y + size - 14, cx + 1.5, y + size - 2), color)
        c.rect((x + size - 7, y + 5, x + size - 4, y + size - 2), color)
    elif kind == "lists":
        for row in range(2):
            yy = y + 6 + row * 7
            c.ellipse((x + 4, yy, x + 7, yy + 3), fill=color)
            c.line((x + 11, yy + 1.5, x + size - 4, yy + 1.5), color)
    elif kind == "solver":
        c.rect((x + 3, cy - 4, x + size - 3, cy - 2), color)
        c.rect((x + 3, cy + 3, x + size - 3, cy + 5), color)
    elif kind == "settings":
        c.ellipse((cx - 5, cy - 5, cx + 5, cy + 5), outline=color, width=2)
        c.ellipse((cx - 1.5, cy - 1.5, cx + 1.5, cy + 1.5), fill=color)
        for angle in range(0, 360, 45):
            dx = math.cos(math.radians(angle)) * 7
            dy = math.sin(math.radians(angle)) * 7
            c.line((cx + dx * .72, cy + dy * .72, cx + dx, cy + dy), color, 2)
    elif kind == "finance":
        c.text((cx, cy), "$", color, 12, bold=True, anchor="mm")
    elif kind == "conics":
        c.ellipse((x + 3, y + 3, x + size - 3, y + size - 3), outline=color, width=1)
    elif kind == "inequality":
        c.line((x + size - 4, y + 4, x + 5, cy, x + size - 4, y + size - 4), color, 1)


def draw_battery(c: Canvas, x: float = 288, y: float = 9) -> None:
    c.rect((x, y, x + 24, y + 10), HEADER, TEXT)
    c.rect((x + 24, y + 3, x + 27, y + 7), TEXT)
    for i in range(4):
        c.rect((x + 3 + i * 5, y + 3, x + 6 + i * 5, y + 7), GREEN)


def draw_header(c: Canvas, app_index: int) -> None:
    title, _, icon, color = APPS[app_index]
    c.rect((0, 0, 320, 28), HEADER)
    c.rect((0, 26, 320, 28), ACCENT)
    c.rect((7, 5, 25, 23), SURFACE_2, radius=2)
    draw_icon(c, icon, 8, 6, 16, color)
    c.text((160, 14), title, TEXT, 9, bold=True, anchor="mm")
    draw_battery(c)


def cursor_visible(frame: int) -> bool:
    return (frame // 5) % 2 == 0


def typed(value: str, progress: float) -> str:
    count = max(0, min(len(value), int(progress * (len(value) + 1))))
    return value[:count]


def draw_fraction(c: Canvas, x: float, y: float, numerator: str, denominator: str) -> None:
    width = max(c.text_width(numerator, 7), c.text_width(denominator, 7)) + 7
    c.text((x + width / 2, y - 5), numerator, TEXT, 7, mono=True, anchor="mm")
    c.line((x, y, x + width, y), TEXT)
    c.text((x + width / 2, y + 7), denominator, TEXT, 7, mono=True, anchor="mm")


def draw_calculator(frame: int) -> Image.Image:
    c = Canvas()
    draw_header(c, 0)
    history: list[tuple[str, str | tuple[str, str]]] = []
    current = ""

    if frame < 16:
        current = typed("8*8", frame / 15)
    else:
        history.append(("8*8", "64"))
        if frame < 35:
            current = ""
        elif frame < 54:
            current = typed("sin(30)", (frame - 35) / 18)
        else:
            history.append(("sin(30)", "0.5"))
            if frame < 65:
                current = ""
            elif frame < 80:
                current = typed("frac(0.1)", (frame - 65) / 14)
            else:
                history.append(("frac(0.1)", ("1", "10")))

    first_y = 40
    row_h = 46
    for index, (expression, result) in enumerate(history[-3:]):
        y = first_y + index * row_h
        c.text((10, y), expression, TEXT, 11, mono=True)
        if isinstance(result, tuple):
            draw_fraction(c, 278, y + 26, result[0], result[1])
        else:
            w = c.text_width(result, 11, mono=True)
            c.text((310 - w, y + 24), result, TEXT, 11, mono=True)

    c.rect((0, 202, 320, 203), BORDER)
    c.text((10, 218), ">", MUTED, 11, bold=True, mono=True, anchor="lm")
    c.text((28, 218), current, TEXT, 11, mono=True, anchor="lm")
    if cursor_visible(frame):
        cursor_x = 28 + c.text_width(current, 11, mono=True)
        c.rect((cursor_x, 209, cursor_x + 7, 226), ACCENT)
        if current:
            last = current[-1]
            last_w = c.text_width(last, 11, mono=True)
            c.text((cursor_x - last_w, 218), last, BG, 11, mono=True, anchor="lm")
    return c.image


def draw_home(selected: int) -> Image.Image:
    c = Canvas()
    draw_header(c, selected)
    for i, (_, label, icon, color) in enumerate(APPS):
        col = i % 4
        row = i // 4
        x = 8 + col * 78
        y = 48 + row * 50
        active = i == selected
        border = ACCENT if active else BORDER
        fill = SURFACE_2 if active else SURFACE
        c.rect((x, y, x + 70, y + 46), fill, border, radius=2)
        c.rect((x + 25, y + 6, x + 45, y + 24), BG, radius=2)
        draw_icon(c, icon, x + 26, y + 7, 18, color)
        c.text((x + 35, y + 36), label, TEXT, 7, bold=active, anchor="mm")
    c.text((160, 220), "enter - open", MUTED, 7, anchor="mm")
    return c.image


def draw_graph(local_frame: int) -> Image.Image:
    c = Canvas()
    top = 0
    bottom = 228
    for x in range(0, 321, 32):
        c.line((x, top, x, bottom), BORDER if x == 160 else GRID)
    for y in range(4, 229, 28):
        c.line((0, y, 319, y), BORDER if abs(y - 116) < 8 else GRID)

    points: list[float] = []
    for px in range(320):
        world_x = (px - 160) / 16
        world_y = math.sin(world_x * .75) * 3.8
        py = 116 - world_y * 14
        points.extend((px, py))
    c.line(tuple(points), "#7ab8ff", 2)

    points2: list[float] = []
    for px in range(320):
        world_x = (px - 160) / 16
        world_y = .07 * world_x * world_x - 2.0
        py = 116 - world_y * 14
        points2.extend((px, py))
    c.line(tuple(points2), "#c879f2", 1)

    progress = min(1.0, max(0.0, local_frame / 48))
    tx = 78 + progress * 160
    world_x = (tx - 160) / 16
    world_y = math.sin(world_x * .75) * 3.8
    ty = 116 - world_y * 14
    c.line((tx, top, tx, bottom), TEXT)
    c.rect((tx - 2, ty - 2, tx + 2, ty + 2), TEXT)
    c.text((6, 7), f"Y1  x {world_x: .2f}  y {world_y: .2f}", TEXT, 7, mono=True)
    c.rect((0, 229, 320, 240), HEADER)
    c.text((4, 235), "y= funcs   window - set   zoom - mode   trace - cursor", TEXT, 6, anchor="lm")
    return c.image


def draw_settings(selected: int) -> Image.Image:
    c = Canvas()
    draw_header(c, 8)
    items = [
        "Brightness 80%",
        "Auto sleep on",
        "Power save off",
        "Theme dark",
        "Reset to factory",
    ]
    for i, item in enumerate(items):
        y = 34 + i * 22
        c.rect((18, y, 302, y + 18), ACCENT_2 if i == selected else SURFACE, radius=2)
        c.text((28, y + 9), item, TEXT, 8, bold=i == selected, anchor="lm")
        if i in {1, 2}:
            on = i == 1
            c.rect((269, y + 4, 292, y + 14), ACCENT if on else BORDER, radius=5)
            knob_x = 286 if on else 275
            c.ellipse((knob_x - 4, y + 5, knob_x + 4, y + 13), fill=TEXT)
    footer = "left, right - brightness" if selected == 0 else "enter - toggle"
    c.text((18, 220), footer, MUTED, 7)
    return c.image


def draw_app_demo(app_index: int, local_frame: int) -> Image.Image:
    c = Canvas()
    draw_header(c, app_index)
    pulse = min(1.0, local_frame / 8)

    if app_index == 2:  # Table
        c.rect((8, 36, 312, 61), SURFACE, BORDER, radius=2)
        c.text((16, 48), "Start  -2.0", TEXT, 7, mono=True, anchor="lm")
        c.text((127, 48), "Step  0.5", TEXT, 7, mono=True, anchor="lm")
        c.text((228, 48), "Rows  7", TEXT, 7, mono=True, anchor="lm")
        cols = ["x", "Y1=x^2", "Y2=sin(x)"]
        xs = [8, 76, 190, 312]
        for i, label in enumerate(cols):
            c.rect((xs[i], 70, xs[i + 1], 91), ACCENT_2, BORDER)
            c.text(((xs[i] + xs[i + 1]) / 2, 80), label, TEXT, 7, bold=True, anchor="mm")
        for row in range(6):
            y = 91 + row * 22
            values = [f"{-2 + row * .5:.1f}", f"{(-2 + row * .5) ** 2:.2f}",
                      f"{math.sin(-2 + row * .5):.3f}"]
            for col, value in enumerate(values):
                c.rect((xs[col], y, xs[col + 1], y + 22), SURFACE_2 if row == local_frame // 4 % 6 else BG, GRID)
                c.text(((xs[col] + xs[col + 1]) / 2, y + 11), value, TEXT, 7, mono=True, anchor="mm")
        c.text((160, 230), "2nd+Window setup   arrows browse", MUTED, 6, anchor="mm")
    elif app_index == 3:  # Python
        c.rect((8, 36, 312, 197), SURFACE, BORDER, radius=2)
        lines = ["n = int(input('Terms: '))", "a, b = 0, 1", "for i in range(n):",
                 "    print(a)", "    a, b = b, a + b"]
        for i, line in enumerate(lines):
            c.text((15, 48 + i * 20), f"{i + 1:>2}  {line}", TEXT if i != 2 else ACCENT, 7, mono=True)
        if local_frame < 10:
            c.rect((12, 46 + (local_frame // 3) * 20, 16, 59 + (local_frame // 3) * 20), ACCENT)
        else:
            c.rect((8, 202, 312, 231), BG, BORDER, radius=2)
            c.text((15, 210), "Terms: 8", YELLOW, 7, mono=True)
            c.text((15, 221), "0  1  1  2  3  5  8  13", GREEN, 7, mono=True)
    elif app_index == 4:  # Statistics
        tabs = ["Summary", "Regression", "Plots", "Distributions"]
        for i, label in enumerate(tabs):
            x = 8 + i * 76
            c.rect((x, 36, x + 70, 57), ACCENT_2 if i == 2 else SURFACE, BORDER, radius=2)
            c.text((x + 35, 46), label, TEXT, 6, bold=i == 2, anchor="mm")
        c.rect((8, 66, 312, 222), SURFACE, BORDER)
        for i, h in enumerate([34, 57, 83, 66, 111, 94, 72, 45]):
            x = 22 + i * 34
            c.rect((x, 207 - h, x + 22, 207), "#6f9ee8")
        c.line((16, 207, 303, 207), BORDER)
        c.text((18, 76), "Histogram  L1", TEXT, 8, bold=True)
        c.text((296, 76), "n=42", MUTED, 7, mono=True, anchor="ra")
    elif app_index == 5:  # Lists
        for i in range(6):
            x = 7 + i * 51
            c.rect((x, 36, x + 47, 58), ACCENT_2 if i == 0 else SURFACE, ACCENT if i == 0 else BORDER, radius=2)
            c.text((x + 23, 47), f"L{i + 1}", TEXT, 7, bold=i == 0, anchor="mm")
        c.rect((7, 67, 313, 220), SURFACE, BORDER)
        c.text((20, 78), "INDEX", MUTED, 6)
        c.text((90, 78), "VALUE", MUTED, 6)
        values = [2, 5, 8, 13, 21, 34]
        for i, value in enumerate(values):
            y = 91 + i * 20
            active = i == (local_frame // 3) % len(values)
            if active:
                c.rect((14, y - 3, 180, y + 14), ACCENT_2)
            c.text((30, y + 5), str(i + 1), MUTED, 7, mono=True, anchor="mm")
            c.text((96, y + 5), str(value), TEXT, 8, mono=True, anchor="mm")
            c.rect((203, y, 203 + value * 2.5, y + 9), "#8f9db4")
        c.text((223, 205), "sum 83", GREEN, 7, mono=True)
    elif app_index == 6:  # Matrices
        c.text((10, 39), "[A]  3 x 3", TEXT, 8, bold=True)
        vals = [[2, 1, 0], [-1, 3, 2], [4, 0, 1]]
        for row in range(3):
            for col in range(3):
                x, y = 28 + col * 53, 65 + row * 38
                active = row * 3 + col == (local_frame // 2) % 9
                c.rect((x, y, x + 44, y + 29), ACCENT_2 if active else SURFACE, ACCENT if active else BORDER)
                c.text((x + 22, y + 14), str(vals[row][col]), TEXT, 9, mono=True, anchor="mm")
        c.rect((205, 55, 307, 192), SURFACE, BORDER, radius=2)
        for i, action in enumerate(["det(A)", "A^-1", "rref(A)", "transpose", "augment"]):
            c.text((216, 72 + i * 22), action, ACCENT if i == 2 else TEXT, 7, mono=True)
        c.text((15, 214), "rref(A) = identity", GREEN, 7, mono=True)
    elif app_index == 7:  # Solver
        choices = ["Equation Solver", "System Solver", "Polynomial Solver", "Numeric Solver"]
        for i, label in enumerate(choices):
            y = 38 + i * 27
            c.rect((10, y, 155, y + 22), ACCENT_2 if i == 2 else SURFACE, ACCENT if i == 2 else BORDER, radius=2)
            c.text((18, y + 11), label, TEXT, 7, bold=i == 2, anchor="lm")
        c.rect((166, 38, 310, 196), SURFACE, BORDER, radius=2)
        c.text((178, 51), "x^3 - 6x^2 + 11x - 6", TEXT, 6, mono=True)
        c.text((178, 76), "Exact roots", MUTED, 7, bold=True)
        for i, root in enumerate(["x1 = 1", "x2 = 2", "x3 = 3"]):
            c.text((187, 99 + i * 23), root, GREEN, 8, mono=True)
        c.text((178, 177), "verified by substitution", MUTED, 6)
    elif app_index == 9:  # Finance
        labels = [("N", "60"), ("I%", "5.25"), ("PV", "25000"), ("PMT", "-474.66"), ("FV", "0")]
        for i, (label, value) in enumerate(labels):
            col, row = i % 2, i // 2
            x, y = 15 + col * 153, 43 + row * 50
            c.text((x, y), label, MUTED, 7, bold=True)
            c.rect((x, y + 13, x + 135, y + 38), ACCENT_2 if i == local_frame // 4 % 5 else SURFACE, BORDER, radius=2)
            c.text((x + 126, y + 25), value, TEXT, 8, mono=True, anchor="rm")
        c.rect((168, 193, 303, 222), ACCENT_2, ACCENT, radius=2)
        c.text((235, 207), "Solve PMT", TEXT, 8, bold=True, anchor="mm")
    elif app_index == 10:  # Conics
        c.rect((8, 37, 133, 221), SURFACE, BORDER, radius=2)
        for i, label in enumerate(["Circle", "Parabola", "Ellipse", "Hyperbola", "General Conic", "Conic Graphs"]):
            c.text((18, 54 + i * 25), label, ACCENT if i == 2 else TEXT, 7, bold=i == 2)
        c.rect((143, 37, 312, 221), BG, BORDER)
        c.line((227, 45, 227, 212), GRID)
        c.line((151, 129, 304, 129), GRID)
        c.ellipse((169, 77, 285, 181), outline="#d0d7e2", width=2)
        c.ellipse((181, 126, 185, 130), fill=YELLOW)
        c.ellipse((269, 126, 273, 130), fill=YELLOW)
        c.text((155, 47), "x^2/9 + y^2/4 = 1", TEXT, 6, mono=True)
        c.text((155, 202), "center (0,0)   e=0.745", MUTED, 6)
    elif app_index == 11:  # Inequality
        c.rect((8, 37, 135, 221), SURFACE, BORDER, radius=2)
        c.text((17, 50), "SYSTEMS", TEXT, 8, bold=True)
        c.text((17, 77), "y <= 2x + 3", "#7ab8ff", 7, mono=True)
        c.text((17, 101), "y > -x + 1", "#c879f2", 7, mono=True)
        c.text((17, 137), "solution", MUTED, 6)
        c.text((17, 153), "intersection", MUTED, 6)
        c.text((17, 168), "(-0.67, 1.67)", GREEN, 7, mono=True)
        c.rect((145, 37, 312, 221), BG, BORDER)
        c.polygon((146, 220, 146, 135, 310, 53, 310, 220), "#173451")
        c.polygon((146, 220, 146, 70, 310, 220), "#382647")
        c.line((146, 135, 310, 53), "#7ab8ff", 2)
        for x in range(146, 311, 8):
            y = 70 + (x - 146) * .91
            c.line((x, y, min(x + 4, 310), min(y + 4, 220)), "#c879f2", 1)
    return c.image


def draw_game_menu(selected: int = 0) -> Image.Image:
    c = Canvas()
    c.rect((0, 0, 320, 22), HEADER)
    c.rect((0, 20, 320, 22), ACCENT)
    c.text((8, 11), "Games", TEXT, 9, bold=True, anchor="lm")
    c.text((225, 11), "high score", TEXT, 7, anchor="lm")
    games = [("Tetris", "12,480"), ("Doom", "18,950"), ("Snake", "740"),
             ("Breakout", "3,260"), ("Mario", "14,100")]
    for i, (name, score) in enumerate(games):
        y = 34 + i * 34
        c.rect((12, y, 308, y + 27), ACCENT_2 if i == selected else SURFACE,
               ACCENT if i == selected else BORDER, radius=2)
        c.text((22, y + 14), name, TEXT, 8, bold=i == selected, anchor="lm")
        c.text((298, y + 14), score, TEXT, 8, mono=True, anchor="rm")
    c.text((12, 220), "enter - play   back - home", MUTED, 7)
    return c.image


TETRIS_COLORS = ["#36e2f7", "#f7d34a", "#c879f2", "#5be36b", "#f75b5b", "#4a90f7", "#f7973a"]


def tetris_block(c: Canvas, col: int, row: int, color: str, ghost: bool = False) -> None:
    x = 105 + col * 11
    y = 10 + row * 11
    if ghost:
        c.rect((x + 2, y + 2, x + 9, y + 9), SURFACE, "#4a5462")
    else:
        c.rect((x + 1, y + 1, x + 10, y + 10), color)
        c.line((x + 2, y + 2, x + 8, y + 2), "#ffffff")


def panel(c: Canvas, x: int, y: int, w: int, h: int, title: str, value: str = "") -> None:
    c.rect((x, y, x + w, y + h), SURFACE, "#3a4250")
    c.text((x + 4, y + 5), title, MUTED, 5, bold=True)
    if value:
        c.text((x + 4, y + 16), value, TEXT, 6, mono=True)


def draw_tetris(local_frame: int) -> Image.Image:
    c = Canvas()
    c.rect((0, 0, 320, 240), BG)
    board_x, board_y, cell = 105, 10, 11
    c.rect((board_x - 1, board_y - 1, board_x + 111, board_y + 221), SURFACE, "#3a4250")
    for col in range(1, 10):
        c.line((board_x + col * cell, board_y, board_x + col * cell, board_y + 220), GRID)
    for row in range(1, 20):
        c.line((board_x, board_y + row * cell, board_x + 110, board_y + row * cell), GRID)

    stack = [
        (0, 19, 5), (1, 19, 5), (2, 19, 6), (3, 19, 6), (4, 19, 1), (6, 19, 2), (7, 19, 2), (8, 19, 3), (9, 19, 3),
        (0, 18, 4), (1, 18, 4), (3, 18, 0), (4, 18, 0), (6, 18, 1), (7, 18, 1), (9, 18, 6),
        (2, 17, 2), (3, 17, 2), (4, 17, 2), (8, 17, 5), (9, 17, 5),
    ]
    for col, row, color in stack:
        tetris_block(c, col, row, TETRIS_COLORS[color])

    fall_row = min(14, max(0, int(local_frame * .22)))
    piece = [(4, fall_row), (3, fall_row + 1), (4, fall_row + 1), (5, fall_row + 1)]
    ghost = [(4, 15), (3, 16), (4, 16), (5, 16)]
    for col, row in ghost:
        tetris_block(c, col, row, TETRIS_COLORS[2], ghost=True)
    for col, row in piece:
        tetris_block(c, col, row, TETRIS_COLORS[2])

    c.text((4, 10), "TETRIS", ACCENT, 8, bold=True)
    panel(c, 4, 26, 93, 36, "HOLD")
    for bx, by in [(34, 44), (40, 44), (46, 44), (52, 44)]:
        c.rect((bx, by, bx + 5, by + 5), TETRIS_COLORS[0])
    panel(c, 4, 65, 93, 26, "HIGH SCORE", "12480")
    panel(c, 4, 94, 93, 26, "SCORE", str(860 + local_frame * 4))
    panel(c, 4, 123, 93, 26, "LEVEL", "3")
    panel(c, 4, 152, 93, 26, "LINES", "18")
    c.text((4, 186), "R/L MOVE", MUTED, 5)
    c.text((4, 196), "Up/Down ROTATE", MUTED, 5)
    c.text((4, 206), "Y= DROP", MUTED, 5)
    c.text((4, 216), "Window HOLD", MUTED, 5)
    c.text((4, 226), "Back PAUSE", MUTED, 5)

    c.text((223, 10), "NEXT", ACCENT, 8, bold=True)
    next_shapes = [TETRIS_COLORS[1], TETRIS_COLORS[3], TETRIS_COLORS[5], TETRIS_COLORS[6]]
    for i, color in enumerate(next_shapes):
        y = 26 + i * 41
        c.rect((223, y, 316, y + 36), SURFACE, "#3a4250")
        cx, cy = 268, y + 17
        for dx, dy in [(-6, 0), (0, 0), (6, 0), (0, -6)]:
            c.rect((cx + dx, cy + dy, cx + dx + 5, cy + dy + 5), color)
    return c.image


def tour_frame(frame: int) -> Image.Image:
    if frame < 28:
        return draw_calculator(int(frame * 84 / 27))
    if frame < 43:
        return draw_home(min(11, (frame - 28) * 12 // 15))

    scene = (frame - 43) // 20
    local = (frame - 43) % 20
    if scene < 11:
        app_index = scene + 1
        if app_index == 1:
            return draw_graph(int(local * 54 / 19))
        if app_index == 8:
            return draw_settings(min(4, local // 4))
        return draw_app_demo(app_index, local)

    if frame < 278:
        return draw_game_menu(min(4, (frame - 263) // 3))
    return draw_tetris(frame - 278)


def main() -> None:
    frames = [tour_frame(frame) for frame in range(FRAME_COUNT)]
    frames[0].save(
        OUTPUT,
        save_all=True,
        append_images=frames[1:],
        duration=1000 // FPS,
        loop=0,
        optimize=True,
        disposal=2,
    )
    print(f"Wrote {OUTPUT} ({FRAME_COUNT / FPS:.1f}s, {WIDTH}x{HEIGHT}, {FPS} fps)")


if __name__ == "__main__":
    main()
