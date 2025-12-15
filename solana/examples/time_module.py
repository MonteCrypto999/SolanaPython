# Time Module Example
# Uses Solana's clock sysvar

import time

# Get current unix timestamp
t = time.time()
print('time.time() =')
print(t)

# Convert to struct_time (UTC)
tm = time.gmtime(t)
print('gmtime:')
print(tm.tm_year)
print(tm.tm_mon)
print(tm.tm_mday)
print(tm.tm_hour)
print(tm.tm_min)
print(tm.tm_sec)

# Format as string
s = time.ctime(t)
print('ctime:')
print(s)

# Get current time string
a = time.asctime()
print('asctime:')
print(a)

# Convert tuple to timestamp
ts = time.mktime((2024, 1, 15, 12, 30, 0, 0, 0, 0))
print('mktime:')
print(ts)
