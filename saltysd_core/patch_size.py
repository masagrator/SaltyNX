file = open("saltysd_core.elf", "rb")
DATA = file.read()
file.close()

file = open("size.txt", "r")
size = int(file.read(), base=10)
file.close()

deadbeef = DATA.find(b"\xEF\xBE\xAD\xDE\x00\x00\x00\x00")
if (deadbeef == -1):
    raise ValueError("MAGIC size was not found!")
encoded_size = size.to_bytes(8, "little")
NEW_DATA = DATA.replace(b"\xEF\xBE\xAD\xDE\x00\x00\x00\x00", encoded_size, deadbeef)

file = open("saltysd_core.elf", "wb")
file.write(NEW_DATA)
file.close()