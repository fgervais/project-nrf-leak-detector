#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>

#include <nrfx_lpcomp.h>

#include <app_event_manager.h>

#define MODULE main
#include <caf/events/module_state_event.h>
#include <caf/events/button_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);


#define SLEEP_TIME_MS   10
#define LED0_NODE DT_ALIAS(myled0alias)


static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(DT_PATH(buzzer, pwm));


static void comparator_handler(nrf_lpcomp_event_t event)
{
	LOG_INF("COMP event");
}

// NRF_POWER_RESETREAS_LPCOMP_MASK

void main(void)
{
	int ret;
	const struct device *cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

	nrfx_err_t err;
	nrfx_lpcomp_config_t lpcomp_config = NRFX_LPCOMP_DEFAULT_CONFIG(NRF_LPCOMP_INPUT_2);


	if (app_event_manager_init()) {
		LOG_ERR("Event manager not initialized");
	} else {
		module_set_state(MODULE_STATE_READY);
	}

	if (!device_is_ready(led.port)) {
		return;
	}

	if (!device_is_ready(buzzer.dev)) {
		printk("%s: device not ready.\n", buzzer.dev->name);
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}

	err = nrfx_lpcomp_init(&lpcomp_config, comparator_handler);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrfx_comp_init error: %08x", err);
		return;
	}

	IRQ_CONNECT(COMP_LPCOMP_IRQn,
		    7-1,
		    nrfx_isr, nrfx_lpcomp_irq_handler, 0);

	nrfx_lpcomp_enable();



	pwm_set_dt(&buzzer, PWM_USEC(250U), PWM_USEC(250U) * 0.53);

	LOG_INF("****************************************");
	LOG_INF("MAIN DONE");
	LOG_INF("****************************************");

	k_sleep(K_SECONDS(3));
	// pm_device_action_run(cons, PM_DEVICE_ACTION_SUSPEND);

	LOG_INF("PM_DEVICE_ACTION_SUSPEND");
}

static bool event_handler(const struct app_event_header *eh)
{
	int ret;
	const struct button_event *evt;

	if (is_button_event(eh)) {
		evt = cast_button_event(eh);

		if (evt->pressed) {
			LOG_INF("Pin Toggle");
			ret = gpio_pin_toggle_dt(&led);
		}
	}

	return true;
}

APP_EVENT_LISTENER(MODULE, event_handler);
APP_EVENT_SUBSCRIBE(MODULE, button_event);
