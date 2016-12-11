#include "crc32.h"

static const uint32_t crctable_4bits[]={
	0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
	0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C,
};
uint32_t crc32_update(arrayview<uint8_t> data, uint32_t crc)
{
	const uint8_t* ptr = data.ptr();
	size_t len = data.size();
	crc = ~crc;
	for (size_t i=0;i<len;i++)
	{
		crc = crctable_4bits[(crc^ ptr[i]    )&0x0F] ^ (crc>>4);
		crc = crctable_4bits[(crc^(ptr[i]>>4))&0x0F] ^ (crc>>4);
	}
	return ~crc;
}

uint32_t crc32(arrayview<uint8_t> data)
{
	return crc32_update(data, 0);
}
