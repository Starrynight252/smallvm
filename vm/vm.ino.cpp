# 1 "/tmp/tmpk1bd22en"
#include <Arduino.h>
# 1 "/home/renard/Code/uBlocks/smallvm/vm/vm.ino"






#include "mem.h"
#include "interp.h"
#include "persist.h"
void setup();
void loop();
#line 11 "/home/renard/Code/uBlocks/smallvm/vm/vm.ino"
void setup() {
#ifdef ARDUINO_NRF52_PRIMO
 sd_softdevice_disable();
#endif
 memInit();
 primsInit();
 hardwareInit();
 outputString((char *) "Welcome to MicroBlocks!");
 restoreScripts();
 if (BLE_isEnabled()) BLE_start();
 startAll();
}

void loop() {
 vmLoop();
}