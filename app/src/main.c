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
#include <nrfx_pwm.h>
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

#define PWM_PRESCALER		1.0f
#define PWM_CLOCK_HZ		(16000000.0f / PWM_PRESCALER)

#define DESIRED_FREQ_HZ		4000.0f
#define TOP_VALUE		((1.0f/DESIRED_FREQ_HZ) / (1.0f/PWM_CLOCK_HZ))

#define BUZZER_PERIOD_SEC 	0.250f
#define VALUE_REPEAT		(BUZZER_PERIOD_SEC / (1.0f/DESIRED_FREQ_HZ))

#define PLAYBACK_COUNT		(ALARM_TIME_SEC / (BUZZER_PERIOD_SEC * 2))

#define TO_NRF_PWM_CLK_T(x)	(LOG2(x) / LOG2(2))

// nrf_pwm_values_t 	values
//  	Pointer to an array with duty cycle values. This array must be in Data RAM. More...
 
// uint16_t 	length
//  	Number of 16-bit values in the array pointed by values.
 
// uint32_t 	repeats
//  	Number of times that each duty cycle should be repeated (after being played once). Ignored in NRF_PWM_STEP_TRIGGERED mode.
 
// uint32_t 	end_delay
//  	Additional time (in PWM periods) that the last duty cycle is to be kept after the sequence is played. Ignored in NRF_PWM_STEP_TRIGGERED mode.

uint16_t seq_values[] = {
	TOP_VALUE / 2,	// 50% PWM
	TOP_VALUE,	// 0%
};

static nrf_pwm_sequence_t seq = {
	.values.p_raw = seq_values,
	.length = ARRAY_SIZE(seq_values),
	.repeats = VALUE_REPEAT,
};

// static uint16_t seq_values[] = {
// 	2000,	// 50% PWM
// 	4000,	// 0%
// };

// static nrf_pwm_sequence_t seq = {
// 	.values.p_raw = seq_values,
// 	.length = ARRAY_SIZE(seq_values),
// 	.repeats = 1000,
// };


static nrfx_pwm_config_t pwm_initial_config = {					      \
	.skip_gpio_cfg = true,
	.skip_psel_cfg = true,
	.base_clock = NRF_PWM_CLK_16MHz,
	.count_mode = NRF_PWM_MODE_UP,
	.top_value = TOP_VALUE,
	.load_mode = NRF_PWM_LOAD_COMMON,
	.step_mode = NRF_PWM_STEP_AUTO,
};
		

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
	const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(DT_PATH(buzzer));


	LOG_INF("\n\n🚀 MAIN START (%s) 🚀\n", APP_VERSION_FULL);

	reset_cause = show_and_clear_reset_cause();

	if (!device_is_ready(buzzer.dev)) {
		printk("%s: device not ready.\n", buzzer.dev->name);
		return 1;
	}

	// alarm(&buzzer, 1);
	// return 1;


	// const struct pwm_nrfx_config *buzzer_config = buzzer.dev->config;

	// It's the fist member of the struct
	const nrfx_pwm_t *pwm = buzzer.dev->config;
	// nrfx_pwm_t pwm = NRFX_PWM_INSTANCE(0);


	nrfx_pwm_reconfigure(pwm,
		      &pwm_initial_config);

	// LOG_INF("TOP_VALUE: %d", (uint16_t)TOP_VALUE);
	// LOG_INF("base_clock: %d", TO_NRF_PWM_CLK_T(PWM_PRESCALER));
	// nrf_pwm_configure(pwm.p_reg,
	// 		  TO_NRF_PWM_CLK_T(PWM_PRESCALER),
	// 		  NRF_PWM_MODE_UP,
	// 		  TOP_VALUE);

	// nrfx_pwm_simple_playback(&pwm,
	// 			 &seq,
	// 			 PLAYBACK_COUNT,
	// 			 NRFX_PWM_FLAG_STOP);

	// nrf_pwm_configure(pwm.p_reg,
	// 		  0,
	// 		  NRF_PWM_MODE_UP,
	// 		  4000);

	nrfx_pwm_simple_playback(pwm,
				 &seq,
				 PLAYBACK_COUNT,
				 0);

	return 0;



	if (is_reset_cause_lpcomp(reset_cause)) {
		// alarm(&buzzer, ALARM_TIME_SEC);
		goto shutdown;
	}
	else {
		// sound_1up(&buzzer);
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
