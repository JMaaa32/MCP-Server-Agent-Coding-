#!/usr/bin/env python3
"""Test script to verify sort_list bug."""

import sys
sys.path.insert(0, 'tasks')

from buggy import sort_list, find_max

# Test sort_list
print("Testing sort_list function:")
test_cases = [
    ([3, 1, 2], [1, 2, 3]),
    ([1, 2, 3, 4], [1, 2, 3, 4]),
    ([5, 4, 3, 2, 1], [1, 2, 3, 4, 5]),
    ([], []),
    ([1], [1]),
    ([2, 1], [1, 2]),
    ([3, 2, 1], [1, 2, 3]),
    ([4, 3, 2, 1], [1, 2, 3, 4]),
]

all_passed = True
for input_list, expected in test_cases:
    result = sort_list(input_list)
    if result == expected:
        print(f"✓ sort_list({input_list}) = {result}")
    else:
        print(f"✗ sort_list({input_list}) = {result}, expected {expected}")
        all_passed = False

print("\nTesting find_max function:")
test_cases_max = [
    ([1, 5, 3, 9, 2], 9),
    ([-3, -1, -7], -1),
    ([], None),
    ([5], 5),
    ([-10, -5, -20], -5),
]

for input_list, expected in test_cases_max:
    result = find_max(input_list)
    if result == expected:
        print(f"✓ find_max({input_list}) = {result}")
    else:
        print(f"✗ find_max({input_list}) = {result}, expected {expected}")
        all_passed = False

if all_passed:
    print("\nAll tests passed!")
else:
    print("\nSome tests failed!")