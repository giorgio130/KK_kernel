#ifndef __LAB126_BOARD_ID_H
#define __LAB126_BOARD_ID_H

extern char mx35_board_id[];

#define IS_ADS()		(GET_BOARD_PRODUCT() == 'A')
#define IS_MARIO()		(GET_BOARD_PRODUCT() == 'M')
#define IS_TURING()		(GET_BOARD_PRODUCT() == 'T')
#define IS_NELL()		(GET_BOARD_PRODUCT() == 'N')
#define IS_TURINGWW()		(GET_BOARD_PRODUCT() == 'W')
#define IS_LUIGI()		(GET_BOARD_PRODUCT() == 'L')
#define IS_SHASTA()		(GET_BOARD_PRODUCT() == 'S')

#define IS_MARIO_PLATFORM()	\
	(IS_ADS()	||	\
	 IS_MARIO()	||	\
	 IS_NELL()	||	\
	 IS_TURING()	||	\
	 IS_TURINGWW())

#define IS_LUIGI_PLATFORM()	\
	(IS_LUIGI()	||	\
	 IS_SHASTA())

#define IS_PROTO()		(GET_BOARD_HW_BUILD() == 'X')
#define IS_EVT()		(GET_BOARD_HW_BUILD() == 'E')
#define IS_DVT()		(GET_BOARD_HW_BUILD() == 'D')
#define IS_PVT()		(GET_BOARD_HW_BUILD() == 'P')

#define GET_BOARD_HW_VERSION()	(mx35_board_id[2] - '0')

#define HAS_128M()		(GET_BOARD_MEMORY() == 'A' || GET_BOARD_MEMORY() == 'C')
#define HAS_256M()		(GET_BOARD_MEMORY() == 'B' || GET_BOARD_MEMORY() == 'D')

#define GET_BOARD_PRODUCT()	(mx35_board_id[0])
#define GET_BOARD_HW_BUILD()	(mx35_board_id[1])
#define GET_BOARD_MEMORY()	(mx35_board_id[3])

#endif
