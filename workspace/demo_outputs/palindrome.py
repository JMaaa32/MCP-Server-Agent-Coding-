"""A module for checking palindromes."""


def is_palindrome(s):
    """Check if a string is a palindrome.
    
    A palindrome is a word, phrase, number, or other sequence of characters
    which reads the same forward and backward, ignoring spaces, punctuation,
    and capitalization.
    
    Args:
        s: The string to check.
        
    Returns:
        True if the string is a palindrome, False otherwise.
    """
    # Clean the string: remove non-alphanumeric characters and convert to lowercase
    cleaned = ''.join(char.lower() for char in s if char.isalnum())
    
    # Check if the cleaned string reads the same forward and backward
    return cleaned == cleaned[::-1]