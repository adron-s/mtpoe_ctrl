#ifndef _MAIN
#define _MAIN

//файл для общения с spidev модулем
#define DEFAULT_DEV_FILE "/dev/spidev0.2"

int need_exit = 0; //еще не пора выходить

/* переменные со значениями опций.
   не забывай при добавлении новой переменной так же вписывать
   ее в opt_X enum и my_options
*/

int dumpvars = 0; //вывод значений переменных
char action[255] = "noop"; //действие(add, del, ...)
char dev_file[30] = DEFAULT_DEV_FILE; //файл для общения с spidev модулем
int period = 1; //период повторения главного цикла
int verbose = 0; //быть более разговорчивым

int single = 1; //запрос о выводе данных касается единичной команды или их множество
char *err_descr = NULL; //подробное описание произошедшей ошибки
int scop = 1; //уже была напечатана открывающаяся скобка

//структура с описанием нашей опции
struct my_option{
  const char *name;
  int id;
  char type;
  void* ptr;
  int l_min;
  int l_max;
};

typedef void (*my_action_cb)(void);

//структура с описанием значения опции action
struct my_action_opt{
  const char *name;
  my_action_cb cb;
};

/* имена опций на которые реагирует программа */
enum {
  opt_dumpvars,
  opt_action,
  opt_dev_file,
  opt_period,
  opt_verbose,
  opt_MAX
};

#include "params.h"

//список известных нам опций
const struct my_option my_options[] = {
  define_flag_opt(dumpvars),
  define_str_opt(action),
  define_str_opt(dev_file),
  define_int_opt(period, 0, 3600),
  define_flag_opt(verbose),
  define_empty_opt()
};

#endif /* _MAIN */
