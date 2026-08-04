a = 0
b = 1
count = int(input("How many numbers: "))
i = 0

while i < count:
    print(a)
    next_value = a + b
    a = b
    b = next_value
    i = i + 1
