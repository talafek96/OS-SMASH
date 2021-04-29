#ifndef SMASH__SIGNALS_H_
#define SMASH__SIGNALS_H_

void ctrlZHandler(int sig_num); // Stop signal
void ctrlCHandler(int sig_num); // Kill signal
void alarmHandler(int sig_num);

#endif //SMASH__SIGNALS_H_
