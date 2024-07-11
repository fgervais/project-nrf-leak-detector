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

#define BUZZER_PERIOD_SEC 	0.500f
// #define VALUE_REPEAT		((BUZZER_PERIOD_SEC / 2.0f) / (1.0f/DESIRED_FREQ_HZ))
#define VALUE_REPEAT		100


/*
 * Although we send a sequence of a single note at a time, `note_value[]` is of
 * size `2` because the STOP event is triggered when the last value of the
 * sequence is loaded i.e. before the value is repeated.
 * Without this second dummy value, only the first period of the note would be
 * played.
 * 
 * See `NRFX_PWM_FLAG_STOP` definition for more information.
 * 
 * > The STOP task is triggered when the last value of the final sequence is
 * loaded from RAM, and the peripheral stops at the end of the current PWM period.
 * For sequences with configured repeating of duty cycle values, this might
 * result in less than the requested number of repeats of the last value.
 */ 
struct sound_effect_playback {
	nrfx_pwm_t *pwm_instance;
	uint16_t note_value[2];
	uint16_t notes_top_value[128];
	uint16_t notes_repeat_value[128];
	int number_of_notes;
	int note_to_play;
};


static void play_next_note(struct sound_effect_playback *sep);


static void pwm_handler(nrfx_pwm_evt_type_t event_type, void *p_context)
{
	struct sound_effect_playback *sep = p_context;

	LOG_INF("pwm_handler (%d)", event_type);

	if (event_type == NRFX_PWM_EVT_STOPPED) {
		if (sep->note_to_play < sep->number_of_notes) {
			play_next_note(sep);
		}
	}

	// nrfx_pwm_stop(sep->pwm_instance, 0);
}

// void beep(const struct pwm_dt_spec *buzzer)
// {
// 	pwm_set_dt(buzzer, PWM_USEC(250U), PWM_USEC(250U) * 0.53);
// 	k_sleep(K_MSEC(50));
// 	pwm_set_dt(buzzer, PWM_USEC(250U), 0);
// 	k_sleep(K_MSEC(50));
// }

// static uint16_t tone_sequence_values[1];
// static uint16_t notes_top_value[128];
// static int number_of_notes;

static struct sound_effect_playback sound_effect_playback;

// void play_tone(const struct pwm_dt_spec *spec, int frequency_hz, int duration_ms)
// {
// 	const nrfx_pwm_t *pwm_instance = spec->dev->config;

// 	// pwm_set_dt(spec, PWM_HZ(frequency_hz), PWM_HZ(frequency_hz) * 0.53);
// 	nrf_pwm_configure(pwm_instance->p_reg,
// 			  NRF_PWM_CLK_16MHz,
// 			  NRF_PWM_MODE_UP,
// 			  TOP_VALUE);

// 	nrf_pwm_sequence_t tone_sequence = {
// 		.values.p_raw = tone_sequence_values,
// 		.length = ARRAY_SIZE(tone_sequence_values),
// 		.repeats = VALUE_REPEAT,
// 	};

// 	nrfx_pwm_simple_playback(pwm_instance,
// 				 &tone_sequence,
// 				 (duration_ms / 1000.0f) / BUZZER_PERIOD_SEC,
// 				 NRFX_PWM_FLAG_STOP);

// 	// k_sleep(K_MSEC(duration_ms));
// }

// void no_tone(const struct pwm_dt_spec *spec)
// {
// 	const nrfx_pwm_t *pwm_instance = spec->dev->config;

// 	// pwm_set_dt(spec, 0, 0);
// 	nrf_pwm_configure(pwm_instance->p_reg,
// 			  NRF_PWM_CLK_16MHz,
// 			  NRF_PWM_MODE_UP,
// 			  TOP_VALUE);
// }

// void sound_coin(const struct pwm_dt_spec *buzzer)
// {
// 	play_tone(buzzer, NOTE_B5, 100);
// 	play_tone(buzzer, NOTE_E6, 350);
// 	no_tone(buzzer);
// }

// static uint16_t note_to_top_value(float note)
// {
// 	return (1.0f / note) / (1.0f / PWM_CLOCK_HZ);
// }

// static uint16_t note_duration_to_repeat_value(float note_duration_ms)
// {
// 	return (note_duration_ms / 1000.0f) / (1.0f / PWM_CLOCK_HZ);
// }

static void configure_note(struct sound_effect_playback *sep,
			       int note_index,
			       float note, float note_duration_ms)
{
	sep->notes_top_value[note_index] = (1.0f / note) / (1.0f / PWM_CLOCK_HZ);
	sep->notes_repeat_value[note_index] = (
		(note_duration_ms / 1000.0f)
		/ ((1.0f / PWM_CLOCK_HZ) * sep->notes_top_value[note_index])
	);
}

static void play_next_note(struct sound_effect_playback *sep)
{
	sep->note_value[0] = sep->notes_top_value[sep->note_to_play] / 2.0f;

	LOG_DBG("🎹 Playing note %d", sep->note_to_play);
	LOG_DBG("├── note top value %d", sep->notes_top_value[sep->note_to_play]);
	LOG_DBG("├── note value %d", sep->note_value[0]);
	LOG_DBG("└── note repeat %d", sep->notes_repeat_value[sep->note_to_play]);

	nrf_pwm_configure(sep->pwm_instance->p_reg,
			  NRF_PWM_CLK_16MHz,
			  NRF_PWM_MODE_UP,
			  sep->notes_top_value[sep->note_to_play]);

	nrf_pwm_sequence_t note_sequence = {
		.values.p_raw = (uint16_t *)&sep->note_value,
		.length = ARRAY_SIZE(sep->note_value),
		.repeats = sep->notes_repeat_value[sep->note_to_play],
	};

	nrfx_pwm_simple_playback(sep->pwm_instance,
				 &note_sequence,
				 1,
				 NRFX_PWM_FLAG_STOP);

	sep->note_to_play++;
}

void sound_1up()
{
	sound_effect_playback.note_to_play = 0;
	sound_effect_playback.number_of_notes = 6;

	configure_note(&sound_effect_playback, 0, NOTE_E6, 160);
	configure_note(&sound_effect_playback, 1, NOTE_G6, 132);
	configure_note(&sound_effect_playback, 2, NOTE_E7, 160);
	configure_note(&sound_effect_playback, 3, NOTE_C7, 125);
	configure_note(&sound_effect_playback, 4, NOTE_D7, 125);
	configure_note(&sound_effect_playback, 5, NOTE_G7, 125);

	play_next_note(&sound_effect_playback);

	// play_tone(buzzer, NOTE_E6, 125);
	// play_tone(buzzer, NOTE_G6, 125);
	// play_tone(buzzer, NOTE_E7, 125);
	// play_tone(buzzer, NOTE_C7, 125);
	// play_tone(buzzer, NOTE_D7, 125);
	// play_tone(buzzer, NOTE_G7, 125);
	// no_tone(buzzer);
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
				 seconds / BUZZER_PERIOD_SEC,
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

	sound_effect_playback.pwm_instance =(nrfx_pwm_t *)pwm_instance;

	nrfx_pwm_uninit(pwm_instance);

	ret = nrfx_pwm_init(pwm_instance, &pwm_initial_config,
                        	 // pwm_handler, NULL);
                        	 pwm_handler, &sound_effect_playback);
	if (ret != NRFX_SUCCESS) {
		LOG_ERR("Cannot initialize pwm");
		return -EIO;
	}

	IRQ_CONNECT(DT_IRQN(DT_NODELABEL(pwm0)),
		    DT_IRQ(DT_NODELABEL(pwm0), priority),
		    nrfx_isr, NRFX_PWM_INST_HANDLER_GET(0), 0);



	// ret = nrfx_pwm_reconfigure(pwm_instance, &pwm_initial_config);
	// if (ret == NRFX_ERROR_INVALID_STATE) {
	// 	LOG_ERR("Cannot reconfigure, pwm was not initialized");
	// 	return -EINVAL;
	// }
	// if (ret == NRFX_ERROR_BUSY) {
	// 	LOG_ERR("Cannot reconfigure, pwm is running");
	// 	return -EBUSY;
	// }

	return 0;
}
