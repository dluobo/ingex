// Recording.cpp

#include "Recording.h"

/*
const char * Recording::TypeText(Recording::EnumeratedType type)
{
	switch(type)
	{
	case Recording::TAPE:
		return "TAPE";
		break;
	case Recording::FILE:
		return "FILE";
		break;
	default:
		return "";
		break;
	}
}
*/

const char * Recording::FormatText(Recording::EnumeratedFormat fmt)
{
	switch(fmt)
	{
	case Recording::TAPE:
		return "TAPE";
		break;
	case Recording::FILE:
		return "FILE";
		break;
	case Recording::DV:
		return "DV";
		break;
	case Recording::SDI:
		return "SDI";
		break;
	default:
		return "";
		break;
	}
}

