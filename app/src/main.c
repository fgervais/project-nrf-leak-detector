#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/sys/poweroff.h>

#include <hal/nrf_power.h>
#include <nrfx_lpcomp.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include <app_version.h>
#include <reset.h>

#include "music_notes.h"


#define ALARM_TIME_SEC	60


static const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(DT_PATH(buzzer));


static void comparator_handler(nrf_lpcomp_event_t event)
{
	LOG_INF("COMP event");
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

static int system_off(void)
{
	int ret;
	const struct device *cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

	LOG_INF("Entering system off");

	ret = pm_device_action_run(cons, PM_DEVICE_ACTION_SUSPEND);
	if (ret < 0) {
		LOG_ERR("Could not suspend console (%d)", ret);
		return -1;
	}

	sys_poweroff();

	return 0;
}

int main(void)
{
	uint32_t reset_cause;

	nrfx_err_t err;
	nrfx_lpcomp_config_t lpcomp_config = {
	    .reference = NRF_LPCOMP_REF_SUPPLY_6_8,
	    .detection = NRF_LPCOMP_DETECT_DOWN,
	    NRFX_COND_CODE_1(LPCOMP_FEATURE_HYST_PRESENT, (.hyst = NRF_LPCOMP_HYST_NOHYST,), ())
	    .input  = (nrf_lpcomp_input_t)NRF_LPCOMP_INPUT_2,
	    .interrupt_priority = NRFX_LPCOMP_DEFAULT_CONFIG_IRQ_PRIORITY
	};

	LOG_INF("\n\n🚀 MAIN START (%s) 🚀\n", APP_VERSION_FULL);

	reset_cause = show_and_clear_reset_cause();

	if (!device_is_ready(buzzer.dev)) {
		printk("%s: device not ready.\n", buzzer.dev->name);
		return 1;
	}

	if (is_reset_cause_lpcomp(reset_cause)) {
		alarm(ALARM_TIME_SEC);
		goto shutdown;
	}
	else {
		sound_1up();
	}

	err = nrfx_lpcomp_init(&lpcomp_config, comparator_handler);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nrfx_comp_init error: %08x", err);
		return 1;
	}

	IRQ_CONNECT(COMP_LPCOMP_IRQn,
		    NRFX_LPCOMP_DEFAULT_CONFIG_IRQ_PRIORITY-1,
		    nrfx_isr, nrfx_lpcomp_irq_handler, 0);

	nrfx_lpcomp_enable();

	LOG_INF("🆗 initialized");

shutdown:
	system_off();

	return 0;
}
