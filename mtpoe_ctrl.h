#ifndef _MTPOE_CTRL
#define _MTPOE_CTRL

#include "mk_boards.h"

//файл для общения с spidev модулем
#define DEFAULT_DEV_FILE "/dev/spidev0.2"
//ключ субконфига uci->network в котором находятся настройки PoE
#define MTIK_POE_UCI_CONFIG_KEY "mtik_poe"
//сколько всего PoE портов
#define POE_PORTS_N ((board < BOARDS_NUM) ? poe_boards[board].ports_num : 0)
//где находится файл с board_name
#define BOARD_NAME_FILE "/tmp/sysinfo/board_name"

int need_exit = 0; //еще не пора выходить
int spidev_fd = -1; //дескриптор spidev файла

int single = 1; //запрос о выводе данных касается единичной команды или их множество
char *err_descr = NULL; //подробное описание произошедшей ошибки
int scop = 0; //уже была напечатана открывающаяся скобка

/* переменные со значениями опций.
   не забывай при добавлении новой переменной так же вписывать
   ее в opt_X enum и my_options
*/
int dumpvars = 0; //вывод значений переменных
char action[255] = "noop"; //действие(add, del, ...)
char dev_file[32] = DEFAULT_DEV_FILE; //файл для общения с spidev модулем
//ключ субконфига uci->network в котором находятся настройки PoE
char poe_uci_config_key[32] = MTIK_POE_UCI_CONFIG_KEY;
int period = 0; //период повторения главного цикла
int verbose = 0; //быть более разговорчивым
int port = -1; //номер порта. используется для set_poe
int val = -1; //значение. например 0, 1 или 2 для set_poe
int version = 0; //нужно показать версию и выйти
int poe_proto = 0; //версия протокола пое(v2, v3, v4). 0 - auto.
int board = 0; //board index from the list, 0 - auto
char raw_hex_val[64] = "00 00 00 00"; //сырые данные в hex формате(в виде строки)

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
  opt_poe_uci_config_key,
  opt_period,
  opt_verbose,
  opt_port,
  opt_val,
  opt_version,
  opt_raw_hex_val,
  opt_poe_proto,
  opt_board,
  opt_MAX
};

#include "params.h"

//список известных нам опций
const struct my_option my_options[] = {
  define_flag_opt(dumpvars),
  define_str_opt(action),
  define_str_opt(dev_file),
  define_str_opt(poe_uci_config_key),
  define_int_opt(period, 0, 3600),
  define_flag_opt(verbose),
  define_int_opt(port, 0, POE_CMD_PORTS_MAX),
  define_int_opt(val, 0, 0xFFFF),
  define_flag_opt(version),
  define_str_opt(raw_hex_val),
  define_int_opt(poe_proto, 0, 0xFF),
  define_int_opt(board, 0, BOARDS_NUM),
  define_empty_opt()
};

#endif /* _MTPOE_CTRL */
