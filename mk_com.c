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

void die(int code);
extern char *err_descr; //подробное описание произошедшей ошибки
extern int verbose; //быть более разговорчивым

/*************************************************************************************
  вызывается при ошибке чтобы прекратить выполнение программы
*/
static void pabort(char *ed){
	static char eed[255];
	snprintf(eed, sizeof(eed), "%s: %s", ed, strerror(errno));
	err_descr = eed;
	die(-100);
}//-----------------------------------------------------------------------------------

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
	if (ret == -1)
		pabort("can't set spi mode");
	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");
	/* bits per word */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");
	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");
	/* max speed hz */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");
	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");
	if(verbose){
		fprintf(stderr, "spi mode: %d\n", mode);
		fprintf(stderr, "bits per word: %d\n", bits);
		fprintf(stderr, "max speed: %d Hz (%d KHz)\n\n", speed, speed / 1000);
	}
	fd_need_init = 0;
}//-----------------------------------------------------------------------------------

#define dump_buf_if_verb(bs){					 \
	if(verbose){												 \
		int a;														 \
		fprintf(stderr, "%s: ", #bs);			 \
		for(a = 0; a < iobuf_len; a++)		 \
			fprintf(stderr, "%.2X ", bs[a]); \
		fprintf(stderr, "\n");						 \
	}																		 \
}

/*************************************************************************************
  выполняет ioctl запрос к spidev драйверу и проверяет полученный ответ(crc, cmd num, etc...)
*/
uint8_t *spidev_query(int fd, uint8_t cmd, uint8_t arg1, uint8_t arg2){
	int ret;
	uint8_t tx_crc;
	uint8_t rx_crc;
	char tmp[255];
	uint8_t *p;
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
	//подготовим байты в буфере tx
	memset(tx, 0x0, sizeof(tx));
	memset(rx, 0x0, sizeof(rx));
	p = tx;
	*(p++) = cmd; //номер команды
	*(p++) = arg1; //arg1
	*(p++) = arg2; //arg2
	tx_crc = dallas_crc8((void*)tx, 3); //crc-8
	*(p++) = tx_crc;
	dump_buf_if_verb(tx);
	//выполняем ioctl запрос
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	dump_buf_if_verb(rx);
	if (ret < 1)
		pabort("can't send spi message");
	if(ret != iobuf_len){
		snprintf(tmp, sizeof(tmp), "expected ansver len != ret len: %zu vs %d",
			iobuf_len, ret);
		pabort(tmp);
	}
	/* проверим то что получили от микроконтроллера */
	p = rx + 4;	//p указывает на начало ответа(+4 байта)
	//проверим что rx[0] == tx_crc
	if(*(p++) != tx_crc)
		pabort("Tx CRC error!");
	//проверим что rx[1] == номер команды
	if(*p != cmd)
		pabort("Cmd num error!");
	//проверим rx_crc(p указывает на байт с номером команды)
	rx_crc = dallas_crc8((void*)(p++), 3);
	//rx_crc вопторяется два раза!(p указыет на байт полезных данных)
	if(rx_crc != p[2] || rx_crc != p[3]){
		pabort("Rx CRC error!");
	}
	//передаем полезные данные(2 байта) ответа от микроконтроллера
	return p;
}//-----------------------------------------------------------------------------------
