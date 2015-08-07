/*
 * ServerUtil.cpp
 *
 *  Created on: Jun 5, 2015
 *      Author: jimm
 */

#include "server_util.h"
#include "common/common_api.h"

string CServerUtil::MakeFixedLengthRandomString(uint8_t nLength)
{
	static char arrCharTable[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
								  't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
								  'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '`', '1', '2', '3', '4',
								  '5', '6', '7', '8', '9', '0', '-', '~', '!', '=', '@', '#', '$', '%', '^', '&', '*', '(', ')',
								  '_', '+', '[', ']', '{', '}', '|', ';', ':', ',', '.', '/', '?'};

	uint8_t nCharTableSize = sizeof(arrCharTable);
	char arrFixedBuf[nLength + 1];
	for(uint8_t i = 0; i < nLength; ++i)
	{
		arrFixedBuf[i] = arrCharTable[Random(nCharTableSize)];
	}
	arrFixedBuf[nLength] = '\0';

	return string(arrFixedBuf);
}
