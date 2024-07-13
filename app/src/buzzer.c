#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(buzzer, LOG_LEVEL_DBG);

#include <nrfx_pwm.h>

#include "music_notes.h"


#define PWM_BASE_CLOCK		16000000.0f
#define PWM_PRESCALER		1.0f
#define PWM_CLOCK_HZ		(PWM_BASE_CLOCK / PWM_PRESCALER)

#define DESIRED_FREQ_HZ		4000.0f
#define TOP_VALUE		((1.0f/DESIRED_FREQ_HZ) / (1.0f/PWM_CLOCK_HZ))

#define BUZZER_PERIOD_SEC 	0.500f
// #define VALUE_REPEAT		((BUZZER_PERIOD_SEC / 2.0f) / (1.0f/DESIRED_FREQ_HZ))
#define VALUE_REPEAT		100

#define MAX_NUMBER_OF_NOTES	32


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
	uint16_t notes_base_clock[MAX_NUMBER_OF_NOTES];
	uint16_t notes_top_value[MAX_NUMBER_OF_NOTES];
	uint16_t notes_value[MAX_NUMBER_OF_NOTES];
	uint32_t notes_repeat_value[MAX_NUMBER_OF_NOTES];
	int number_of_notes;
	int note_to_play;
};


static void play_next_note(struct sound_effect_playback *sep);


static void pwm_handler(nrfx_pwm_evt_type_t event_type, void *p_context)
{
	struct sound_effect_playback *sep = p_context;

	LOG_DBG("pwm_handler (%d)", event_type);

	if (event_type == NRFX_PWM_EVT_STOPPED) {
		if (sep->note_to_play < sep->number_of_notes) {
			play_next_note(sep);
		}
	}
}


static struct sound_effect_playback sound_effect_playback;


static int add_note(struct sound_effect_playback *sep,
		     float note, float note_duration_ms)
{
	int note_index = sep->number_of_notes;
	uint8_t clk_divider;
	uint32_t top_value; // 32bit so we can verify if is fits in 15bit
	uint32_t pwm_clock;

	LOG_DBG("-----------------");

	LOG_DBG("note: %d", (int)note);

	if (note == 0) {
		pwm_clock = 16000000;
		clk_divider = 0;
		top_value = 16000; // Gives 1ms per pwm period
	}
	else {
		for (clk_divider = 0; clk_divider < 8; clk_divider++) {
			pwm_clock = 16000000 >> clk_divider;
			LOG_DBG("pwm_clock: %d", pwm_clock);

			top_value = (1.0f / note) / (1.0f / pwm_clock);
			LOG_DBG("top_value: %d", top_value);

			if (top_value < (UINT16_MAX >> 1)) {
				break;
			}
		}

		if (clk_divider == 8) {
			LOG_ERR("Could not find a suitable PWM divider");
			return -EINVAL;
		}
	}

	sep->notes_base_clock[note_index] = clk_divider;
	sep->notes_top_value[note_index] = top_value;

	if (note == 0) {
		sep->notes_value[note_index] = top_value;
	}
	else {
		sep->notes_value[note_index] = top_value / 2.0f;
	}

	sep->notes_repeat_value[note_index] = (
		(note_duration_ms / 1000.0f)
		/ ((1.0f / pwm_clock) * top_value)
	);

	LOG_DBG("notes_repeat_value: %d", sep->notes_repeat_value[note_index]);

	sep->number_of_notes++;

	return 0;
}

static void play_next_note(struct sound_effect_playback *sep)
{
	LOG_DBG("🎹 Playing note %d", sep->note_to_play);
	LOG_DBG("├── note base clock %d", sep->notes_base_clock[sep->note_to_play]);
	LOG_DBG("├── note top value %d", sep->notes_top_value[sep->note_to_play]);
	LOG_DBG("├── note value %d", sep->notes_value[sep->note_to_play]);
	LOG_DBG("└── note repeat %d", sep->notes_repeat_value[sep->note_to_play]);

	sep->note_value[0] = sep->notes_value[sep->note_to_play];

	nrf_pwm_configure(sep->pwm_instance->p_reg,
			  sep->notes_base_clock[sep->note_to_play],
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
	sound_effect_playback.number_of_notes = 0;

	add_note(&sound_effect_playback, NOTE_E6, 135);
	add_note(&sound_effect_playback, NOTE_G6, 135);
	add_note(&sound_effect_playback, NOTE_E7, 135);
	add_note(&sound_effect_playback, NOTE_C7, 135);
	add_note(&sound_effect_playback, NOTE_D7, 135);
	add_note(&sound_effect_playback, NOTE_G7, 135);

	play_next_note(&sound_effect_playback);
}

void sound_enter_world()
{
	sound_effect_playback.note_to_play = 0;
	sound_effect_playback.number_of_notes = 0;

	add_note(&sound_effect_playback, NOTE_FS8, 66);
	add_note(&sound_effect_playback, NOTE_DS8, 66);
	add_note(&sound_effect_playback, NOTE_C8, 66);
	add_note(&sound_effect_playback, NOTE_A7, 66);
	add_note(&sound_effect_playback, NOTE_G7, 66);
	add_note(&sound_effect_playback, NOTE_E7, 66);
	add_note(&sound_effect_playback, NOTE_CS7, 66);
	add_note(&sound_effect_playback, NOTE_A6, 66);
	add_note(&sound_effect_playback, NOTE_G6, 66);
	add_note(&sound_effect_playback, NOTE_E6, 66);
	add_note(&sound_effect_playback, NOTE_CS6, 66);
	add_note(&sound_effect_playback, NOTE_A5, 300);

	play_next_note(&sound_effect_playback);
}

void sound_game_over()
{
	sound_effect_playback.note_to_play = 0;
	sound_effect_playback.number_of_notes = 0;

	add_note(&sound_effect_playback, NOTE_G5, 150);
	add_note(&sound_effect_playback, 0, 50);
	add_note(&sound_effect_playback, NOTE_G5, 150);
	add_note(&sound_effect_playback, 0, 50);
	add_note(&sound_effect_playback, NOTE_G5, 150);
	add_note(&sound_effect_playback, 0, 50);
	add_note(&sound_effect_playback, NOTE_D6, 400);
	add_note(&sound_effect_playback, NOTE_C6, 200);
	add_note(&sound_effect_playback, NOTE_A5, 200);
	add_note(&sound_effect_playback, NOTE_F5, 200);
	add_note(&sound_effect_playback, NOTE_AS5, 200);
	add_note(&sound_effect_playback, 0, 400);
	add_note(&sound_effect_playback, NOTE_CS5, 200);
	add_note(&sound_effect_playback, NOTE_D5, 150);
	add_note(&sound_effect_playback, 0, 50);
	add_note(&sound_effect_playback, NOTE_G4, 200);

	play_next_note(&sound_effect_playback);
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
