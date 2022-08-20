/*
  xdrv_88_princess_fan.ino - Princess Fan driver for Tasmota

  Copyright (C) 2022 Johannes Stratmann

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_PRINCESS_FAN
/*********************************************************************************************\
 * Princess Fan driver using serial connection to fan controller
\*********************************************************************************************/

#define XDRV_88                    88
#define HARDWARE_FALLBACK          2

#ifdef ESP8266
const uint16_t SERIAL_BRIDGE_BUFFER_SIZE = 64;
#else
const uint16_t SERIAL_BRIDGE_BUFFER_SIZE = INPUT_BUFFER_SIZE;
#endif


const char kPrincessFanCommands[] PROGMEM = "|"  // No prefix
  D_CMND_SSERIALSEND;

void (* const PrincessFanCommands[])(void) PROGMEM = {
  &CmndSSerialSend };

#include <TasmotaSerial.h>

TasmotaSerial *PrincessFanSerial = nullptr;

unsigned long princess_fan_polling_window = 0;
char *princess_fan_buffer = nullptr;
int princess_fan_in_byte_counter = 0;
bool princess_fan_raw = false;

/********************************************************************************************/

bool SetPrincessFanBegin(void) {
  return PrincessFanSerial->begin(9600);  // Reinitialize serial port with new baud rate
}


void PrincessFanPrintf(PGM_P formatP, ...) {
#ifdef USE_PRINCESS_FAN_TEE
  if (Settings->sbflag1.serbridge_console && princess_fan_buffer) {
    va_list arg;
    va_start(arg, formatP);
    char* data = ext_vsnprintf_malloc_P(formatP, arg);
    va_end(arg);
    if (data == nullptr) { return; }

    PrincessFanSerial->printf(data);
    free(data);
  }
#endif  // USE_SERIAL_BRIDGE_TEE
}

/********************************************************************************************/

void PrincessFanInput(void) {
  while (PrincessFanSerial->available()) {
    yield();
    uint8_t serial_in_byte = PrincessFanSerial->read();

    princess_fan_polling_window = millis();                                   // Wait for more data
  }
}

/********************************************************************************************/

void PrincessFanInit(void) {
  if (PinUsed(GPIO_PRINCESS_FAN_RX) && PinUsed(GPIO_PRINCESS_FAN_TX)) {
    PrincessFanSerial = new TasmotaSerial(Pin(GPIO_PRINCESS_FAN_RX), Pin(GPIO_PRINCESS_FAN_TX), HARDWARE_FALLBACK);
    if (SetPrincessFanBegin()) {
      if (PrincessFanSerial->hardwareSerial()) {
        ClaimSerial();
        princess_fan_buffer = TasmotaGlobal.serial_in_buffer;  // Use idle serial buffer to save RAM
      } else {
        princess_fan_buffer = (char*)(malloc(SERIAL_BRIDGE_BUFFER_SIZE));
      }
      PrincessFanSerial->flush();
      PrincessFanPrintf("\r\n");
    }
  }
}

/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

void CmndSSerialSend(void) {
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv88(uint8_t function) {
  bool result = false;

  if (FUNC_PRE_INIT == function) {
    AddLog(LOG_LEVEL_INFO, PSTR("Princess Fan start init")); 
    PrincessFanInit();
    AddLog(LOG_LEVEL_INFO, PSTR("Princess Fan init done")); 
    PrincessFanPrintf(PSTR("Princess Fan Init done"));
  }
  else if (princess_fan_buffer) {
    switch (function) {
      case FUNC_LOOP:
        PrincessFanInput();
        break;
      case FUNC_COMMAND:
        result = DecodeCommand(kPrincessFanCommands, PrincessFanCommands);
        break;
    }
  }
  return result;
}

#endif // USE_PRINCESS_FAN
