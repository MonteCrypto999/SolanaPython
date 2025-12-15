# Solana Module Example
# Access slot, epoch, and CPI functions

import solana

# Get current slot
slot = solana.slot()
print('slot =')
print(slot)

# Get current epoch
epoch = solana.epoch()
print('epoch =')
print(epoch)

# CPI example (requires target program and accounts)
# result = solana.cpi(program_idx, [account_indices], data_bytes)
# Returns 0 on success, error code otherwise
