#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(buzzer, LOG_LEVEL_DBG);

#include <nrfx_pwm.h>

#include "music_notes.h"


#define PWM_PRESCALER		1.0f
#define PWM_CLOCK_HZ		(16000000.0f / PWM_PRESCALER)

#define DESIRED_FREQ_HZ		4000.0f
#define TOP_VALUE		((1.0f/DESIRED_FREQ_HZ) / (1.0f/PWM_CLOCK_HZ))

#define BUZZER_PERIOD_SEC 	0.250f
#define VALUE_REPEAT		(BUZZER_PERIOD_SEC / (1.0f/DESIRED_FREQ_HZ))


void beep(const struct pwm_dt_spec *buzzer)
{
	pwm_set_dt(buzzer, PWM_USEC(250U), PWM_USEC(250U) * 0.53);
	k_sleep(K_MSEC(50));
	pwm_set_dt(buzzer, PWM_USEC(250U), 0);
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

void sound_coin(const struct pwm_dt_spec *buzzer)
{
	play_tone(buzzer, NOTE_B5, 100);
	play_tone(buzzer, NOTE_E6, 350);
	no_tone(buzzer);
}

void sound_1up(const struct pwm_dt_spec *buzzer)
{
	play_tone(buzzer, NOTE_E6, 125);
	play_tone(buzzer, NOTE_G6, 125);
	play_tone(buzzer, NOTE_E7, 125);
	play_tone(buzzer, NOTE_C7, 125);
	play_tone(buzzer, NOTE_D7, 125);
	play_tone(buzzer, NOTE_G7, 125);
	no_tone(buzzer);
}

static uint16_t alarm_sequence_values[] = {
	TOP_VALUE / 2,	// 50% PWM
	TOP_VALUE,	// 0%
};

int buzzer_alarm(const struct pwm_dt_spec *buzzer, int seconds)
{
	const nrfx_pwm_t *pwm_instance = buzzer->dev->config;

	nrf_pwm_sequence_t alarm_sequence = {
		.values.p_raw = alarm_sequence_values,
		.length = ARRAY_SIZE(alarm_sequence_values),
		.repeats = VALUE_REPEAT,
	};

	nrfx_pwm_simple_playback(pwm_instance,
				 &alarm_sequence,
				 seconds / (BUZZER_PERIOD_SEC * 2),
				 NRFX_PWM_FLAG_STOP);

	return 0;
}

bool buzzer_is_running(const struct pwm_dt_spec *buzzer)
{
	const nrfx_pwm_t *pwm_instance = buzzer->dev->config;

	return !nrfx_pwm_stopped_check(pwm_instance);
}

int buzzer_init(const struct pwm_dt_spec *buzzer)
{
	int ret;

	nrfx_pwm_config_t pwm_initial_config = {					      \
		.skip_gpio_cfg = true,
		.skip_psel_cfg = true,
		.base_clock = NRF_PWM_CLK_16MHz,
		.count_mode = NRF_PWM_MODE_UP,
		.top_value = TOP_VALUE,
		.load_mode = NRF_PWM_LOAD_COMMON,
		.step_mode = NRF_PWM_STEP_AUTO,
	};

	// We cheat here but we can do it since `nrfx_pwm_t`
	// is the first member of the struct.
	const nrfx_pwm_t *pwm_instance = buzzer->dev->config;

	ret = nrfx_pwm_reconfigure(pwm_instance, &pwm_initial_config);
	if (ret == NRFX_ERROR_INVALID_STATE) {
		LOG_ERR("Cannot reconfigure, pwm was not initialized");
		return -EINVAL;
	}
	if (ret == NRFX_ERROR_BUSY) {
		LOG_ERR("Cannot reconfigure, pwm is running");
		return -EBUSY;
	}

	return 0;
}
