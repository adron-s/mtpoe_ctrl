#ifndef _SIGNALS
#define _SIGNALS
//набюор функция установки обработчика сигналов выхода для процесса

extern int need_exit; //флаг того что пора выходить

//**************************************************
//вызывается когда мы получаем сигнал kill
static void sig_kill_handler(int sig_number){
//  printf("Received kill signal. Begin exiting...\n");
  need_exit=1;
}//-------------------------------------------------

//**************************************************
//выполняет установку обработчика sig_kill_handler
//для сигналов INT и TERM
static inline void setup_sig_kill_handler(){
  struct sigaction sa; //структура нужна для отлова сигналов
  //инитим структуру отлова сигналов 
  memset(&sa, 0, sizeof(sa));
  //уснатавливаем обработчик
  sa.sa_handler=&sig_kill_handler;
  //говорим на какие сигналы нужно реагировать
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
}//-------------------------------------------------

#endif /* _SIGNALS */
