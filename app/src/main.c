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

#include "buzzer.h"


#define ALARM_TIME_SEC	60


static void comparator_handler(nrf_lpcomp_event_t event)
{
	LOG_INF("COMP event");
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
	int ret;

	nrfx_err_t err;
	nrfx_lpcomp_config_t lpcomp_config = {
	    .reference = NRF_LPCOMP_REF_SUPPLY_6_8,
	    .detection = NRF_LPCOMP_DETECT_DOWN,
	    NRFX_COND_CODE_1(LPCOMP_FEATURE_HYST_PRESENT, (.hyst = NRF_LPCOMP_HYST_NOHYST,), ())
	    .input  = (nrf_lpcomp_input_t)NRF_LPCOMP_INPUT_2,
	    .interrupt_priority = NRFX_LPCOMP_DEFAULT_CONFIG_IRQ_PRIORITY
	};
	const struct pwm_dt_spec buzzer_dt_spec = PWM_DT_SPEC_GET(DT_PATH(buzzer));


	LOG_INF("\n\n🚀 MAIN START (%s) 🚀\n", APP_VERSION_FULL);

	reset_cause = show_and_clear_reset_cause();

	if (!device_is_ready(buzzer_dt_spec.dev)) {
		printk("%s: device not ready.\n", buzzer_dt_spec.dev->name);
		return 1;
	}

	ret = buzzer_init(&buzzer_dt_spec);
	if (ret < 0) {
		LOG_ERR("Could not initialize buzzer");
		return 1;
	}

	if (is_reset_cause_lpcomp(reset_cause)) {
		buzzer_alarm(&buzzer_dt_spec, ALARM_TIME_SEC);
		goto shutdown;
	}
	else {
		LOG_INF("Playing 1up");
		// sound_1up();
		// sound_enter_world();
		// sound_game_over();
		// smb2_sound_game_over();
		smb2_main_theme();
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
	while (buzzer_is_running(&buzzer_dt_spec)) {
		LOG_INF("Waiting for buzzer to finish");
		k_sleep(K_SECONDS(1));
	}

	system_off();

	return 0;
}
