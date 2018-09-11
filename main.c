#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdint.h>
#include <linux/types.h>
#include <signal.h>
#include "signals.h"
#include "main.h"

int spidev_fd = -1;

uint8_t *spidev_query(int, uint8_t, uint8_t, uint8_t);

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
  	}
  	else{
  		if(!scop) printf("{\n");
  		printf("  error: %d\n", code);
			printf("}\n");
	  }
  }
  exit(code);
}//-----------------------------------------------------------------------------------

/* макросы для упрощения синтаксиса простых action функций */
#define scobs(code) { 									 							\
	if(single > 0){ printf("{\n"); scop = 1; } 					\
	code;																	 							\
	if(single > 0) printf("}\n");							 					\
}
#define do_sq(cmd, code)	{								 						\
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
void parse_options(int argc, char *argv[]){
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
  выводит данные о номере версии прошивки poe микроконтроллера
*/
void do_action_get_fw_ver(){
	do_sq(0x41, {
		printf("  %s: %d.%02d%s\n", "fw_version", ansv[0], ansv[1], need_coma());
	});
}//-----------------------------------------------------------------------------------

/*************************************************************************************
  выводит данные о входном напряжении на устройстве
*/
void do_action_get_voltage(){
	do_sq(0x42, {
		float v = (x * 35.7 / 1024);
		printf("  %s: %.2f%s\n", "voltage", v, need_coma());
	});
}//-----------------------------------------------------------------------------------

/*************************************************************************************
  выводит данные о температуре на устройстве
*/
void do_action_get_temperature(){
	do_sq(0x43, {
		int c = x - 0x113; //зависимость линейная. 0C это 0x113
		printf("  %s: %d%s\n", "temperature", c, need_coma());
	});
}//-----------------------------------------------------------------------------------

/*************************************************************************************
  выводит данные о состоянии PoE(включено ли пое, на каких портах и в каком режиме)
*/
void do_action_get_poe(){
	do_sq(0x45, {
		int a;
		printf("  %s: [ ", "poe_state");
		for(a = 0; a < 4; a++){
			uint8_t ps = x & 0xF;
			x >>= 4;
			printf("%d%s ", ps, a + 1 < 4 ? "," : "");
		}
		printf("]%s\n", need_coma());
	});
}//-----------------------------------------------------------------------------------

/*************************************************************************************
  устанавливает состояние определенного PoE порта
*/
void do_action_set_poe(){
	period = 0;
	static char err_mess[255];
	if(port < 0 || port > 3){
		strcpy(err_mess, "port value must be 0..3");
		err_descr = err_mess;
		die(-20);
	}
	if(val < 0 || val > 2){
		strcpy(err_mess, "PoE value must be 0..2");
		err_descr = err_mess;
		die(-20);
	}
	{
		uint8_t *ansv = spidev_query(spidev_fd, 0x44, 4 - port, val);
		uint32_t must_be_ret = 4 - port;
		must_be_ret <<= 8;
		must_be_ret += val;
		uint32_t x = ansv[0] << 8 | ansv[1];
		if(x != must_be_ret){
			snprintf(err_mess, sizeof(err_mess), "status must be 0x%x but it 0x%x",
				must_be_ret, x);
			err_descr = err_mess;
			die(-21);
		}
		scobs({
			printf("  status: \"ok\"\n");
		});
	}
}//-----------------------------------------------------------------------------------

/*************************************************************************************
  получает все данные о состоянии poe и выводит их в виде json
*/
void do_action_info(){
	single = 0; //команд выводящих данныхз будет много
	{ printf("{\n"); scop = 1; }
	do_action_get_fw_ver();
	do_action_get_voltage();
	do_action_get_temperature();
	single = -100; //последняя выводимая переменная. запятая не нужна.
	do_action_get_poe();
	printf("}\n");
}//-----------------------------------------------------------------------------------

//список известных нам значенией(X) для --action=X
const struct my_action_opt my_actions[] = {
  define_my_action(info), //вывод инфо данных
  define_my_action(get_fw_ver), //вывод версии прошивки
  define_my_action(get_voltage), //вывод входного напряжения
  define_my_action(get_temperature), //вывод температуры
  define_my_action(get_poe), //вывод состояния PoE для портов
  define_my_action(set_poe), //устанавливает состояние force-on для указанного PoE порта
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
void do_action(){
  my_action_react(my_action->cb());
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
  //проверим что был передан параметр action. и если он не был передан то:
  add_default_if_not_set(action, "info"); //используем значение по умолчанию action="info"
  if(oisp(dumpvars)){ //если была передана опция отладка
    print_opts_vars_info(my_options); //напечатает отладочную информацию
    die(0); //и выходим
  }
  //открываем файл на чтение(этого вполне достаточно для ioctl-я)
  if((spidev_fd = open(dev_file, O_RDONLY)) < 0){
    fprintf(stderr, "Can't open device %s\n", dev_file);
    die(-1);
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
