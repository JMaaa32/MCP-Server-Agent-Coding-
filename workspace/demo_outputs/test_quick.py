from palindrome import is_palindrome

test_cases = [
    ("racecar", True),
    ("hello", False),
    ("A man, a plan, a canal: Panama!", True),
    ("", True),
    ("12321", True),
    ("12345", False),
]

print("Testing is_palindrome function:")
all_passed = True
for test_str, expected in test_cases:
    result = is_palindrome(test_str)
    status = "✓" if result == expected else "✗"
    print(f"{status} '{test_str}' -> {result} (expected: {expected})")
    if result != expected:
        all_passed = False

if all_passed:
    print("\nAll tests passed!")
else:
    print("\nSome tests failed!")