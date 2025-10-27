# CPI to System Program: Create Account
# Account 0: Payer (signer, writable)
# Account 1: New account (signer, writable)
# Account 2: System Program
# CreateAccount: discriminator=0, lamports(8), space(8), owner(32)
# Lamports: 1000000 (0x0F4240)
# Space: 100 bytes (0x64)
# Owner: Token Program 11111111111111111111111111111111
lamports = b"\x40\x42\x0f\x00\x00\x00\x00\x00"
space = b"\x64\x00\x00\x00\x00\x00\x00\x00"
owner = b"\x00" * 32
data = b"\x00\x00\x00\x00" + lamports + space + owner
cpi(2, [(0,1,1), (1,1,1)], data)
