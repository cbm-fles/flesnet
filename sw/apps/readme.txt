

hdrrev  8       dpb
sysid	8       dpb
linkid  16      dpb
sysrev  8       dpb
flags	8+      dpb

msnr	48/64   dpb

size	32      flib
offset  64      flib

CRC     32   flib/dpb Footer

|        |        |        |        |        |        |        |        |
 hdr_id   hdr_ver   eq_id            flags             sys_id   sys_ver
 mc_nr
 crc                                 size
 offset




unsigned:8   hdr_id  "Header format identifier"
unsigned:8   hdr_ver "Header format version"
unsigned:16  eq_id   "Equipment identifier"
unsigned:16  flags   "Status and error flags"
unsigned:8   sys_id  "Subsystem identifier"
unsigned:8   sys_ver "Subsystem format version"
unsigned:64  idx     "Microslice index"
unsigned:32  crc     "CRC32 checksum"
unsigned:32  size
unsigned:64  offset







header revision	internal revision specifier
		8bit = 265 formats
		
timestamp	globle timestamp: 	64bit * 1ns = 585a
					48bit * 1ns = 3,3d

		header timestamp	38bit * 1us ~ 3d

FLES Link ID	ID of the incoming fles link, (filled in flip ?)
		min 11 bit = 2048 links

payload size	data size of payload, filled in flip
		min 24 bit = 16 MByte

checksum	16 bit crc -> HD4 till 32k data
		32 bit crc -> HD3 till 2^32-1

data format specifier
		specifies the format of the payload for event selection SW
		max 3 identifiers per detector group, not a revision specifier
		filled by the detector group
		8bit = 256 formats

Error flags	corrupt data
		16 bit = 16 errors ?

Flags		debug flag?
		8 bit
