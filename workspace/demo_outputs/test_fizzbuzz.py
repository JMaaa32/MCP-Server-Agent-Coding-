import unittest
from fizzbuzz import fizzbuzz

class TestFizzBuzz(unittest.TestCase):
    
    def test_fizzbuzz_1(self):
        """测试 n=1 的情况"""
        result = fizzbuzz(1)
        expected = ["1"]
        self.assertEqual(result, expected)
    
    def test_fizzbuzz_3(self):
        """测试 n=3 的情况，包含第一个 Fizz"""
        result = fizzbuzz(3)
        expected = ["1", "2", "Fizz"]
        self.assertEqual(result, expected)
    
    def test_fizzbuzz_5(self):
        """测试 n=5 的情况，包含第一个 Buzz"""
        result = fizzbuzz(5)
        expected = ["1", "2", "Fizz", "4", "Buzz"]
        self.assertEqual(result, expected)
    
    def test_fizzbuzz_15(self):
        """测试 n=15 的情况，包含 FizzBuzz"""
        result = fizzbuzz(15)
        expected = [
            "1", "2", "Fizz", "4", "Buzz", "Fizz", "7", "8", "Fizz", "Buzz",
            "11", "Fizz", "13", "14", "FizzBuzz"
        ]
        self.assertEqual(result, expected)
    
    def test_fizzbuzz_30(self):
        """测试 n=30 的情况，包含多个 FizzBuzz"""
        result = fizzbuzz(30)
        # 验证几个关键点
        self.assertEqual(result[2], "Fizz")    # 3
        self.assertEqual(result[4], "Buzz")    # 5
        self.assertEqual(result[14], "FizzBuzz")  # 15
        self.assertEqual(result[29], "FizzBuzz")  # 30
        self.assertEqual(len(result), 30)
    
    def test_fizzbuzz_zero(self):
        """测试 n=0 的情况"""
        result = fizzbuzz(0)
        expected = []
        self.assertEqual(result, expected)
    
    def test_fizzbuzz_negative(self):
        """测试负数输入"""
        result = fizzbuzz(-5)
        expected = []
        self.assertEqual(result, expected)

if __name__ == "__main__":
    unittest.main()