# CPI to System Program: Transfer lamports
# Account 0: Payer (signer, writable)
# Account 1: Recipient (writable)
# Account 2: System Program
# Transfer instruction: discriminator=2 (4 bytes LE) + lamports (8 bytes LE)
# 1000000 lamports = 0x0F4240 => LE: 40 42 0f 00 00 00 00 00
data = b"\x02\x00\x00\x00\x40\x42\x0f\x00\x00\x00\x00\x00"
cpi(2, [(0,1,1), (1,1,0)], data)
