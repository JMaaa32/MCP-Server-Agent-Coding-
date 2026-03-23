#!/usr/bin/env python3
"""Example usage of the fibonacci module."""

from fibonacci import (
    fibonacci_recursive,
    fibonacci_dp,
    fibonacci_generator,
    fibonacci_generator_list
)


def main():
    """Demonstrate different Fibonacci calculation methods."""
    print("Fibonacci Sequence Examples")
    print("=" * 50)
    
    # Example 1: Calculate individual Fibonacci numbers
    print("\n1. Calculating individual Fibonacci numbers:")
    print(f"   F(5) = {fibonacci_recursive(5)}  (recursive)")
    print(f"   F(10) = {fibonacci_dp(10)}  (dynamic programming)")
    print(f"   F(15) = {fibonacci_dp(15)}  (dynamic programming)")
    
    # Example 2: Generate Fibonacci sequence
    print("\n2. Generating Fibonacci sequence up to F(10):")
    sequence = fibonacci_generator_list(10)
    print(f"   Sequence: {sequence}")
    
    # Example 3: Using the generator
    print("\n3. Using the generator (memory efficient):")
    print("   First 10 Fibonacci numbers:")
    for i, fib in enumerate(fibonacci_generator(9)):
        print(f"   F({i}) = {fib}")
    
    # Example 4: Performance comparison
    print("\n4. Performance comparison:")
    print("   Calculating F(30) with different methods:")
    print(f"   - Dynamic Programming: {fibonacci_dp(30)}")
    print("   - Recursive: Very slow for n=30 (not shown)")
    
    # Example 5: Error handling
    print("\n5. Error handling:")
    try:
        fibonacci_recursive(-5)
    except ValueError as e:
        print(f"   Error caught: {e}")
    
    # Example 6: Large sequence generation
    print("\n6. Generating large sequence (first 20 numbers):")
    large_sequence = fibonacci_generator_list(19)
    print("   First 20 Fibonacci numbers:")
    for i in range(0, 20, 5):
        chunk = large_sequence[i:i+5]
        indices = [f"F({j})" for j in range(i, i+len(chunk))]
        print(f"   {', '.join(indices)}: {chunk}")


if __name__ == "__main__":
    main()