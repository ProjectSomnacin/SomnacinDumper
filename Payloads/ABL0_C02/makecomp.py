import zlib
import sys
import os

tgt_uncomp=0xF10
tgt_comp  =0x232

def zlib_compress(s):
  return zlib.compress(s, 9)


if len(sys.argv) < 3:
  print("usage: %s <in> out>"%(sys.argv[0]))
  exit(1)

infile = sys.argv[1]
outfile = sys.argv[2]
print("infile=%s"%(infile))
print("outfile=%s"%(outfile))

with open(infile,"rb") as f:
  r = f.read()
origlen = len(r)

if len(r) < tgt_uncomp:
  diff = tgt_uncomp - len(r)
  rand=os.urandom(diff)
  r+=rand

print("uncom=0x%08x"%(len(r)))
if len(r) > tgt_uncomp:
  print("uncompressed image too large!")
  exit(1)

r = bytearray(r)


cmpr = zlib_compress(r)
for i in range(tgt_uncomp-1,origlen+1,-1):
  r[i] = 0xFF
  cmpr = zlib_compress(r)
  if len(cmpr) <=tgt_comp:
    break

print("comp =0x%08x"%(len(cmpr)))

l = len(cmpr)
if (l > tgt_comp):
  print("COMP size too large!")
  exit(1)

if (l &~0xF != tgt_comp &~0xF):
  print("COMP size bad!")
  exit(1)

tgt_comp_aligned = tgt_comp
if tgt_comp_aligned & 0x3:
  tgt_comp_aligned &= ~0x3
  tgt_comp_aligned += 0x4
while len(cmpr) < tgt_comp_aligned:
  cmpr += b"\x00"

with open(outfile,"wb") as f:
  f.write(cmpr)

print("done")