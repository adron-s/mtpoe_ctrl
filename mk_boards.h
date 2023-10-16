#ifndef _MK_BOARDS
#define _MK_BOARDS

#include <stdint.h>
#include "mk_com.h"

#define BOARDS_NUM (sizeof(poe_boards)/sizeof(poe_boards[0]))
#define POE_BOARD_NAME_LEN 32
#define SPIDEV_MAX_LEN 20

struct poe_board {
	char name[POE_BOARD_NAME_LEN];
	int proto_ver;
	char spidev[SPIDEV_MAX_LEN];
	int ports_num;
	int port_state_map[POE_CMD_PORTS_MAX];
};

/*************************************************************************************
	все изместные нам PoE устройства */
const struct poe_board poe_boards[ ] = {
	{ /* RouterBOARD 750P r2, RouterBOARD 750UP r2*/
		.name = "rb-750p-pbr2",
		.proto_ver = 2,
		.spidev = "/dev/spidev0.2",
		.ports_num = 4,
		.port_state_map = {0xd, 0xc, 0xb, 0xa}
	}, { /* RouterBOARD 960PGS */
		.name = "mikrotik,routerboard-960pgs",
		.proto_ver = 3,
		.spidev = "/dev/spidev0.2",
		.ports_num = 4,
		.port_state_map = {0xd, 0xc, 0xb, 0xa}
	}, { /* RouterBOARD RB5009UPr+S+IN */
		.name = "mikrotik,rb5009",
		.proto_ver = 4,
		.spidev = "/dev/spidev2.0",
		.ports_num = 8,
		.port_state_map = {0x8, 0x7, 0x6, 0x5, 0x4, 0xb, 0xa, 0x9}
	}
};//----------------------------------------------------------------------------------

#endif
