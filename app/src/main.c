#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>

#include <hal/nrf_power.h>
#include <nrfx_lpcomp.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include "music_notes.h"


#define ALARM_TIME_SEC	60


static const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(DT_PATH(buzzer, pwm));


static void comparator_handler(nrf_lpcomp_event_t event)
{
	LOG_INF("COMP event");
}


static void print_power_resetreas(uint32_t resetreas)
{
	if (resetreas & NRF_POWER_RESETREAS_RESETPIN_MASK) {
		LOG_INF("Reset Reason: Pin Reset");
	} else if (resetreas & NRF_POWER_RESETREAS_DOG_MASK) {
		LOG_INF("Reset Reason: Watchdog");
	} else if (resetreas & NRF_POWER_RESETREAS_SREQ_MASK) {
		LOG_INF("Reset Reason: Software");
	} else if (resetreas & NRF_POWER_RESETREAS_LOCKUP_MASK) {
		LOG_INF("Reset Reason: Lockup");
#if defined (POWER_RESETREAS_LPCOMP_Msk)
	} else if (resetreas & NRF_POWER_RESETREAS_LPCOMP_MASK) {
		LOG_INF("Reset Reason: LPCOMP Wakeup");
#endif
	} else if (resetreas & NRF_POWER_RESETREAS_DIF_MASK) {
		LOG_INF("Reset Reason: Debug Interface Wakeup");
#if defined (NRF_POWER_RESETREAS_VBUS_MASK)
	} else if (resetreas & NRF_POWER_RESETREAS_VBUS_MASK) {
		LOG_INF("Reset Reason: VBUS Wakeup");
#endif
	} else if (resetreas == 0) {
		// absence of a value, means a power on reset took place
		LOG_INF("Reset Reason: Power on Reset");
	} else {
		LOG_INF("Reset Reason: Unknown");
	}
}


void beep(void)
{
	pwm_set_dt(&buzzer, PWM_USEC(250U), PWM_USEC(250U) * 0.53);
	k_sleep(K_MSEC(50));
	pwm_set_dt(&buzzer, PWM_USEC(250U), 0);
	k_sleep(K_MSEC(50));
}

void play_tone(const struct pwm_dt_spec *spec, int frequency_hz, int duration_ms)
{
	pwm_set_dt(spec, PWM_HZ(frequency_hz), PWM_HZ(frequency_hz) * 0.53);
	k_sleep(K_MSEC(duration_ms));
}

void no_tone(const struct pwm_dt_spec *spec)
{
	pwm_set_dt(spec, 0, 0);
}

void sound_coin(void)
{
	play_tone(&buzzer, NOTE_B5, 100);
	play_tone(&buzzer, NOTE_E6, 350);
	no_tone(&buzzer);
}

void sound_1up(void)
{
	play_tone(&buzzer, NOTE_E6, 125);
	play_tone(&buzzer, NOTE_G6, 125);
	play_tone(&buzzer, NOTE_E7, 125);
	play_tone(&buzzer, NOTE_C7, 125);
	play_tone(&buzzer, NOTE_D7, 125);
	play_tone(&buzzer, NOTE_G7, 125);
	no_tone(&buzzer);
}

void alarm(int seconds)
{
	for (int i = 0; i < (seconds * 2); i++) {
		pwm_set_dt(&buzzer, PWM_USEC(250U), PWM_USEC(250U) * 0.53);
		k_sleep(K_MSEC(250));
		pwm_set_dt(&buzzer, PWM_USEC(250U), 0);
		k_sleep(K_MSEC(250));
	}
}

static void system_off()
{
	const struct device *cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

	LOG_INF("Entering system off");

	pm_device_action_run(cons, PM_DEVICE_ACTION_SUSPEND);
	LOG_ERR("Console did not suspend");

	k_sleep(K_SECONDS(1));

	/* Before we disabled entry to deep sleep. Here we need to override
	 * that, then force a sleep so that the deep sleep takes effect.
	 */
	const struct pm_state_info si = {PM_STATE_SOFT_OFF, 0, 0};

	pm_state_force(0, &si);

	/* Going into sleep will actually go to system off mode, because we
	 * forced it above.
	 */
	k_sleep(K_MSEC(1));

	/* k_sleep will never exit, so below two lines will never be executed
	 * if system off was correct. On the other hand if someting gone wrong
	 * we will see it on terminal.
	 */
	// LOG_ERR("System off failed");
}

void main(void)
{
	uint32_t reas;

	nrfx_err_t err;
	nrfx_lpcomp_config_t lpcomp_config = {
	    .hal    = {  NRF_LPCOMP_REF_SUPPLY_6_8,
	                 NRF_LPCOMP_DETECT_DOWN,
	                 NRF_LPCOMP_HYST_NOHYST },
	    .input  = (nrf_lpcomp_input_t)NRF_LPCOMP_INPUT_2,
	    .interrupt_priority = NRFX_LPCOMP_DEFAULT_CONFIG_IRQ_PRIORITY
	};


	reas = nrf_power_resetreas_get(NRF_POWER);
	nrf_power_resetreas_clear(NRF_POWER, reas);
	print_power_resetreas(reas);

	if (!device_is_ready(buzzer.dev)) {
		printk("%s: device not ready.\n", buzzer.dev->name);
		return;
	}

	if (reas & NRF_POWER_RESETREAS_LPCOMP_MASK) {
		alarm(ALARM_TIME_SEC);
		goto shutdown;
	}
	else {
		sound_1up();
	}

	err = nrfx_lpcomp_init(&lpcomp_config, comparator_handler);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrfx_comp_init error: %08x", err);
		return;
	}

	IRQ_CONNECT(COMP_LPCOMP_IRQn,
		    NRFX_LPCOMP_DEFAULT_CONFIG_IRQ_PRIORITY-1,
		    nrfx_isr, nrfx_lpcomp_irq_handler, 0);

	nrfx_lpcomp_enable();

shutdown:
	LOG_INF("****************************************");
	LOG_INF("MAIN DONE");
	LOG_INF("****************************************");

	system_off();
}
