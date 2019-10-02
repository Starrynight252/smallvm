/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2018 John Maloney, Bernat Romagosa, and Jens Mönig

// sensorPrims.cpp - Microblocks I2C, SPI, tilt, and temperature primitives
// John Maloney, May 2018

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <stdio.h>

#include "mem.h"
#include "interp.h"

// i2c helper functions

static int wireStarted = false;

static void startWire() {
	Wire.begin();
	Wire.setClock(400000); // i2c fast mode (seems pretty ubiquitous among i2c devices)
	wireStarted = true;
}

static int readI2CReg(int deviceID, int reg) {
	if (!wireStarted) startWire();
	Wire.beginTransmission(deviceID);
	Wire.write(reg);
	#if defined(ARDUINO_ARCH_ESP32)
		int error = Wire.endTransmission();
	#else
		int error = Wire.endTransmission((bool) false);
	#endif
	if (error) return -error; // error; bad device ID?

	Wire.requestFrom(deviceID, 1);
	while (!Wire.available());
	return Wire.read();
}

static void writeI2CReg(int deviceID, int reg, int value) {
	if (!wireStarted) startWire();
	Wire.beginTransmission(deviceID);
	Wire.write(reg);
	Wire.write(value);
	Wire.endTransmission();
}

// i2c prims

OBJ primI2cGet(OBJ *args) {
	if (!isInt(args[0]) || !isInt(args[1])) return fail(needsIntegerError);
	int deviceID = obj2int(args[0]);
	int registerID = obj2int(args[1]);
	if ((deviceID < 0) || (deviceID > 128)) return fail(i2cDeviceIDOutOfRange);
	if ((registerID < 0) || (registerID > 255)) return fail(i2cRegisterIDOutOfRange);

	return int2obj(readI2CReg(deviceID, registerID));
}

OBJ primI2cSet(OBJ *args) {
	if (!isInt(args[0]) || !isInt(args[1]) || !isInt(args[2])) return fail(needsIntegerError);
	int deviceID = obj2int(args[0]);
	int registerID = obj2int(args[1]);
	int value = obj2int(args[2]);
	if ((deviceID < 0) || (deviceID > 128)) return fail(i2cDeviceIDOutOfRange);
	if ((registerID < 0) || (registerID > 255)) return fail(i2cRegisterIDOutOfRange);
	if ((value < 0) || (value > 255)) return fail(i2cValueOutOfRange);

	writeI2CReg(deviceID, registerID, value);
	return falseObj;
}

static OBJ primI2cRead(int argCount, OBJ *args) {
	// Read multiple bytes from the given I2C device into the given array and return the
	// number of bytes read. The array size determines the number of bytes to read (up to a
	// max of 32). This operation is usually preceded by an I2C write to request some data.

	if ((argCount < 2) || !isInt(args[0])) return int2obj(0);
	int deviceID = obj2int(args[0]);
	OBJ array = args[1];
	if (!IS_CLASS(array, ArrayClass)) return int2obj(0);

	if (!wireStarted) startWire();
	int count = objWords(array);
	if (count > 32) count = 32; // the Arduino Wire library limits reads to a max of 32 bytes
	Wire.requestFrom(deviceID, count);
	for (int i = 0; i < count; i++) {
		while (!Wire.available()) /* wait for data */;
		int byte = Wire.read();
		FIELD(array, i) = int2obj(byte);
	}
	return int2obj(count);
}

static OBJ primI2cWrite(int argCount, OBJ *args) {
	// Write one or multiple bytes to the given I2C device. If the second argument is an
	// integer, write it as a single byte. If it is an array of bytes, write those bytes.
	// The array should contain integers in the range 0..255; anything else will be skipped.

	if ((argCount < 2) || !isInt(args[0])) return int2obj(0);
	int deviceID = obj2int(args[0]);
	OBJ data = args[1];

	if (!wireStarted) startWire();
	Wire.beginTransmission(deviceID);
	if (isInt(data)) {
		Wire.write(obj2int(data) & 255);
	} else if (IS_CLASS(data, ArrayClass)) {
		int count = objWords(data);
		for (int i = 0; i < count; i++) {
			OBJ item = FIELD(data, i);
			if (isInt(item)) {
				Wire.write(obj2int(item) & 255);
			}
		}
	}
	Wire.endTransmission();
	return falseObj;
}

// SPI prims

static void initSPI() {
	setPinMode(13, OUTPUT);
	setPinMode(14, OUTPUT);
	setPinMode(15, INPUT);
	SPI.begin();
	SPI.setClockDivider(SPI_CLOCK_DIV16);
}

OBJ primSPISend(OBJ *args) {
	if (!isInt(args[0])) return fail(needsIntegerError);
	unsigned data = obj2int(args[0]);
	if (data > 255) return fail(i2cValueOutOfRange);
	initSPI();
	SPI.transfer(data); // send data byte to the slave
	return falseObj;
}

OBJ primSPIRecv(OBJ *args) {
	initSPI();
	int result = SPI.transfer(0); // send a zero byte while receiving a data byte from slave
	return int2obj(result);
}

#if defined(ARDUINO_BBC_MICROBIT) || defined(ARDUINO_SINOBIT)

typedef enum {
	accel_unknown = -1,
	accel_none = 0,
	accel_MMA8653 = 1,
	accel_LSM303 = 2,
	accel_FXOS8700 = 3,
} AccelerometerType_t;

static AccelerometerType_t accelType = accel_unknown;

#define MMA8653_ID 29
#define LSM303_ID 25
#define FXOS8700_ID 30

static void detectAccelerometer() {
	if (0x5A == readI2CReg(MMA8653_ID, 0x0D)) {
		accelType = accel_MMA8653;
		writeI2CReg(MMA8653_ID, 0x2A, 1);
	} else if (0x33 == readI2CReg(LSM303_ID, 0x0F)) {
		accelType = accel_LSM303;
		writeI2CReg(LSM303_ID, 0x20, 0x5F); // 100 Hz sample rate, low power, all axes
	} else if (0xC7 == readI2CReg(FXOS8700_ID, 0x0D)) {
		accelType = accel_FXOS8700;
		writeI2CReg(FXOS8700_ID, 0x2A, 0); // turn off chip before configuring
		writeI2CReg(FXOS8700_ID, 0x2A, 0x1B); // 100 Hz sample rate, fast read, turn on
	} else {
		accelType = accel_none;
	}
}

static int readAcceleration(int registerID) {
	if (accel_unknown == accelType) detectAccelerometer();
	int sign = -1;
	int val = 0;
	switch (accelType) {
	case accel_MMA8653:
		val = readI2CReg(MMA8653_ID, registerID);
		break;
	case accel_LSM303:
		if (1 == registerID) { val = readI2CReg(LSM303_ID, 0x29); sign = 1; } // x-axis
		if (3 == registerID) val = readI2CReg(LSM303_ID, 0x2B); // y-axis
		if (5 == registerID) val = readI2CReg(LSM303_ID, 0x2D); // z-axis
		break;
	case accel_FXOS8700:
		val = readI2CReg(FXOS8700_ID, registerID);
		break;
	default:
		val = 0;
		break;
	}
	val = (val >= 128) ? (val - 256) : val; // value is a signed byte
	if (val < -127) val = -127; // keep in range -127 to 127
	val = sign * ((val * 100) / 127); // scale to range 0-100 and multiply by sign
	return val;
}

static int readTemperature() {
	volatile int *startReg = (int *) 0x4000C000;
	volatile int *readyReg = (int *) 0x4000C100;
	volatile int *tempReg = (int *) 0x4000C508;

	*startReg = 1;
	while (!(*readyReg)) { /* busy wait */ }
	return (*tempReg / 4) - 6; // callibrated at 26 degrees C using average of 3 micro:bits
}

#elif defined(ARDUINO_CALLIOPE_MINI)

static int readAcceleration(int registerID) {
	int val = 0;
	if (1 == registerID) val = readI2CReg(ACCEL_ID, 5); // x-axis
	if (3 == registerID) val = readI2CReg(ACCEL_ID, 3); // y-axis
	if (5 == registerID) val = readI2CReg(ACCEL_ID, 7); // z-axis

	val = (val >= 128) ? (val - 256) : val; // value is a signed byte
	if (val < -127) val = -127; // keep in range -127 to 127
	val = -((val * 100) / 127); // invert sign and scale to range 0-100
	if (5 == registerID) val = -val; // invert z-axis
	return val;
}

static int readTemperature() {
	int fudgeFactor = 2;
	return (readI2CReg(ACCEL_ID, 8) / 2) + 23 - fudgeFactor;
}

#elif defined(ARDUINO_SAMD_CIRCUITPLAYGROUND_EXPRESS)

#define LIS3DH_ID 25

static int accelStarted = false;

static int readAcceleration(int registerID) {
	if (!accelStarted) {
		Wire1.begin(); // use internal I2C bus
		// turn on the accelerometer
		Wire1.beginTransmission(LIS3DH_ID);
		Wire1.write(0x20);
		Wire1.write(0x7F);
		Wire1.endTransmission();
		accelStarted = true;
	}
	Wire1.beginTransmission(LIS3DH_ID);
	Wire1.write(0x28 + registerID);
	int error = Wire1.endTransmission(false);
	if (error) return 0; // error; return 0

	Wire1.requestFrom(LIS3DH_ID, 1);
	while (!Wire1.available());
	int val = Wire1.read();

	val = (val >= 128) ? (val - 256) : val; // value is a signed byte
	if (val < -127) val = -127; // keep in range -127 to 127
	val = ((val * 100) / 127); // scale to range 0-100
	if (1 == registerID) val = -val; // invert sign for x axis
	return val;
}

static int readTemperature() {
	// Return the temperature in Celcius

	setPinMode(A9, INPUT);
	int adc = analogRead(A9);

	return ((int) (0.116 * adc)) - 37; // linear approximation

	// The following unused code does not seem as accurate as the linear approximation
	// above (based on comparing the thermistor to a household digital thermometer).
	// See https://learn.adafruit.com/thermistor/using-a-thermistor
	// The following constants come from the NCP15XH103F03RC thermister data sheet:
	#define SERIES_RESISTOR 10000
	#define RESISTANCE_AT_25C 10000
	#define B_CONSTANT 3380

	if (adc < 1) adc = 1; // avoid divide by zero (although adc should never be zero)
	float r = ((1023 * SERIES_RESISTOR) / adc) - SERIES_RESISTOR;

	float steinhart = log(r / RESISTANCE_AT_25C) / B_CONSTANT;
	steinhart += 1.0 / (25 + 273.15); // add 1/T0 (T0 is 25C in Kelvin)
	float result = (1.0 / steinhart) - 273.15; // steinhart is 1/T; invert and convert to C

	return (int) round(result);
}

#elif defined(ARDUINO_CITILAB_ED1)

#define LIS3DH_ID 25

static int accelStarted = false;

static int readAcceleration(int registerID) {
	if (!accelStarted) {
		writeI2CReg(LIS3DH_ID, 0x20, 0x7F); // turn on accelerometer, 400 Hz update, 8-bit
		writeI2CReg(LIS3DH_ID, 0x1F, 0xC0); // enable temperature reporting
		accelStarted = true;
	}
	int val = readI2CReg(LIS3DH_ID, 0x28 + registerID);
	val = (val >= 128) ? (val - 256) : val; // value is a signed byte
	if (val < -127) val = -127; // keep in range -127 to 127
	val = ((val * 100) / 127); // scale to range 0-100
	val = -val; // invert sign for all axes
	return val;
}

static int readTemperature() {
	if (!accelStarted) readAcceleration(1); // initialize accelerometer if necessary

	writeI2CReg(LIS3DH_ID, 0x23, 0x80); // enable block data update (needed for temperature)
	int hiByte = readI2CReg(LIS3DH_ID, 0x0D);
	int lowByte = readI2CReg(LIS3DH_ID, 0x0C);
	writeI2CReg(LIS3DH_ID, 0x23, 0); // disable block data update
	int offsetDegreesC;

	if (hiByte <= 127) { // positive offset
		offsetDegreesC = hiByte + ((lowByte >= 128) ? 1 : 0); // round up
	} else { // negative offset
		offsetDegreesC = (hiByte - 256) + ((lowByte >= 128) ? -1 : 0); // round down
	}
	return 20 + offsetDegreesC;
}

#else // stubs for non-micro:bit boards

static int readAcceleration(int reg) { return 0; }
static int readTemperature() { return 0; }

#endif // micro:bit primitve support

OBJ primMBTiltX(int argCount, OBJ *args) { return int2obj(readAcceleration(1)); }
OBJ primMBTiltY(int argCount, OBJ *args) { return int2obj(readAcceleration(3)); }
OBJ primMBTiltZ(int argCount, OBJ *args) { return int2obj(readAcceleration(5)); }
OBJ primMBTemp(int argCount, OBJ *args) { return int2obj(readTemperature()); }

// Capacitive Touch Primitives for ARDUINO_IOT_BUS

#ifdef ARDUINO_IOT_BUS

static OBJ primTouchRead(int argCount, OBJ *args) {
	return int2obj(touchRead(obj2int(args[0])));
}

#else // stubs for non-ARDUINO_IOT_BUS boards

static OBJ primTouchRead(int argCount, OBJ *args) { return int2obj(0); }

#endif // Capacitive Touch Primitives

static PrimEntry entries[] = {
	"temperature", primMBTemp,
	"tiltX", primMBTiltX,
	"tiltY", primMBTiltY,
	"tiltZ", primMBTiltZ,
	"touchRead", primTouchRead,
	"i2cRead", primI2cRead,
	"i2cWrite", primI2cWrite,
};

void addSensorPrims() {
	addPrimitiveSet("sensors", sizeof(entries) / sizeof(PrimEntry), entries);
}
