#!/usr/bin/python3
file_name = "hbm.trace"
#3bit, 0-7
channel_range = range(0,7)
#1bit, 0-1
pseudo_channel_range = range(0,1)
#2bit, 0-3
bank_group_range = range(0,3)
#2bit, 0-3
bank_range = range(0,3)
#14bit, 0-2^14-1
row_range = [0]
#5bit, 0-2^5-1
column_range = range(0,31)
#5bit, 0-2^5-1
tx_range = [0]
#read or write
rw_range = ['R', 'W']

repeat_cnt = 1

def padhexa(s):
	return '0x' + s[2:].zfill(9)

f = open(file_name, "w")
for rp in range(0,repeat_cnt):
  for col in column_range:
    for ba in bank_range:
      for bg in bank_group_range:
        for rw in rw_range:
          for pc in pseudo_channel_range:
            for ch in channel_range:
              for row in row_range:
                for tx in tx_range:
                  addr = '{0:b}'.format(row).zfill(14)+\
                  '{0:b}'.format(ba).zfill(2)+\
                  '{0:b}'.format(bg).zfill(2)+\
                  '{0:b}'.format(pc).zfill(1)+\
                  '{0:b}'.format(col).zfill(5)+\
                  '{0:b}'.format(ch).zfill(3)+\
                  '{0:b}'.format(tx).zfill(5)
                  # print (addr)
                  addr = padhexa(hex(int(addr,2)))
                  f.write(addr+' '+rw+'\n')
f.close()