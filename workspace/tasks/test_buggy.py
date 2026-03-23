"""Tests for buggy.py — some will fail due to bugs."""

from buggy import sort_list, find_max


def test_sort_basic():
    assert sort_list([3, 1, 2]) == [1, 2, 3]


def test_sort_already_sorted():
    assert sort_list([1, 2, 3, 4]) == [1, 2, 3, 4]


def test_sort_reverse():
    assert sort_list([5, 4, 3, 2, 1]) == [1, 2, 3, 4, 5]


def test_find_max_positive():
    assert find_max([1, 5, 3, 9, 2]) == 9


def test_find_max_negative():
    assert find_max([-3, -1, -7]) == -1


def test_find_max_empty():
    assert find_max([]) is None
