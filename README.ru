Программа для управления микротиковским PoE V2 на базе микроконтроллера ATtiny 461a

Лицензия GNU GPL V3

Примеры использования:
	mtpoe_ctrl -dumpvars #показывает значение переменных-параметров
	mtpoe_ctrl #показывает полную информацию о работе PoE контроллера
	mtpoe_ctrl --action=get_fw_ver #выводит версию прошивки PoE микроконтроллера ATtiny
	mtpoe_ctrl --action=get_voltage #выводит входное напряжения
	mtpoe_ctrl --action=get_temperature #выводит температуру
	mtpoe_ctrl --action=get_poe #показывает состояние PoE для портов. 0->ether2, 1..2, 3->ether5
	mtpoe_ctrl --action=set_poe --port=0 --val=1 #force-on PoE для порта ether2
	mtpoe_ctrl --action=set_poe --port=1 --val=2 #auto-on PoE для порта ether3
	mtpoe_ctrl --action=set_poe --port=2 --val=1 #force-on PoE для порта ether4
	mtpoe_ctrl --action=set_poe --port=3 --val=0 #выключить PoE для порта ether5
	mtpoe_ctrl --action=load_poe_from_uci #загрузить PoE настройки портов из uci->network->mtik_poe

В /etc/config/network должна присутствовать секция mtik_poe:
config mtik_poe
	option port0 '1'
	option port1 '0'
	option port2 '1'
	option port3 '0'

Управление производится путем отправки команд по SPI шине. Посредником между userspace-ом
и ядром выспутает spidev драйвер и как следствие в mach-rb-*.c файле инита платформы необходимо
прописать примерно следующее:

/* RB 750-r2(HB) with POE v2 */
#define RB750R2_ATTINY_CS			12
#define RB750R2_ATTINY_RESET	14
static int rb750r2spi_cs_gpios[3] = {
	-ENOENT, /* NOR flash. CS автоматически управляется самим spi контроллером. */
	RB750R2SSR_CS, /* 74x164 gpio extender */
	RB750R2_ATTINY_CS, /* ATtiny PoE v2 */
};
static struct ath79_spi_platform_data rb750r2spi_data __initdata = {
	.bus_num = 0,
	.num_chipselect = 3, /* CS-ов у нас три штуки */
	.cs_gpios = rb750r2spi_cs_gpios, /* какие именно cs-ы */
};
static struct spi_board_info rb750r2spi_info[] = {
	...
		.max_speed_hz = 10000000,
		.modalias = "74x164",
		.platform_data = &rb750r2ssr_data,
	}, {
		.bus_num = 0,
		.chip_select = 2,
		.max_speed_hz = 2000000,
		.modalias = "spidev",
  }
}
static void __init rb750r2_setup(void)
{
	...
	ath79_register_spi(&rb750r2spi_data, rb750r2spi_info, ARRAY_SIZE(rb750r2spi_info));
	...
}

или если вы используете DTS:
&spi {
	spidev@2 {
		#address-cells = <1>;
		#size-cells = <1>;
		compatible = "linux,spidev";
		reg = <2>;
		spi-max-frequency = <2200000>;
	};
};
