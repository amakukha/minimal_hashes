# minimal_hashes
Comparison of hashing functions with small code size.

This is full code for StackOverflow answer: https://stackoverflow.com/a/77342581/5407270

### How to run? 

```
gcc -std=c99 test_minimal_hash.c farmhash.c -O2 -o test_hash
./test_hash words_names.txt
```

## Results
Some of the best functions with small code size are **MurmurOAAT**, **RSHash**, **FNV1a** and **SDBM**. However, depending on your use case, you might want to consider using a good optimized hashing function, such as FarmHash or Murmur3.

### Collisions & Avalanching

```
                       |        Collisions           |  Avalanching
Hashing function       | random 16B |  words + names | words + names
=====================================================================
FNV1a                  |     0.20%  |     45,  0.01% |        98.0
MurmurOAAT             |     0.20%  |     50,  0.01% |       100.0
RSHash                 |     0.20%  |     51,  0.01% |        95.5
SDBM                   |     0.20%  |     53,  0.01% |        93.1
Jenkins OAAT           |     0.19%  |     66,  0.01% |       100.0
K&R v2                 |     0.20%  |    767,  0.12% |        89.8
SuperFastHash          |     0.21%  |    808,  0.13% |       100.0
DJB2                   |     0.20%  |    856,  0.13% |        89.7
XOR_rotate             |     0.19%  |   2156,  0.34% |        89.5
DEKHash                |     0.19%  |   2312,  0.36% |        88.6
Fletcher32             |     0.19%  |   5274,  0.82% |        90.9
Adler32                |    67.44%  | 371448, 58.03% |        73.9
---------------------------------------------------------------------
MurmurHash3_x86_32     |     0.19%  |     42,  0.01% |       100.0
MurmurHash3_x64_32     |     0.20%  |     50,  0.01% |       100.0
FarmHash_fingerprint32 |     0.20%  |     42,  0.01% |       100.0
FarmHash_fingerprint64 |     0.19%  |     36,  0.01% |       100.0  <-- best
CRC32                  |     0.20%  |     45,  0.01% |        99.8
```

### Speed
```
Hashing function       |                   Time, ms
                       | random 16B | random 16MB | words + names
==================================================================
FNV1a                  |     132.3  |      339.3  |         12.9
MurmurOAAT             |     170.3  |      500.7  |         11.9
RSHash                 |     122.2  |      250.5  |         10.9
SDBM                   |     116.1  |      250.8  |         11.3
Jenkins OAAT           |     159.0  |      417.8  |         11.2
K&R v2                 |     116.1  |      250.2  |         11.1
SuperFastHash          |      71.0  |      127.0  |          9.8
DJB2                   |     169.5  |      251.0  |         10.8
XOR_rotate             |     119.8  |      167.6  |         10.9
DEKHash                |     115.5  |      167.0  |         10.9
Fletcher32             |      79.0  |       52.2  |         18.5
Adler32                |     391.6  |      669.0  |         12.3
------------------------------------------------------------------
MurmurHash3_x86_32     |      93.0  |      124.3  |         10.9
MurmurHash3_x64_32     |      96.3  |       53.6  |         13.5
FarmHash_fingerprint32 |      87.2  |       52.2  |          5.4
FarmHash_fingerprint64 |      48.8  |       17.3  |          5.7  <-- fastest
CRC32 (bit-by-bit)     |    1189.6  |     2091.1  |         23.0
CRC32 (table-driven)   |     262.0  |      667.3  |         11.1
```


