# Transfer SOL using PikaPython CPI
#
# This script transfers lamports from the payer to a recipient using
# a Cross-Program Invocation (CPI) to the System Program.
#
# Required accounts (passed to the transaction):
#   Account 0: Payer (signer, writable) - the wallet funding the transfer
#   Account 1: Recipient (writable) - the destination wallet
#   Account 2: System Program (11111111111111111111111111111111)
#
# The transfer amount is 0.001 SOL (1,000,000 lamports)

import struct

# Transfer amount: 0.001 SOL = 1,000,000 lamports
lamports = 1000000

# System Program Transfer instruction format:
#   - Instruction discriminator: 2 (4 bytes, little-endian)
#   - Lamports amount (8 bytes, little-endian)
data = struct.pack('<IQ', [2, lamports])

# CPI to System Program (account index 2)
# Account metas: [(account_index, is_writable, is_signer), ...]
#   - Account 0: Payer (writable=1, signer=1)
#   - Account 1: Recipient (writable=1, signer=0)
cpi(2, [(0, 1, 1), (1, 1, 0)], data)

print('Transfer complete!')
