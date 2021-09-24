// Cerebrum Serial Extension, Helvar ltd (c)
// Main- blink led every 1000ms

#include "inc/main.h"

/* Define thread priority management */
K_FIFO_DEFINE(printk_fifo);

// UART Flags
K_SEM_DEFINE(tx_done, 0, 1);
K_SEM_DEFINE(tx_aborted, 0, 1);
K_SEM_DEFINE(rx_rdy, 0, 1);
K_SEM_DEFINE(rx_buf_released, 0, 1);
K_SEM_DEFINE(rx_disabled, 0, 1);

//Uart buffers
uint8_t rx_buf[1] = {0};
uint8_t tx_buf[1] = {1};
int redLed = 0;

// Tick LEDs
void blink(const struct led *led, uint32_t sleep_ms)
{
	const struct gpio_dt_spec *spec = &led->spec;
	int cnt = 0;
	int ret;

	if (!device_is_ready(spec->port)) {
		printk("Error: %s device is not ready\n", spec->port->name);
		return;
	}

	ret = gpio_pin_configure_dt(spec, GPIO_OUTPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure pin %d (LED '%s')\n",
			ret, spec->pin, led->gpio_pin_name);
		return;
	}

	while (1) {
		gpio_pin_set(spec->port, spec->pin, cnt % 2);
		k_msleep(sleep_ms);
		cnt++;
	}
}

//Thread functions
void blink0(void)
{
	blink(&led0, 100);
}

void blink1(void)
{
	const struct led *led = &led1;
	const struct gpio_dt_spec *spec = &led->spec;
	int ret;

	ret = gpio_pin_configure_dt(spec, GPIO_OUTPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure pin %d (LED '%s')\n",
			ret, spec->pin, led->gpio_pin_name);
		return;
	}
	gpio_pin_set(spec->port, spec->pin, redLed % 2);
	redLed++;
}

void transmit_receive(void){
	// UART DEV|BUFFER|BUFFER SIZE|TIMEOUT
	uart_rx_enable(uart2, rx_buf, sizeof(rx_buf), SYS_FOREVER_MS); //SYS_FOREVER_MS disables timeout for RX_RDY
	k_sem_take(&rx_rdy, K_MSEC(100));
	uart_tx(uart2, tx_buf, sizeof(tx_buf), SYS_FOREVER_MS);
	k_sem_take(&tx_done, K_MSEC(100));
}

//UART Priph, event, data
void uartInterrupt(const struct device *uart, struct uart_event *evt, void *user_data){
	//Handle IRQ Flags
	switch (evt->type) {
		case UART_TX_DONE:
			//Completed transfers
			k_sem_give(&tx_done);
			break;
		case UART_TX_ABORTED:
			k_sem_give(&tx_aborted);
			break;
		case UART_RX_RDY:
			// Received data so resend to test again
			k_sem_give(&rx_rdy);
			blink1();
			transmit_receive();
			break;
		case UART_RX_BUF_RELEASED:
			k_sem_give(&rx_buf_released);
			break;
		case UART_RX_DISABLED:
			k_sem_give(&rx_disabled);
			break;
		case UART_RX_BUF_REQUEST:
			k_sem_give(&rx_disabled);
			break;
		default:
			break;
		}
}

void uartTx(){
	// Define UART Devices and access to interrupt flags
	uart2 = device_get_binding(UART2);
	k_thread_access_grant(k_current_get(), &tx_done, &tx_aborted,
			      &rx_rdy, &rx_buf_released, &rx_disabled,
			      uart2);
	// Set callbacks on completion
	uart_callback_set(uart2, uartInterrupt, NULL);
	transmit_receive();
}

// Define threads to launch on runtime
K_THREAD_DEFINE(blink0_id, STACKSIZE, blink0, NULL, NULL, NULL, PRIORITY, 0, 0);
//K_THREAD_DEFINE(blink1_id, STACKSIZE, blink1, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(uartTx_id, STACKSIZE, uartTx, NULL, NULL, NULL, PRIORITY, 0, 0);