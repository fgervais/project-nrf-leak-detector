#ifndef BUZZER_H_
#define BUZZER_H_

struct pwm_dt_spec;

void smb3_sound_1up(void);
void smb3_sound_enter_world(void);
void smd3_sound_game_over(void);
void smb2_sound_game_over(void);
void smb2_main_theme(void);

int buzzer_alarm(int seconds);
bool buzzer_is_running(const struct pwm_dt_spec *spec);
int buzzer_init(const struct pwm_dt_spec *spec);

#endif /* BUZZER_H_ */