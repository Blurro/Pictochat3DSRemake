with open("texdump.raw", "rb") as f:
    data = f.read()

nibbles = []
for i in range(0, len(data), 4):
    r, g, b, a = data[i:i+4]
    if r == 0 and g == 0 and b == 0 and a == 0xFF:
        nibbles.append(0)
    elif r == 0 and g == 0 and b == 0 and a == 0x00:
        nibbles.append(1)

print("Nibbles:", nibbles)
print("Count:", len(nibbles))