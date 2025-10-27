# CPI to System Program: Allocate space
# Account 0: Account to allocate (signer, writable)
# Account 1: System Program
# Allocate instruction: discriminator=8 (4 bytes LE) + space (8 bytes LE)
# Space: 256 bytes = 0x100
space = 256
data = b"\x08\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00"
cpi(1, [(0,1,1)], data)
