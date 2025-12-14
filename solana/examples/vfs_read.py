# VFS Read Example
# Read data from a Solana account (works with read-only accounts)

f = open("/sol/0", "r")
data = f.read()
print(data)
f.close()
