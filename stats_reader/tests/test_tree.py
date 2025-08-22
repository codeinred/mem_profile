import pytest
from mp_reader.tree import _iter_check_last, Tree, print_tree, _print_item
import io
from mp_reader.color import *


def test_iter_check_last():
    """Test _iter_check_last with various list sizes"""

    # Empty list should raise StopIteration

    result = list(_iter_check_last(iter([])))
    assert result == []

    # Single item list
    result = list(_iter_check_last(iter([1])))
    assert result == [(False, 1)]

    # Two item list
    result = list(_iter_check_last(iter([1, 2])))
    assert result == [(True, 1), (False, 2)]

    # Three item list
    result = list(_iter_check_last(iter([1, 2, 3])))
    assert result == [(True, 1), (True, 2), (False, 3)]

    # Five item list
    result = list(_iter_check_last(iter([1, 2, 3, 4, 5])))
    assert result == [(True, 1), (True, 2), (True, 3), (True, 4), (False, 5)]


def test_print_tree():
    def _print_get_out(func, *args) -> str:
        with io.StringIO() as out:
            print(f"Invoking {bb_blue(func.__name__)}{bb_blue(repr(args))}")
            func(*args, file=out)
            result = out.getvalue()
            print(result)
            return result

    assert _print_get_out(_print_item, "hello", "", "", "") == "hello\n"
    assert (
        _print_get_out(print_tree, Tree("head", [1, 2, 3]))
        == """head
├── 1
├── 2
└── 3
"""
    )
    assert (
        _print_get_out(
            print_tree,
            Tree("head", [1, Tree(["two", "three"], ["four", ["five", "five (again)"], "six"]), ["seven", "eight"]]),
        )
        == """head
├── 1
├── two
│   three
│   ├── four
│   ├── five
│   │   five (again)
│   └── six
└── seven
    eight
"""
    )
    assert (
        _print_get_out(print_tree, Tree("head", ["one", Tree("two", []), "three"]))
        == """head
├── one
├── two
└── three
"""
    )

    assert (
        _print_get_out(
            print_tree,
            Tree(
                "head",
                [
                    "one",
                    Tree("tests", ["test_tree.py"]),
                    Tree("three", ["four", "five", "six"]),
                ],
            ),
        )
        == """head
├── one
├── tests
│   └── test_tree.py
└── three
    ├── four
    ├── five
    └── six
"""
    )

    assert (
        _print_get_out(
            print_tree,
            Tree(
                "head",
                [
                    "one",
                    Tree("tests", ["test_tree.py", Tree("wip", ["x", "y", "z"])]),
                    Tree("three", ["four", "five", "six"]),
                ],
            ),
        )
        == """head
├── one
├── tests
│   ├── test_tree.py
│   └── wip
│       ├── x
│       ├── y
│       └── z
└── three
    ├── four
    ├── five
    └── six
"""
    )

    assert (
        _print_get_out(
            print_tree,
            Tree(
                "head",
                [
                    "one",
                    Tree(
                        "tests",
                        [
                            "test_tree.py",
                            Tree(
                                ["wip", "more info", "more info 2"],
                                ["x", ["y", "y2"], ["z", "z2"]],
                            ),
                        ],
                    ),
                    Tree("three", ["four", "five", "six"]),
                ],
            ),
        )
        == """head
├── one
├── tests
│   ├── test_tree.py
│   └── wip
│       more info
│       more info 2
│       ├── x
│       ├── y
│       │   y2
│       └── z
│           z2
└── three
    ├── four
    ├── five
    └── six
"""
    )

    # This case is very cursed, but for convinience we permit trees an entities within
    # an item...
    assert (
        _print_get_out(
            print_tree,
            Tree(
                "head",
                [
                    "one",
                    Tree(
                        "tests",
                        [
                            "test_tree.py",
                            Tree(
                                ["wip", Tree("more info", ['a', 'b', 'c']), "more info 2"],
                                ["x", ["y", "y2"], ["z", "z2"]],
                            ),
                            "ent3"
                        ],
                    ),
                    Tree("three", ["four", "five", "six"]),
                ],
            ),
        )
        == """head
├── one
├── tests
│   ├── test_tree.py
│   ├── wip
│   │   more info
│   │   ├── a
│   │   ├── b
│   │   └── c
│   │   more info 2
│   │   ├── x
│   │   ├── y
│   │   │   y2
│   │   └── z
│   │       z2
│   └── ent3
└── three
    ├── four
    ├── five
    └── six
"""
    )

    assert (
        _print_get_out(
            print_tree,
            Tree(
                "head",
                [
                    "one",
                    Tree(
                        "tests",
                        [
                            "test_tree.py",
                            Tree(
                                ["wip", "more info", "more info 2"],
                                [bb_red("x"), ["y", bb_yellow("y2")], ["z", "z2"]],
                            ),
                        ],
                    ),
                    Tree("three", ["four", "five", "six"]),
                ],
            ),
        )
        == f"""head
├── one
├── tests
│   ├── test_tree.py
│   └── wip
│       more info
│       more info 2
│       ├── {bb_red('x')}
│       ├── y
│       │   {bb_yellow('y2')}
│       └── z
│           z2
└── three
    ├── four
    ├── five
    └── six
"""
    )
