"""Tests for palindrome.py."""

from palindrome import is_palindrome


def test_is_palindrome_basic():
    """Test basic palindrome cases."""
    assert is_palindrome("racecar") == True
    assert is_palindrome("level") == True
    assert is_palindrome("radar") == True
    assert is_palindrome("hello") == False
    assert is_palindrome("world") == False


def test_is_palindrome_empty():
    """Test empty string and single character."""
    assert is_palindrome("") == True  # Empty string is considered palindrome
    assert is_palindrome("a") == True
    assert is_palindrome("A") == True


def test_is_palindrome_case_insensitive():
    """Test case insensitive palindrome detection."""
    assert is_palindrome("Racecar") == True
    assert is_palindrome("Level") == True
    assert is_palindrome("RaDaR") == True
    assert is_palindrome("Python") == False


def test_is_palindrome_with_spaces():
    """Test palindrome detection with spaces."""
    assert is_palindrome("race car") == True
    assert is_palindrome("a man a plan a canal panama") == True
    assert is_palindrome("was it a car or a cat i saw") == True
    assert is_palindrome("not a palindrome") == False


def test_is_palindrome_with_punctuation():
    """Test palindrome detection with punctuation."""
    assert is_palindrome("racecar!") == True
    assert is_palindrome("A man, a plan, a canal: Panama!") == True
    assert is_palindrome("Was it a car or a cat I saw?") == True
    assert is_palindrome("Hello, world!") == False


def test_is_palindrome_numbers():
    """Test palindrome detection with numbers."""
    assert is_palindrome("12321") == True
    assert is_palindrome("123321") == True
    assert is_palindrome("12345") == False
    assert is_palindrome("12321 is a palindrome") == False


def test_is_palindrome_mixed():
    """Test palindrome detection with mixed alphanumeric characters."""
    assert is_palindrome("a1b2b1a") == True
    assert is_palindrome("a1b2c2b1a") == True
    assert is_palindrome("a1b2c3") == False