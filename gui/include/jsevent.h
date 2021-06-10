#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool buttonA;
  bool buttonB;
  bool buttonX;
  bool buttonY;
  bool buttonLeft;
  bool buttonRight;
  bool buttonUp;
  bool buttonDown;
  bool buttonL1;
  bool buttonR1;
  bool buttonL3;
  bool buttonR3;
  bool buttonStart;
  bool buttonSelect;
  bool buttonGuide;
  bool _dummy;
  bool buttonL2;
  bool buttonR2;
  short axisLeftX;
  short axisLeftY;
  short axisRightX;
  short axisRightY;
}JSEvent_Struct;
