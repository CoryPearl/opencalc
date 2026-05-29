a = 0
b = 1
i = 0

while i < 10:
    print(a)
    next_value = a + b
    a = b
    b = next_value
    i = i + 1

# def fib(n):
#     a, b = 0, 1
#     for _ in range(n):
#         print(a, end=" ")
#         a, b = b, a + b

# fib(10)
