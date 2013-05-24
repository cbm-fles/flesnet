

hdrrev  8       dpb
sysid	8       dpb
linkid  16      dpb
sysrev  8       dpb
flags	8       dpb

msnr	48/64   dpb

size	32      flib
offset  64      flib



  hdr_word_0 <= hdrrev & sysid & flags & size;
  hdr_word_1 <= rsvd & std_logic_vector(mc_nr);




header revision	internal revision specifier
		8bit = 265 formats
		
timestamp	gobble timestamp: 	64bit * 1ns = 585a
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
