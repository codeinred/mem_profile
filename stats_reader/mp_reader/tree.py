from collections.abc import Iterable
import typing
from dataclasses import dataclass


@dataclass
class Tree[T, U]:
    head: T
    values: typing.Iterable[U]


def _iter_check_last[T](items: typing.Iterable[T]) -> typing.Iterable[tuple[bool, T]]:
    """Iterats through the given collection. Returns (True, item) until reaching the last item, at which (False, item) is returned"""
    it = iter(items)
    try:
        a = next(it)

        try:
            while True:
                b = next(it)
                yield (True, a)
                a = b
        except StopIteration:
            yield (False, a)
            return
    except StopIteration:
        return


def _print_item(
    item, i1: str, c1: str, c2: str, file: typing.IO | None = None
):
    match item:
        case Tree():
            _print_tree(item, i1, c1, c2, file)
        case str():
            print(i1, c1, item, sep="", file=file)
        case []:
            print(i1, c1, "[]", sep="", file=file)
        case [first, *rest]:
            _print_item(first, i1, c1, c2, file=file)
            for x in rest:
                _print_item(x, i1, c2, c2, file=file)
        case _:
            print(i1, c1, item, sep="", file=file)

def print_iter(
    ls: typing.Iterable,
    indent: str = "",
    file: typing.IO | None = None,
):
    for has_next, item in _iter_check_last(ls):
        if isinstance(item, Tree):
            if has_next:
                _print_tree(item, indent, "├── ", "│   ", file)
            else:
                _print_tree(item, indent, "└── ", "    ", file)
        else:
            if has_next:
                _print_item(item, indent, "├── ", "│   ", file)
            else:
                _print_item(item, indent, "└── ", "    ", file)


def _print_tree(
    tree: Tree,
    i1: str = "",
    c1: str = "",
    c2: str = "",
    file: typing.IO | None = None,
):
    _print_item(tree.head, i1, c1, c2, file)
    print_iter(tree.values, i1 + c2, file)


def print_tree(
    tree: Tree,
    file: typing.IO | None = None,
):
    _print_tree(tree, file=file)
