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
#include <mymodule/base/ha.h>
#include <mymodule/base/mqtt.h>
#include <mymodule/base/openthread.h>
#include <mymodule/base/reset.h>
#include <mymodule/base/uid.h>

#include "buzzer.h"


#define ALARM_TIME_SEC		60
#define HA_NUMBER_OF_RETRIES	3
#define HA_RETRY_DELAY_SECONDS	10


static struct ha_sensor leak_detected_sensor = {
	.type = HA_BINARY_SENSOR_TYPE,
	.name = "Leak",
	.device_class = "problem",
	.retain = true,
};


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

	// Do only the bare minimum to start the alarm if required.
	// This is the priority.
	if (is_reset_cause_lpcomp(reset_cause)) {
		buzzer_alarm(ALARM_TIME_SEC);
	}

	ret = openthread_my_start();
	if (ret < 0) {
		LOG_ERR("Could not start openthread");
		return ret;
	}

	ret = uid_init();
	if (ret < 0) {
		LOG_ERR("Could not init uid module");
		return ret;
	}

	ret = uid_generate_unique_id(leak_detected_sensor.unique_id,
				     sizeof(leak_detected_sensor.unique_id),
				     "nrf52840", "leak",
				     uid_get_device_id());
	if (ret < 0) {
		LOG_ERR("Could not generate leak sensor unique id");
		return ret;
	}

	LOG_INF("💤 waiting for openthread to be ready");
	openthread_wait_for_ready();
	// Something else is not ready, not sure what
	k_sleep(K_MSEC(100));

	bool inhibit_discovery = is_reset_cause_lpcomp(reset_cause);
	bool enable_last_will = false;
	ha_start(uid_get_device_id(), inhibit_discovery, enable_last_will);

	// On the alert path, this has not way to fail other than ENOMEM
	ha_register_sensor_retry(&leak_detected_sensor,
				 HA_NUMBER_OF_RETRIES,
				 HA_RETRY_DELAY_SECONDS,
				 0);

	ha_set_binary_sensor_state(&leak_detected_sensor,
				   is_reset_cause_lpcomp(reset_cause));

	if (is_reset_cause_lpcomp(reset_cause)) {
		// Retry for about 3h, this is the last thing we will do anyway
		ha_send_binary_sensor_retry(&leak_detected_sensor,
				    	    11, // Last after about 3h
					    HA_RETRY_DELAY_SECONDS,
					    (HA_RETRY_EXP_BACKOFF |
					     HA_RETRY_WAIT_PUBACK));
	}
	else {
		// We set the device online a little after sensor
		// registrations so HA gets time to process the sensor
		// registrations first before setting the entities online
		LOG_INF("💤 waiting for HA to process registration");
		k_sleep(K_SECONDS(3));

		ha_set_online_retry(HA_NUMBER_OF_RETRIES,
				    HA_RETRY_DELAY_SECONDS,
				    HA_RETRY_NO_FLAGS);

		ha_send_binary_sensor_retry(&leak_detected_sensor,
				    	    HA_NUMBER_OF_RETRIES,
					    HA_RETRY_DELAY_SECONDS,
					    HA_RETRY_NO_FLAGS);

		// Arm the detector
		err = nrfx_lpcomp_init(&lpcomp_config, comparator_handler);
		if (err != NRFX_SUCCESS) {
			LOG_ERR("nrfx_comp_init error: %08x", err);
			return 1;
		}

		IRQ_CONNECT(COMP_LPCOMP_IRQn,
			    IRQ_PRIO_LOWEST,
			    nrfx_isr, nrfx_lpcomp_irq_handler, 0);

		nrfx_lpcomp_enable();

		LOG_INF("🆗 initialized");

		LOG_INF("Playing detector ready sound");
		// smb3_sound_1up();
		smb3_sound_enter_world();
		// smd3_sound_game_over();
		// smb2_sound_game_over();
		// smb2_main_theme();
	}

	while (buzzer_is_running(&buzzer_dt_spec)) {
		LOG_INF("Waiting for buzzer to finish");
		k_sleep(K_SECONDS(1));
	}

	system_off();

	return 0;
}
