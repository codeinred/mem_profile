"""
Contains color codes + terminal style options

https://stackoverflow.com/a/33206814
"""

import typing
from dataclasses import dataclass


# Reset
RE = "\033[0m"

BOLD = "\033[1m"
FAINT = "\033[2m"
ITALIC = "\033[3m"
UNDERLINE = "\033[4m"
REVERSE = "\033[7m"

# Foreground colors
BL = "\033[30m"  # Black
R = "\033[31m"  # Red
G = "\033[32m"  # Green
Y = "\033[33m"  # Yellow
B = "\033[34m"  # Blue
M = "\033[35m"  # Magenta
C = "\033[36m"  # Cyan
W = "\033[37m"  # White

Grey = "\033[38;2;130;130;120m"

BOLD_BL = "\033[30;1m"  # Black
BOLD_R = "\033[31;1m"  # Red
BOLD_G = "\033[32;1m"  # Green
BOLD_Y = "\033[33;1m"  # Yellow
BOLD_B = "\033[34;1m"  # Blue
BOLD_M = "\033[35;1m"  # Magenta
BOLD_C = "\033[36;1m"  # Cyan
BOLD_W = "\033[37;1m"  # White

# Bright foreground colors (prefer these for anything eye-catching)
BB_BL = "\033[90;1m"  # Bright (Bold) Black
BB_R = "\033[91;1m"  # Bright (Bold) Red
BB_G = "\033[92;1m"  # Bright (Bold) Green
BB_Y = "\033[93;1m"  # Bright (Bold) Yellow
BB_B = "\033[94;1m"  # Bright (Bold) Blue
BB_M = "\033[95;1m"  # Bright (Bold) Magenta
BB_C = "\033[96;1m"  # Bright (Bold) Cyan
BB_W = "\033[97;1m"  # Bright (Bold) White


@dataclass
class Styled:
    term_style: str
    content: typing.Any

    def __format__(self, format_spec) -> str:
        return f"{self.term_style}{self.content:{format_spec}}{RE}"

    def __str__(self) -> str:
        return f"{self.term_style}{str(self.content)}{RE}"


def black(input: typing.Any) -> Styled:
    return Styled(BL, input)


def red(input: typing.Any) -> Styled:
    return Styled(R, input)


def green(input: typing.Any) -> Styled:
    return Styled(G, input)


def yellow(input: typing.Any) -> Styled:
    return Styled(Y, input)


def blue(input: typing.Any) -> Styled:
    return Styled(B, input)


def magenta(input: typing.Any) -> Styled:
    return Styled(M, input)


def cyan(input: typing.Any) -> Styled:
    return Styled(C, input)


def white(input: typing.Any) -> Styled:
    return Styled(W, input)

def grey(input: typing.Any) -> Styled:
    return Styled(Grey, input)

def bold_black(input: typing.Any) -> Styled:
    return Styled(BOLD_BL, input)


def bold_red(input: typing.Any) -> Styled:
    return Styled(BOLD_R, input)


def bold_green(input: typing.Any) -> Styled:
    return Styled(BOLD_G, input)


def bold_yellow(input: typing.Any) -> Styled:
    return Styled(BOLD_Y, input)


def bold_blue(input: typing.Any) -> Styled:
    return Styled(BOLD_B, input)


def bold_magenta(input: typing.Any) -> Styled:
    return Styled(BOLD_M, input)


def bold_cyan(input: typing.Any) -> Styled:
    return Styled(BOLD_C, input)


def bold_white(input: typing.Any) -> Styled:
    return Styled(BOLD_W, input)


def bb_black(input: typing.Any) -> Styled:
    return Styled(BB_BL, input)


def bb_red(input: typing.Any) -> Styled:
    return Styled(BB_R, input)


def bb_green(input: typing.Any) -> Styled:
    return Styled(BB_G, input)


def bb_yellow(input: typing.Any) -> Styled:
    return Styled(BB_Y, input)


def bb_blue(input: typing.Any) -> Styled:
    return Styled(BB_B, input)


def bb_magenta(input: typing.Any) -> Styled:
    return Styled(BB_M, input)


def bb_cyan(input: typing.Any) -> Styled:
    return Styled(BB_C, input)


def bb_white(input: typing.Any) -> Styled:
    return Styled(BB_W, input)
