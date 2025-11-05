file = open("saltysd_core.elf", "rb")
DATA = file.read()
file.close()

file = open("size.txt", "r")
size = int(file.read(), base=10)
file.close()

deadbeef_bytes = 0xDEADBEEF.to_bytes(8, "little")

deadbeef = DATA.find(deadbeef_bytes)
if (deadbeef == -1):
    raise ValueError("MAGIC size was not found!")
encoded_size = size.to_bytes(8, "little")
NEW_DATA = DATA.replace(deadbeef_bytes, encoded_size)

file = open("saltysd_core.elf", "wb")
file.write(NEW_DATA)
file.close()
