#ifndef _MK_COM
#define _MK_COM

/* команды для ATtiny POE V2 */
#define POE_CMD_FW_VER 0x41 /* запрос версии прошивки */
#define POE_CMD_INP_VOLT 0x42 /* запрос входного напряжения */
#define POE_CMD_TEMPERAT 0x43 /* запрос температуры */
#define POE_CMD_ON_OFF 0x44 /* запрос на включение/отключение POE на указанном порту */
#define POE_CMD_STATE 0x45 /* получение статуса всех POE портов(off, force-on, auto-on) */
/* запрос статуса(есть ли потребление, КЗ) POE для указанного порта.
	 так же возвращает ток потребления в mA */
#define POE_CMD_PORT_STATE_BASE 0x50 /* 0x50 .. 0x5F */
#define POE_CMD_PORTS_MAX 16 /* Maximum number of handled ports.*/

uint8_t *spidev_query(int, uint8_t, uint8_t, uint8_t);
uint8_t *spidev_raw_query(int, uint8_t *, size_t);

#endif /* _MK_COM */
