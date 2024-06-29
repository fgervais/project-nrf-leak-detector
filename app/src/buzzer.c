#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include "music_notes.h"


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

void alarm(const struct pwm_dt_spec *buzzer, int seconds)
{
	for (int i = 0; i < (seconds * 2); i++) {
		pwm_set_dt(buzzer, PWM_USEC(250U), PWM_USEC(250U) * 0.53);
		k_sleep(K_MSEC(250));
		pwm_set_dt(buzzer, PWM_USEC(250U), 0);
		k_sleep(K_MSEC(250));
	}
}
