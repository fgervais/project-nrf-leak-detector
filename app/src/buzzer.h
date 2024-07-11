#ifndef BUZZER_H_
#define BUZZER_H_

struct pwm_dt_spec;

void sound_1up();
int buzzer_alarm(const struct pwm_dt_spec *buzzer, int seconds);
bool buzzer_is_running(const struct pwm_dt_spec *buzzer);
int buzzer_init(const struct pwm_dt_spec *buzzer);

#endif /* BUZZER_H_ */