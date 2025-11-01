#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

enum PressState {
  IDLE,
  RUNNING, 
  WAITING_FOR_REASON
};

extern PressState pressState;
extern unsigned long pressStartTime;
extern unsigned long pressStopTime;

#endif