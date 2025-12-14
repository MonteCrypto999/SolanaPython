# VFS Write Example
# Write data to a Solana account (requires writable account)

f = open("/sol/0", "w")
f.write("Hello Solana!")
f.close()

# Verify by reading back
f = open("/sol/0", "r")
print(f.read())
f.close()
