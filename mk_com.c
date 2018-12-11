#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <errno.h>

/* экспорт ф-й и переменных из mtpoe_ctrl.c */
void die(int code);
extern char *err_descr; //подробное описание произошедшей ошибки
extern int verbose; //быть более разговорчивым
extern char err_mess[255];
extern int poe_proto;

/*************************************************************************************
  вызывается при ошибке чтобы прекратить выполнение программы
*/
static void pabort(char *ed){
	static char eed[255];
	snprintf(eed, sizeof(eed), "%s: %s", ed, strerror(errno));
	err_descr = eed;
	die(-100);
}//-----------------------------------------------------------------------------------

/* умереть и выдать осмысленное сообщение о причине смерти */
#define die_and_mess(code, mess, args...){						\
	snprintf(err_mess, sizeof(err_mess), mess, ##args);	\
	err_descr = err_mess;																\
	die(code);																					\
}

/*************************************************************************************
  считает 8 битную контрольную сумму по алгоритму CRC-8 Dallas/Maxim
*/
static unsigned char dallas_crc8(const unsigned char *data, const unsigned int size){
	unsigned char crc = 0;
	unsigned int i;
	for(i = 0; i < size; ++i){
		unsigned char inbyte = data[i];
		unsigned char j;
		for(j = 0; j < 8; ++j){
			unsigned char mix = (crc ^ inbyte) & 0x01;
			crc >>= 1;
			if(mix) crc ^= 0x8C;
			inbyte >>= 1;
		}
	}
	return crc;
}//-----------------------------------------------------------------------------------

static uint32_t mode = SPI_MODE_0; //режим работы spi шины(их всего 4-ре)
static uint8_t bits = 8; //8 bites per word
static uint32_t speed = 8000; //частота. т.к. контроллеру нужно время на подумать то частота 8kHz

static uint8_t tx[10];
#define iobuf_len (sizeof(tx) / sizeof(tx[0]))
static uint8_t rx[iobuf_len];


/*************************************************************************************
  инициализирует(единожды) fd для ioctl-запросов к spidev
*/
static void spidev_init(int fd){
	int ret;
	static int fd_need_init = 1;
	if(!fd_need_init)
		return;
	/* spi mode */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if(ret == -1)
		pabort("can't set spi mode");
	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if(ret == -1)
		pabort("can't get spi mode");
	/* bits per word */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if(ret == -1)
		pabort("can't set bits per word");
	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if(ret == -1)
		pabort("can't get bits per word");
	/* max speed hz */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if(ret == -1)
		pabort("can't set max speed hz");
	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if(ret == -1)
		pabort("can't get max speed hz");
	if(verbose){
		fprintf(stderr, "spi mode: %d\n", mode);
		fprintf(stderr, "bits per word: %d\n", bits);
		fprintf(stderr, "max speed: %d Hz (%d KHz)\n\n", speed, speed / 1000);
	}
	fd_need_init = 0;
}//-----------------------------------------------------------------------------------

/* печатает tx или rx буфер если включен режим verbose */
#define dump_buf_if_verb(bs, len){		 \
	if(verbose){												 \
		int a;														 \
		fprintf(stderr, "%s: ", #bs);			 \
		for(a = 0; a < len; a++)		 			 \
			fprintf(stderr, "%.2X ", bs[a]); \
		fprintf(stderr, "\n");						 \
	}																		 \
}

/*************************************************************************************
  выполняет ioctl запрос к spidev драйверу и проверяет полученный ответ(crc, cmd num, etc...)
*/
uint8_t *spidev_query(int fd, uint8_t cmd, uint8_t arg1, uint8_t arg2){
	int ret;
	const int max_retry_count = 2; //сколько retry пытаться делать
	uint8_t tx_crc;
	uint8_t rx_crc;
	uint8_t *p;
	int a;
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = iobuf_len,
		.delay_usecs = 100, //задержка с переключением CS после окончания запроса
		.speed_hz = speed,
		.bits_per_word = bits,
	};
	//выполним инит spidev fd
	spidev_init(fd);
	//пытамся делать Retry в случае ошибки
	for(a = 0; ; a++){
		//подготовим буферы
		memset(tx, 0x0, sizeof(tx));
		memset(rx, 0x0, sizeof(rx));
		p = tx;
		*(p++) = cmd; //номер команды
		*(p++) = arg1; //arg1
		*(p++) = arg2; //arg2
		tx_crc = dallas_crc8((void*)tx, 3); //crc-8
		*(p++) = tx_crc;
		dump_buf_if_verb(tx, iobuf_len);
		//выполняем ioctl запрос
		ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
		dump_buf_if_verb(rx, iobuf_len);
		if(ret < 1){
			if(a < max_retry_count) continue; //try to retry
			pabort("can't send spi message");
		}
		if(ret != iobuf_len){
			if(a < max_retry_count) continue; //try to retry
			die_and_mess(-100, "expected ansver len != ret len: %zu vs %d",
				iobuf_len, ret);
		}
		/* проверим то, что получили от микроконтроллера */
		p = rx + 4;	//p указывает на начало ответа(+4 байта)
		if(poe_proto == 3)
			tx_crc = 0xFF;
		//проверим что rx[0] == tx_crc
		if(*(p++) != tx_crc){
			if(a < max_retry_count) continue; //try to retry
			die_and_mess(-101, "Tx CRC error: 0x%x vs 0x%x", *(p - 1), tx_crc);
		}
		//проверим что rx[1] == номер команды
		if(*p != cmd){
			if(a < max_retry_count) continue; //try to retry
			die_and_mess(-102, "Cmd num error!: 0x%x vs 0x%x", *p, cmd);
		}
		//проверим rx_crc(p указывает на байт с номером команды)
		rx_crc = dallas_crc8((void*)(p++), 3);
		//rx_crc вопторяется два раза!(p указыет на байт полезных данных)
		if(rx_crc != p[2] || rx_crc != p[3]){
			if(a < max_retry_count) continue; //try to retry
			die_and_mess(-101, "Rx CRC error!: 0x%x vs 0x%x, 0x%x", rx_crc, p[2], p[3]);
		}
		break;
	}
	//передаем полезные данные(2 байта) ответа от микроконтроллера
	return p;
}//-----------------------------------------------------------------------------------

/*************************************************************************************
  выполняет ioctl запрос к spidev драйверу чтобы отправить $len @tx_raw_data байт.
  используется для отладки. не забывай освобождать память буфера результата.
*/
uint8_t *spidev_raw_query(int fd, uint8_t *tx_raw_data, size_t len){
	int ret;
	uint8_t *rx_ansv = malloc(len);
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx_raw_data,
		.rx_buf = (unsigned long)rx_ansv,
		.len = len,
		.delay_usecs = 100, //задержка с переключением CS после окончания запроса
		.speed_hz = speed,
		.bits_per_word = bits,
	};
	if(!rx_ansv){
		return NULL;
	}
	dump_buf_if_verb(tx_raw_data, len);
	//выполним инит spidev fd
	spidev_init(fd);
	//подготовим буфер для rx
	memset(rx_ansv, 0x0, len);
	//выполняем ioctl запрос
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	dump_buf_if_verb(rx_ansv, len);
	if(ret < 1){
		free(rx_ansv);
		return NULL;
	}
	return rx_ansv; //не забывай освобождать память !
}//-----------------------------------------------------------------------------------
