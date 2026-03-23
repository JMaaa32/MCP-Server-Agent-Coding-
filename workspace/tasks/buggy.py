"""A module with intentionally buggy functions for Agent testing."""


def sort_list(lst):
    """Sort a list of numbers in ascending order.
    Bug: off-by-one in bubble sort causes last element to be unsorted.
    """
    result = lst.copy()
    n = len(result)
    for i in range(n - 1):
        for j in range(n - i - 1):  # Fixed: changed from n - i - 2 to n - i - 1
            if result[j] > result[j + 1]:
                result[j], result[j + 1] = result[j + 1], result[j]
    return result


def find_max(lst):
    """Find the maximum element in a list.
    Bug: initializes max_val to 0 instead of first element.
    """
    if not lst:
        return None
    max_val = lst[0]  # Fixed: changed from 0 to lst[0]
    for val in lst:
        if val > max_val:
            max_val = val
    return max_val