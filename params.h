#ifndef _PARAMS_OPTIONS
#define _PARAMS_OPTIONS

/* общие ф-и для работы с переданными параметрами.
   в программе должны быть объявлены:
     my_options - массив описывающий опции которые понимает программа
     enum с именами опций и оканчивающийся opt_MAX
*/

//прототип функции помирания. она должна быть объявлена в основной программе.
void die(int);

/*************************************************************************************
  объявляет массив long_options заполняя его значениями из my_options
  long_options нужен для работы ф-и getopt_long_only.
*/
#define define_long_options_from_my_options(my_options)                          \
  struct option long_options[sizeof(my_options) / sizeof(my_options[0])];        \
  {                                                                              \
    struct my_option *my_opt = (void*)my_options;                                \
    struct option *long_opt = (void*)long_options;                               \
    while(my_opt->name){                                                         \
      long_opt->name = my_opt->name;                                             \
      long_opt->has_arg = my_opt->type == 'f' ? no_argument : required_argument; \
      long_opt->flag = NULL;                                                     \
      long_opt->val = my_opt->id;                                                \
      my_opt++;                                                                  \
      long_opt++;                                                                \
    }                                                                            \
    memset((void*)long_opt, 0x0, sizeof(*long_opt));                             \
  }
//------------------------------------------------------------------------------------

//макросы для более простого объявления списка известных нам опций
#define define_flag_opt(name) \
  { #name, opt_##name, 'f', &name, 0, 0 }
#define define_str_opt(name) \
  { #name, opt_##name, 's', &name, 0, sizeof(name) - 1 }
#define define_int_opt(name, l_min, l_max) \
  { #name, opt_##name, 'i', &name, l_min, l_max }
#define define_empty_opt() { NULL, 0, '\0', NULL, 0, 0 }
//макросы для более простого объявления списка известных нам my_actions
#define define_my_action(name) { #name, do_action_##name }

//макрос для безопасного копирования одной строки в другую
#define safe_str_copy(to, from)              \
  strncpy(to, from, sizeof(to));             \
  to[sizeof(to) - 1] = 0;

//макрос для отработки ошибок
#define errret(rc) ret = rc; goto err;

/*************************************************************************************
  static string printf. вернет указатель на свою статическую строку
*/
static inline char *sprf(const char *format, ...){
  static char buf[255];
  va_list args;
  va_start (args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end (args); //освобождаем ресурсы выделенные через va_start
  return buf;
}//-----------------------------------------------------------------------------------

/*************************************************************************************
  вернет 1 если в строке только цифры
*/
static inline int __only_digits(char *str){
  int a;
  if(*str == '\0') return 0; //строка пустая !
  for(a = 0; a < strlen(str); a++){
    if(str[a] < '0' || str[a] > '9')
      return 0; //найдена не цифра
  }
  return 1; //если мы дожили до сюда то в str одни цифры
}//-----------------------------------------------------------------------------------
//только положительные числа
#define only_digits(str) __only_digits(str)
//разрешен знак - перед числом
#define only_digits_and_neg(str)({ *str == '-' ? __only_digits(str + 1) : only_digits(str); })

//массив для контроля какие опции были переданы
unsigned char passed_options[opt_MAX];
/* проверяет не выходит ли индекс i за пределы массива passed_options.
   если выходит то вернет true */
#define check_for_passed_options_ranges(i) ( i < 0 || i >= opt_MAX)
//вернет 1 если опция с указанным именем была передана и 0 в противном случае
#define oisp(name) (check_for_passed_options_ranges(opt_##name) ? 0 : passed_options[opt_##name])

/*************************************************************************************
  вернет номер опции из массива my_options по ее имени(например debug, num, ...)
*/
#define get_opt_by_name(opt_name)({                                                  \
  int ret = -1;                                                                      \
  const struct my_option *opt = my_options;                                          \
  while(opt->name){                                                                  \
    if(!strcmp(opt->name, opt_name)){                                                \
      ret = opt->id;                                                                 \
      break;                                                                         \
    }                                                                                \
    /* printf("opt = %s\n", opt->name); */                                           \
    opt++;                                                                           \
  }                                                                                  \
  ret;                                                                               \
})//----------------------------------------------------------------------------------

/*************************************************************************************
  проверяет что эта опция известна нам и в случае ошибки умирает с кодом ошибки = -4
*/
#define check_for_unknown_option_and_die(opt_num)                                        \
  /* проверка на выход за допустимые границы(что такая опция нам известна) */            \
  if(check_for_passed_options_ranges(opt_num)){                                          \
    fprintf(stderr, "Logic bug! Unknown option id := '%d'\n", opt_num);                  \
    die(-4);                                                                             \
  }                                                                                      \
//------------------------------------------------------------------------------------

/*************************************************************************************
  узнает и присваивает переменной opt_num номер переданной опции а так же
*/
#define get_opt_num_and_out_of_range_check(opt_name)({                                   \
  int opt_num = get_opt_by_name(opt_name); /* выясняем int номер опции по ее имени */    \
  /* проверка на выход за допустимые границы(что такая опция нам известна) */            \
  check_for_unknown_option_and_die(opt_num);                                             \
  opt_num;                                                                               \
})//----------------------------------------------------------------------------------

/*************************************************************************************
  проверяет все параметры из списка на то что они были переданы как аргументы командной
  строки. если какой то параметр не был передан - умирает выдав поясняющее сообщение.
*/
#define check_for_needed_params(args...){                                                \
  /* хитрость как от списка агрументов макроса перейти к массиву для цикла */            \
  char* opt_list[] = { args };                                                           \
  int a;                                                                                 \
  int opt_num;                                                                           \
  for(a = 0; a < sizeof(opt_list) / sizeof(typeof(opt_list[0])); a++){                   \
    /* выясняем int номер опции по ее имени и проверяет что такая опция нам известна */  \
    opt_num = get_opt_num_and_out_of_range_check(opt_list[a]);                           \
    /* opt_num уже проверен на предмет выхода за границы массива passed_options */       \
    if(!passed_options[opt_num]){ /* осталось проверить был ли такой параметр передан */ \
      fprintf(stderr, "No needed parameter '%s' !\n", opt_list[a]);                      \
      die(-5);                                                                           \
    }                                                                                    \
  }                                                                                      \
}//-----------------------------------------------------------------------------------

/*************************************************************************************
  проверяет был ли указанный параметр передан и если нет то вставляет его
  в список переданных используя указанное значение по умолчанию
*/
#define add_default_if_not_set(opt_name, default)({                                      \
  int ret = 0;                                                                           \
  int opt_num = get_opt_num_and_out_of_range_check(#opt_name);                           \
  if(!passed_options[opt_num]){ /* если указанная опция не была нам передана */          \
    passed_options[opt_num] = 1; /* ставим флаг что эта опция была передана */           \
    opt_set_val(opt_num, default); /* присваиваем значение по умолчанию */               \
    ret = 1;                                                                             \
  }                                                                                      \
  ret;                                                                                   \
})//----------------------------------------------------------------------------------

/*************************************************************************************
  макрос для присвоения переменной опции ее значения согласно ее(опции) типу
  в случае ошибки умирает с кодом ошибки = -2
*/
#define opt_set_val(opt_num, val){                                                   \
  check_for_unknown_option_and_die(opt_num);                                         \
  const struct my_option *my_opt = &my_options[opt_num];                             \
  switch(my_opt->type){                                                              \
    case('f'):                                                                       \
      *(int *)my_opt->ptr = 1;                                                       \
      break;                                                                         \
    case('i'):                                                                       \
      {                                                                              \
        int ival = atoi(val);                                                        \
        /* разрешаем от limL .. limH */                                              \
        if(ival < my_opt->l_min || ival > my_opt->l_max){                            \
          fprintf(stderr, "option '%s' is out of range!"                             \
                  " must be [ %d .. %d ] but you set %d\n",                          \
                  my_opt->name, my_opt->l_min, my_opt->l_max, ival);                 \
          die(-2);                                                                   \
        }                                                                            \
        *(int *)my_opt->ptr = ival;                                                  \
      }                                                                              \
      break;                                                                         \
    case('s'):                                                                       \
      {                                                                              \
        char *to = (char*)my_opt->ptr;                                               \
        strncpy(to, val, my_opt->l_max); /* l_max считается с 0 для строк */         \
        to[my_opt->l_max] = '\0';                                                    \
        if(*to == '\0'){ /* защита от пустой строки в качестве значения */           \
          fprintf(stderr, "option '%s' can't be zero length string !\n",             \
                  my_opt->name);                                                     \
          die(-2);                                                                   \
        }                                                                            \
      }                                                                              \
      break;                                                                         \
  }                                                                                  \
}//-----------------------------------------------------------------------------------

/*************************************************************************************
  печатает отладочную информацию - содержимое opts переменных. эти переменные или
  были установлены переданными опциями или остались с установленными по умолчанию
  значениями.
*/
static void print_opts_vars_info(const void *start_opt){
  const struct my_option *opt = start_opt;
  printf("opts variables status:\n");
  while(opt->name){
    switch(opt->type){
    case('f'): case('i'):
      printf("%20s := %i\n", opt->name, *(int *)opt->ptr);
      break;
    case('s'):
      printf("%20s := '%s'\n", opt->name, (char *)opt->ptr);
      break;
    default:
      fprintf(stderr, "Logic bug! UNKNOWN option type '%c'\n", opt->type);
      die(-14);
    }
    opt++;
  }
}//-----------------------------------------------------------------------------------

#endif /* _PARAMS_OPTIONS */
