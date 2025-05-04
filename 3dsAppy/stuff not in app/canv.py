with open("texdump.raw", "rb") as f:
    data = f.read()

nibbles = []

# Define the color mappings to nibbles
for i in range(0, len(data), 4):
    a, r, g, b = data[i:i+4]

    # Check if it's white (FFFFFF)
    if a == 0x0 and r == 0 and g == 0 and b == 0:
        nibbles.append(0)
    # Check if it's black (FF000000)
    elif a == 0xFF and r == 0 and g == 0 and b == 0:
        nibbles.append(1)
    # Full red
    elif a == 0xFF and r == 0xFF and g == 0 and b == 0:
        nibbles.append(2)
    # Full green
    elif a == 0xFF and r == 0 and g == 0xFF and b == 0:
        nibbles.append(3)
    # Full blue
    elif a == 0xFF and r == 0 and g == 0 and b == 0xFF:
        nibbles.append(4)
    # Add additional checks for other colors or special cases if needed
    # You can extend the logic to map more colors to nibbles as per your needs.
    else:
        # For any other color, you can map them to an unused nibble value, or add custom mappings
        nibbles.append(15)  # Example default case, you can change as needed

# Now, write the nibbles to a new file
with open("canvnibbles.raw", "wb") as out_file:
    # Write nibbles as bytes, we need to group them into 2 nibbles per byte
    for i in range(0, len(nibbles), 2):
        # Combine 2 nibbles into 1 byte (shift the first nibble to the higher bits and OR it with the second nibble)
        nibble1 = nibbles[i] if i < len(nibbles) else 0
        nibble2 = nibbles[i+1] if i + 1 < len(nibbles) else 0
        byte = (nibble1 << 4) | nibble2
        out_file.write(bytes([byte]))

print("Count:", len(nibbles))
print("Nibbles written to canvnibbles.raw")