"""A module for calculating Fibonacci numbers using different methods."""


def fibonacci_recursive(n):
    """Calculate the nth Fibonacci number using recursion.
    
    The Fibonacci sequence is defined as:
    F(0) = 0, F(1) = 1
    F(n) = F(n-1) + F(n-2) for n > 1
    
    Args:
        n: The position in the Fibonacci sequence (non-negative integer).
        
    Returns:
        The nth Fibonacci number.
        
    Raises:
        ValueError: If n is negative.
    """
    if n < 0:
        raise ValueError("n must be non-negative")
    if n <= 1:
        return n
    return fibonacci_recursive(n - 1) + fibonacci_recursive(n - 2)


def fibonacci_dp(n):
    """Calculate the nth Fibonacci number using dynamic programming.
    
    This method uses memoization to avoid redundant calculations,
    making it much more efficient than the naive recursive approach.
    
    Args:
        n: The position in the Fibonacci sequence (non-negative integer).
        
    Returns:
        The nth Fibonacci number.
        
    Raises:
        ValueError: If n is negative.
    """
    if n < 0:
        raise ValueError("n must be non-negative")
    if n <= 1:
        return n
    
    # Initialize DP table
    dp = [0] * (n + 1)
    dp[1] = 1
    
    # Fill DP table
    for i in range(2, n + 1):
        dp[i] = dp[i - 1] + dp[i - 2]
    
    return dp[n]


def fibonacci_generator(n):
    """Generate Fibonacci numbers up to the nth position.
    
    This method uses a generator to yield Fibonacci numbers one by one,
    which is memory efficient for large sequences.
    
    Args:
        n: The maximum position in the Fibonacci sequence (non-negative integer).
        
    Yields:
        Fibonacci numbers from position 0 to n.
        
    Raises:
        ValueError: If n is negative.
    """
    if n < 0:
        raise ValueError("n must be non-negative")
    
    a, b = 0, 1
    for i in range(n + 1):
        if i == 0:
            yield a
        elif i == 1:
            yield b
        else:
            a, b = b, a + b
            yield b


def fibonacci_generator_list(n):
    """Get a list of Fibonacci numbers up to the nth position.
    
    This is a convenience function that uses the generator to create a list.
    
    Args:
        n: The maximum position in the Fibonacci sequence (non-negative integer).
        
    Returns:
        A list containing Fibonacci numbers from position 0 to n.
        
    Raises:
        ValueError: If n is negative.
    """
    return list(fibonacci_generator(n))