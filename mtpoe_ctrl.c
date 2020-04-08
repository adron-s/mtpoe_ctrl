#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <uci.h>
#include <linux/types.h>
#include <signal.h>
#include "signals.h"
#include "mk_com.h"
#include "mtpoe_ctrl.h"

char err_mess[255];
/*************************************************************************************
	закрывает ioctl файл и умирает напечатав ошибку
*/
void die(int code){
	if(spidev_fd > 0){
		close(spidev_fd);
		spidev_fd = -1;
	}
	if(code != 0){
		if(err_descr){
			if(!scop) printf("{\n");
			printf("  error: %d,\n", code);
			printf("  err_descr: \"%s\"\n", err_descr);
			printf("}\n");
		}else{
			if(!scop) printf("{\n");
			printf("  error: %d\n", code);
			printf("}\n");
		}
	}
	exit(code);
}//-----------------------------------------------------------------------------------

/* умереть и выдать осмысленное сообщение о причине смерти */
#define die_and_mess(code, mess, args...){						\
	snprintf(err_mess, sizeof(err_mess), mess, ##args);	\
	err_descr = err_mess;																\
	die(code);																					\
}

/* макросы для упрощения синтаксиса простых action функций */
#define scobs(code){ 									 								\
	if(single > 0){ printf("{\n"); scop = 1; } 					\
	code;																	 							\
	if(single > 0) printf("}\n");							 					\
}
#define do_sq(cmd, code){								 							\
	uint8_t *ansv = spidev_query(spidev_fd, cmd, 0, 0); \
	uint32_t x = ansv[0] << 8 | ansv[1];								\
	scobs(code);																				\
}
#define need_coma() (single == 0 ? "," : "")

/*************************************************************************************
	парсит переданные опции, проверяет(на выход за допустимые границы) и помещает
	их значения в соответствующие переменные.
	! до вызова этой функции массив passed_options должен быть заполнен значением 0x0!
*/
static void parse_options(int argc, char *argv[]){
	int pc;
	define_long_options_from_my_options(my_options);
	/* парсим опции командной строки. поддерживаем только long опции но в
		 гибридной форме(то есть можно вместо --dumpvars написать -vard) */
	while((pc = getopt_long_only(argc, argv, "", long_options, NULL)) != -1){
		//загружаем значение из optarg в выделенную для этой опции переменную
		opt_set_val(pc, optarg); //этот макрос так же проверяет что pc не выходит за границы opt_MAX
		//printf("%d\n", pc);
		passed_options[pc] = 1; //ставим флаг что эта опция была передана
	}
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	выводит версию этой утилиты
*/
static void do_action_get_version(void){
	scobs({
		printf("  %s: %s\n", "version", VERSION);
	});
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	выводит данные о номере версии прошивки poe микроконтроллера
*/
static void do_action_get_fw_ver(void){
	uint8_t *ansv = spidev_query(spidev_fd, POE_CMD_FW_VER, 0, 0);
	scobs({
		printf("  %s: %d.%02d%s\n", "fw_version", ansv[0], ansv[1], need_coma());
	});
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	выводит данные о входном напряжении на устройстве
*/
static void do_action_get_voltage(void){
	do_sq(POE_CMD_INP_VOLT, {
		float v = 0;
		if(poe_proto == 2){
			v = (x * 35.7 / 1024);
		}else if(poe_proto == 3){
			v = x / 100.;
		}
		printf("  %s: %.2f%s\n", "voltage", v, need_coma());
	});
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	выводит данные о температуре на устройстве
*/
static void do_action_get_temperature(void){
	do_sq(POE_CMD_TEMPERAT, {
		int c = 0;
		//в обоих случаях зависимость линейная
		if(poe_proto == 2){
			c = x - 273; //переходим на градусы Цельсия из Кельвинов
		}else if(poe_proto == 3){
			/* 12 значений(блок) => 5 делений температуры ! */
			int n = x / 12; //номер блока
			int o = x - n * 12; //остаток - хвост после блока
			c = n * 5; //первое из значений в блоке
			c -= 273; //переходим на градусы Цельсия из Кельвинов
			/* прибавляем с учетом длины хвоста блока */
			if(o > 9) c += 4; else
			if(o > 6) c += 3; else
			if(o > 4) c += 2; else
			if(o > 2) c += 1;
		}
		printf("  %s: %d%s\n", "temperature", c, need_coma());
	});
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	возвращает массив с текущим состоянием PoE портов
*/
static uint8_t *get_poe_ports_state(void){
	int a;
	static uint8_t nps[POE_PORTS_N];
	uint8_t *ansv = spidev_query(spidev_fd, POE_CMD_STATE, 0, 0);
	uint32_t x = ansv[0] << 8 | ansv[1];
	for(a = 0; a < POE_PORTS_N; a++){
		if(poe_proto == 2){
			nps[a] = x & 0xF;
		} else if(poe_proto == 3){
			nps[POE_PORTS_N - a - 1] = x & 0xF;
		}
		x >>= 4;
	}
	return nps;
}//-----------------------------------------------------------------------------------


/*************************************************************************************
	возвращает массив с текущим потреблением(mA) PoE портов
*/
static int *get_poe_ports_cons(void){
	int a;
	static int npc[POE_PORTS_N];
	int x;
	for(a = 0; a < POE_PORTS_N; a++){
		//порты и номера команд обратны друг другу
		int cmd = POE_CMD_PORT_STATE_BASE + POE_PORTS_N - 1 - a;
		uint8_t *ansv = spidev_query(spidev_fd, cmd, 0, 0);
		x = ansv[0] << 8 | ansv[1];
		//если старший бит == 1 то порт отключен(off или КЗ или auto-on и нет нагрузки)
		if(x & 0x8000){
			if(x == 0x800A){ //если значение 0x800A то это КЗ
				x = -111;
			}else{
				x = -1; //к порту просто ничего не подключено или он off
			}
		}
		npc[a] = x;
	}
	return npc;
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	выводит данные о состоянии PoE(включено ли пое, на каких портах и в каком режиме)
*/
static void do_action_get_poe(void){
	int a;
	uint8_t *nps = get_poe_ports_state();
	int *npc = get_poe_ports_cons();
	scobs({
		printf("  %s: [ ", "poe_state");
		for(a = 0; a < POE_PORTS_N; a++){
			printf("%d%s ", nps[a], a + 1 < POE_PORTS_N ? "," : "");
		}
		printf("],\n");
		printf("  %s: [ ", "poe_cons");
		for(a = 0; a < POE_PORTS_N; a++){
			printf("%d%s ", npc[a], a + 1 < POE_PORTS_N ? "," : "");
		}
		printf("]%s\n", need_coma());
	});
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	устанавливает состояние определенного PoE порта
*/
static void __set_poe(int port, int val){
	uint8_t *ansv = spidev_query(spidev_fd, POE_CMD_ON_OFF, POE_PORTS_N - port, val);
	uint32_t must_be_ret = POE_PORTS_N - port; //что должно быть в ответе от МК
	must_be_ret <<= 8;
	must_be_ret += val;
	uint32_t x = ansv[0] << 8 | ansv[1]; //2 байта ответа
	if(x != must_be_ret){
		die_and_mess(-21, "ansv must be 0x%x but it 0x%x",
			must_be_ret, x);
	}
}//-----------------------------------------------------------------------------------
/*************************************************************************************
	action для устанавки состояние определенного PoE порта
*/
static void do_action_set_poe(void){
	if(port < 0 || port > 3)
		die_and_mess(-20, "port value must be 0..3");
	if(val < 0 || val > 2)
		die_and_mess(-20, "PoE value must be 0..2");
	__set_poe(port, val);
	scobs({
		printf("  status: \"ok\"\n");
	});
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	посредством UCI загружает состояния PoE портов
*/
static void do_action_load_poe_from_uci(void){
	struct uci_context *ctx = NULL;
	struct uci_package *p = NULL;
	struct uci_element *e;
	struct uci_section *s;
	const char *o_val;
	int a;
	int processed_ports = 0;
	//получмм текущее состояние poe портов
	uint8_t *nps = get_poe_ports_state();
	ctx = uci_alloc_context();
	if(!ctx)
		die_and_mess(-22, "Can't alloc UCI context: %s", strerror(errno));
	ctx->flags &= ~UCI_FLAG_STRICT;
	if(uci_load(ctx, "network", &p) != UCI_OK){
		uci_free_context(ctx);
		die_and_mess(-22, "Can't load UCI->network");
	}
	uci_foreach_element(&p->sections, e){
		s = uci_to_section(e);
		if(s->type && !strcmp(s->type, poe_uci_config_key)){
			for(a = 0; a < POE_PORTS_N; a++){
				char port_str[ ] = "port0";
				int val;
				port_str[4] = '0' + a;
				o_val = uci_lookup_option_string(ctx, s, port_str);
				if(!o_val)
					continue; //порт с номером $a не указан
				val = o_val[0] - '0'; //переводим в int формат
				if(val < 0 || val > 2){
					die_and_mess(-23, "PoE %s has wrong value '%s'. Must be 0..2 !",
						port_str, o_val);
					continue;
				}
				//пинаем МК только если значение PoE для порта отличается от текущего
				if(nps[a] != val){
					__set_poe(a, val);
					processed_ports++; //считаем сколько PoE портов было переконфигурено
					nps[a] = val; //запоминаем новое состояние PoE для порта $a
				}
			}
		}
	}
	uci_free_context(ctx);
	scobs({
		printf("  status: \"ok\",\n");
		printf("  processed_ports: %d,\n", processed_ports);
		printf("  %s: [ ", "poe_state");
		for(a = 0; a < POE_PORTS_N; a++){
			printf("%d%s ", nps[a], a + 1 < POE_PORTS_N ? "," : "");
		}
		printf("]%s\n", need_coma());
	});
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	получает все данные о состоянии poe и выводит их в виде json
*/
static void do_action_info(){
	single = 0; //команд выводящих данные будет много
	{ printf("{\n"); scop = 1; }
	do_action_get_fw_ver();
	do_action_get_voltage();
	do_action_get_temperature();
	single = -100; //последняя выводимая переменная. запятая не нужна.
	do_action_get_poe();
	printf("}\n");
}//-----------------------------------------------------------------------------------


/*************************************************************************************
	выполняет парсинг hex строки вида "0D 23 4C 56" возвращая ответ в виде массива байт
	не забывай освобождать память массива с результатом !!!
*/
static uint8_t *parse_hex_str(char *str, size_t *ret_len){
	char byte[3]; //для символов байта
	int a, nw;
	uint8_t *res = NULL;
	uint8_t *p = NULL;
	size_t len = 0;
	*ret_len = 0;
	/* делаем два прохода. в первом считаем размер для массива результата
		 а во втором парсим каждый найденный байт(его символы) */
	for(nw = 0; nw < 2; nw++){
		memset(byte, '\0', sizeof(byte));
		if(nw){ //если это второй проход цыкла
			if(len == 0)
				return NULL;
			res = malloc(len);
			if(!res)
				return NULL;
			p = res;
		}
		for(a = 0; a < strlen(str) + 1; a++){
			if((str[a] == ' ') || (str[a] == '\0')){
				if(byte[0] == '\0')
					continue;
				goto recogn;
			}
			if(byte[0] == '\0'){
				byte[0] = str[a];
			}else{
				byte[1] = str[a];
				goto recogn;
			}
			continue;
	recogn:
			if(nw)
				*(p++) = strtol(byte, NULL, 16);
			else
				len++;
			memset(byte, '\0', sizeof(byte));
		}
	}
	*ret_len = len;
	return res;
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	отправляет "сырую" команду состоящую из байтов заданных в raw_hex_val
*/
static void do_action_raw_send(){
	int a;
	check_for_needed_params("raw_hex_val");
	size_t tx_len = 0;
	size_t rx_len = 0;
	uint8_t *tx_data = parse_hex_str(raw_hex_val, &tx_len);
	uint8_t *rx_data = spidev_raw_query(spidev_fd, tx_data, tx_len);
	if(rx_data > 0)
		rx_len = tx_len;
	scobs({
		printf("  %s: %s,\n", "action",  "\"raw_send\"");
		printf("  %s: \"", "tx");
		for(a = 0; a < tx_len; a++)
			printf("%s0x%02X", a > 0 ? " " : "", tx_data[a]);
		printf("\",\n");
		printf("  %s: \"", "rx");
		for(a = 0; a < rx_len; a++)
			printf("%s0x%02X", a > 0 ? " " : "", rx_data[a]);
		printf("\"\n");
	});
	if(tx_data)
		free(tx_data);
	if(rx_data)
		free(rx_data);
}//-----------------------------------------------------------------------------------

//список известных нам значенией(X) для --action=X
static const struct my_action_opt my_actions[] = {
	define_my_action(info), //вывод инфо данных
	define_my_action(get_fw_ver), //вывод версии прошивки
	define_my_action(get_voltage), //вывод входного напряжения
	define_my_action(get_temperature), //вывод температуры
	define_my_action(get_poe), //вывод состояния PoE для портов
	define_my_action(set_poe), //устанавливает состояние X для указанного PoE порта
	define_my_action(load_poe_from_uci), //посредством UCI загружает и применяет состояния к PoE портам
	define_my_action(get_version), //возвращает версию этой утилиты
	define_my_action(raw_send), //отправка "сырой" команды из байтов заданных в raw_hex_val
	{ NULL, NULL }
};

/*************************************************************************************
	ищет значение переменной action среди известных my_actions и выполняет
	react в случае если поиск был успешным */
#define my_action_react(react){                           \
	const struct my_action_opt *my_action = &my_actions[0]; \
	while(my_action->name){                                 \
		if(!strcmp(action, my_action->name)){                 \
			react;                                              \
			break;                                              \
		}                                                     \
		my_action++;                                          \
	}                                                       \
	if(!my_action->name)                                    \
		fprintf(stderr, "Unknown action := '%s'\n", action);  \
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	выполняет указанное в значении параметра action действие */
static void do_action(){
	my_action_react(my_action->cb());
}//-----------------------------------------------------------------------------------

#define POE_BOARD_NAME_LEN 32
struct poe_board {
	char name[POE_BOARD_NAME_LEN];
	int ver;
};

/*************************************************************************************
	все изместные нам PoE устройства */
struct poe_board poe_boards[ ] = {
	{ /* RouterBOARD 750P r2, RouterBOARD 750UP r2*/
		.name = "rb-750p-pbr2",
		.ver = 2
	}, { /* RouterBOARD 960PGS */
		.name = "rb-960pgs",
		.ver = 3
	}
};//----------------------------------------------------------------------------------

/*************************************************************************************
	пытается по модели устройства определить версию poe протокола. */
char *try_to_detect_poe_proto(int fallback_val){
	static char res[8];
	char buf[POE_BOARD_NAME_LEN];
	int ver = fallback_val;
	int fd = -1;
	size_t len;
	fd = open(BOARD_NAME_FILE, O_RDONLY);
	if(fd > 0){
		len = read(fd, buf, sizeof(buf) - 1);
		if(len > 0){
			int a;
			buf[len] = '\0';
			if(len > 0 && buf[len - 1] == '\n')
				buf[len - 1] = '\0';
			for(a = 0; a < sizeof(poe_boards) / sizeof(poe_boards[0]); a++){
				if((strlen(buf) == strlen(poe_boards[a].name)) && !strcmp(buf, poe_boards[a].name)){
					ver = poe_boards[a].ver;
				}
			}
		}
		close(fd);
	}
	snprintf(res, sizeof(res), "%d", ver);
	return res;
}//-----------------------------------------------------------------------------------

/*************************************************************************************
	main
*/
int main(int argc, char *argv[]){
	int count = 0;
	setup_sig_kill_handler(); //устанавливаем обработчики сигналов
	//проинитим массив переданных опций
	memset(passed_options, 0x0, sizeof(passed_options));
	//парсим переданные нам опции
	parse_options(argc, argv);
	if(!poe_proto) //если протокол poe не был задан
		add_default_if_not_set(poe_proto, try_to_detect_poe_proto(0));
	if(!poe_proto){ //если версию пое так определить и не удалось
		die_and_mess(100, "Unsupported PoE device !");
		//то лучше дальше не продолжать. вдруг устройство вообще не имеет poe!
	}
	if(version) //это тоже самое что и --action="get_version"
		add_default_if_not_set(action, "get_version");
	//проверим что был передан параметр action. и если он не был передан то:
	add_default_if_not_set(action, "info"); //используем значение по умолчанию action="info"
	if(oisp(dumpvars)){ //если была передана опция отладка
		print_opts_vars_info(my_options); //напечатает отладочную информацию
		die(0); //и выходим
	}
	//открываем файл на чтение(этого вполне достаточно для ioctl-я)
	if((spidev_fd = open(dev_file, O_RDONLY)) < 0){
		die_and_mess(-1, "Can't open device %s", dev_file);
	}
	while(!need_exit){
		//если мы здесь то по крайней мере action параметр передан. выполняем указанное в action действие.
		do_action();
		if(period == 0)
	 	break;
		count++;
		sleep(1 * period);
		if(count > 0)
	 	printf("\n");
		if((count > 0) && (verbose > 0))
			fprintf(stderr, "\n");
	}
	die(0); //all done. close ioctl file end exit(0).
	return 0;
}//-----------------------------------------------------------------------------------
