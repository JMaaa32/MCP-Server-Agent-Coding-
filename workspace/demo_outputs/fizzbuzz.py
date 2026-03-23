def fizzbuzz(n):
    """
    返回 1 到 n 的 FizzBuzz 列表
    
    规则：
    - 如果数字能被 3 整除，用 "Fizz" 替换
    - 如果数字能被 5 整除，用 "Buzz" 替换
    - 如果数字能被 3 和 5 整除，用 "FizzBuzz" 替换
    - 否则保留数字本身
    
    参数:
    n (int): 最大值
    
    返回:
    list: FizzBuzz 结果列表
    """
    result = []
    for i in range(1, n + 1):
        if i % 3 == 0 and i % 5 == 0:
            result.append("FizzBuzz")
        elif i % 3 == 0:
            result.append("Fizz")
        elif i % 5 == 0:
            result.append("Buzz")
        else:
            result.append(str(i))
    return result

if __name__ == "__main__":
    # 示例用法
    print("FizzBuzz for 15:")
    for item in fizzbuzz(15):
        print(item)