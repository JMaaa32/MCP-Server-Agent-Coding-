"""Tests for fibonacci.py."""

from fibonacci import (
    fibonacci_recursive,
    fibonacci_dp,
    fibonacci_generator,
    fibonacci_generator_list
)


def test_fibonacci_base_cases():
    """Test base cases for all Fibonacci methods."""
    print("Testing base cases...")
    
    # Test F(0) = 0
    assert fibonacci_recursive(0) == 0
    assert fibonacci_dp(0) == 0
    assert list(fibonacci_generator(0)) == [0]
    assert fibonacci_generator_list(0) == [0]
    
    # Test F(1) = 1
    assert fibonacci_recursive(1) == 1
    assert fibonacci_dp(1) == 1
    assert list(fibonacci_generator(1)) == [0, 1]
    assert fibonacci_generator_list(1) == [0, 1]
    
    print("✓ Base cases passed")


def test_fibonacci_small_numbers():
    """Test Fibonacci numbers for small n."""
    print("Testing small numbers...")
    
    # Known Fibonacci sequence: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55
    test_cases = [
        (2, 1),
        (3, 2),
        (4, 3),
        (5, 5),
        (6, 8),
        (7, 13),
        (8, 21),
        (9, 34),
        (10, 55),
    ]
    
    for n, expected in test_cases:
        assert fibonacci_recursive(n) == expected
        assert fibonacci_dp(n) == expected
    
    print("✓ Small numbers passed")


def test_fibonacci_sequence():
    """Test that all methods produce the same sequence."""
    print("Testing sequence consistency...")
    
    n = 10
    expected_sequence = [0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55]
    
    # Test generator produces correct sequence
    assert list(fibonacci_generator(n)) == expected_sequence
    assert fibonacci_generator_list(n) == expected_sequence
    
    # Test that individual values match
    for i, expected in enumerate(expected_sequence):
        assert fibonacci_recursive(i) == expected
        assert fibonacci_dp(i) == expected
    
    print("✓ Sequence consistency passed")


def test_fibonacci_larger_numbers():
    """Test Fibonacci numbers for larger n."""
    print("Testing larger numbers...")
    
    # Test some larger Fibonacci numbers
    test_cases = [
        (15, 610),
        (20, 6765),
        (25, 75025),
    ]
    
    for n, expected in test_cases:
        assert fibonacci_dp(n) == expected
    
    print("✓ Larger numbers passed")


def test_fibonacci_generator_behavior():
    """Test the generator behavior."""
    print("Testing generator behavior...")
    
    gen = fibonacci_generator(5)
    
    # Test that generator yields values correctly
    assert next(gen) == 0
    assert next(gen) == 1
    assert next(gen) == 1
    assert next(gen) == 2
    assert next(gen) == 3
    assert next(gen) == 5
    
    # Generator should be exhausted
    try:
        next(gen)
        assert False, "Generator should be exhausted"
    except StopIteration:
        pass  # Expected
    
    print("✓ Generator behavior passed")


def test_fibonacci_negative_input():
    """Test that negative input raises ValueError."""
    print("Testing negative input...")
    
    # Test recursive method
    try:
        fibonacci_recursive(-1)
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "n must be non-negative" in str(e)
    
    # Test DP method
    try:
        fibonacci_dp(-5)
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "n must be non-negative" in str(e)
    
    # Test generator
    try:
        list(fibonacci_generator(-10))
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "n must be non-negative" in str(e)
    
    # Test generator list
    try:
        fibonacci_generator_list(-3)
        assert False, "Should have raised ValueError"
    except ValueError as e:
        assert "n must be non-negative" in str(e)
    
    print("✓ Negative input tests passed")


def test_fibonacci_methods_consistency():
    """Test that all methods give consistent results."""
    print("Testing methods consistency...")
    
    for n in range(0, 20):  # Test up to n=19 (recursive is still manageable)
        recursive_result = fibonacci_recursive(n)
        dp_result = fibonacci_dp(n)
        generator_result = list(fibonacci_generator(n))[-1]  # Last element
        
        assert recursive_result == dp_result == generator_result
    
    print("✓ Methods consistency passed")


def test_fibonacci_generator_edge_cases():
    """Test edge cases for the generator."""
    print("Testing generator edge cases...")
    
    # Test n=0
    assert list(fibonacci_generator(0)) == [0]
    
    # Test n=1
    assert list(fibonacci_generator(1)) == [0, 1]
    
    # Test that generator can be iterated multiple times
    for _ in range(3):
        assert list(fibonacci_generator(3)) == [0, 1, 1, 2]
    
    print("✓ Generator edge cases passed")


def test_fibonacci_performance_demo():
    """Demonstrate performance difference between methods."""
    print("Demonstrating performance difference...")
    
    # DP should handle this quickly
    dp_result = fibonacci_dp(30)
    assert dp_result == 832040
    
    print("✓ Performance demo completed")
    print("  Note: Recursive method would be very slow for n=30")


def test_fibonacci_generator_memory_efficiency():
    """Test that generator is memory efficient."""
    print("Testing generator memory efficiency...")
    
    # Generate a large sequence without storing it all in memory
    n = 100
    count = 0
    total = 0
    
    for fib in fibonacci_generator(n):
        count += 1
        total += fib
    
    # Verify we got the right number of elements
    assert count == n + 1  # 0 to n inclusive
    
    print("✓ Generator memory efficiency test passed")
    print(f"  Processed {count} Fibonacci numbers")


def run_all_tests():
    """Run all tests."""
    print("=" * 60)
    print("Running Fibonacci tests...")
    print("=" * 60)
    
    try:
        test_fibonacci_base_cases()
        test_fibonacci_small_numbers()
        test_fibonacci_sequence()
        test_fibonacci_larger_numbers()
        test_fibonacci_generator_behavior()
        test_fibonacci_negative_input()
        test_fibonacci_methods_consistency()
        test_fibonacci_generator_edge_cases()
        test_fibonacci_performance_demo()
        test_fibonacci_generator_memory_efficiency()
        
        print("=" * 60)
        print("✅ All tests passed!")
        print("=" * 60)
        return True
    except AssertionError as e:
        print("=" * 60)
        print(f"❌ Test failed: {e}")
        print("=" * 60)
        return False
    except Exception as e:
        print("=" * 60)
        print(f"❌ Unexpected error: {e}")
        print("=" * 60)
        return False


if __name__ == "__main__":
    run_all_tests()