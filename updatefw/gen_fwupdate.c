#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>

#define CMD "mtpoe_ctrl --action=raw_send --raw_hex_val="

int main(int argc, char *argv[]){
	int fd;
	size_t total_len = 0;
	unsigned char buf[2];
	int word_n = 0;
	int page_n = 0;
	if(argc != 2){
		printf("No needed args!\n");
		return -1;
	}
	fd = open(argv[1], O_RDONLY);
	if(fd < 0){
		printf("Can't open fd !\n");
		return -1;
	}
	printf("echo 0 > /sys/class/gpio/gpio14/value\n");
	printf("#Programming Enable\n");
	printf(CMD "\"AC 53 00 00\"\n");
	printf("#rx must be == \"0x00 0x00 0x53 0x00\"\n");
	printf("#Chip Erase\n");
	printf(CMD "\"AC 80 00 00\"\n");
	printf("sleep 10\n");
	printf("#*** Write to flash memory ***\n");
	printf("#for Attiny461: Flash size: 2K words (4K bytes), Page size: 32 words, No. of Pages: 64\n");
	printf("#Load Program Memory Page\n");
	printf("#40 000xxxxx xxxbbbbb iiiiiiii - low word byte\n");
	printf("#48 000xxxxx xxxbbbbb iiiiiiii - high word byte\n");
	printf("#Write Program Memory Page - 4C 00000aaa bbbxxxxx xxxxxxxx\n");
	do{
		memset(buf, 0x0, sizeof(buf));
		ssize_t len = read(fd, buf, sizeof(buf));
		if(len > 0){
			total_len += len;
		}else break;
		if(len != sizeof(buf)){
			printf("size mismatch ! %zd vs %zu\n", len, sizeof(buf));
		}
		/* Load Program Memory Page */
		//40 000xxxxx xxxbbbbb iiiiiiii - low word byte
		printf(CMD "\"40 00 %02X %02X\"\n", word_n & 0x1F, buf[0]);
		//48 000xxxxx xxxbbbbb iiiiiiii - high word byte
		printf(CMD "\"48 00 %02X %02X\"\n", word_n & 0x1F, buf[1]);
		word_n++;
		if(word_n == 32){
			/* Write Program Memory Page - - 4C xxxxxaaa bbbxxxxx xxxxxxxx */
			printf(CMD "\"4C %02X %02X 00\"\n", (page_n & 0x3F) >> 3,  (page_n & 0x7) << 5);
			printf("sleep 1\n");
			word_n = 0;
			page_n++;
		}
	}while(1);
	if(word_n > 0){
		//write last page
		printf(CMD "\"4C %02X %02X 00\"\n", (page_n & 0x3F) >> 3,  (page_n & 0x7) << 5);
		printf("sleep 1\n");
	}
	close(fd);
	printf("echo 1 > /sys/class/gpio/gpio14/value\n\n");
	return 0;
}
