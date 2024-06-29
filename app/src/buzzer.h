#ifndef BUZZER_H_
#define BUZZER_H_

struct pwm_dt_spec;

void alarm(const struct pwm_dt_spec *buzzer, int seconds);
void sound_1up(const struct pwm_dt_spec *buzzer);

#endif /* BUZZER_H_ */