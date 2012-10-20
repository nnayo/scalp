#include "fr_cmdes.h"


u8 frame_set_0(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde)
{
	fr->dest = dest;
	fr->orig = orig;
	fr->cmde = cmde;
	fr->status = 0;

	return OK;
}


u8 frame_set_1(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 argv0)
{
	fr->dest = dest;
	fr->orig = orig;
	fr->cmde = cmde;
	fr->status = 0;
	fr->argv[0] = argv0;

	return OK;
}


u8 frame_set_2(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 argv0, u8 argv1)
{
	fr->dest = dest;
	fr->orig = orig;
	fr->cmde = cmde;
	fr->status = 0;
	fr->argv[0] = argv0;
	fr->argv[1] = argv1;

	return OK;
}


u8 frame_set_3(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 argv0, u8 argv1, u8 argv2)
{
	fr->dest = dest;
	fr->orig = orig;
	fr->cmde = cmde;
	fr->status = 0;
	fr->argv[0] = argv0;
	fr->argv[1] = argv1;
	fr->argv[2] = argv2;

	return OK;
}


u8 frame_set_4(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 argv0, u8 argv1, u8 argv2, u8 argv3)
{
	fr->dest = dest;
	fr->orig = orig;
	fr->cmde = cmde;
	fr->status = 0;
	fr->argv[0] = argv0;
	fr->argv[1] = argv1;
	fr->argv[2] = argv2;
	fr->argv[3] = argv3;

	return OK;
}


u8 frame_set_5(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 argv0, u8 argv1, u8 argv2, u8 argv3, u8 argv4)
{
	fr->dest = dest;
	fr->orig = orig;
	fr->cmde = cmde;
	fr->status = 0;
	fr->argv[0] = argv0;
	fr->argv[1] = argv1;
	fr->argv[2] = argv2;
	fr->argv[3] = argv3;
	fr->argv[4] = argv4;

	return OK;
}


