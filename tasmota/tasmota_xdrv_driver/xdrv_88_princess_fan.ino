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

#define USE_PRINCESS_FAN
#ifdef USE_PRINCESS_FAN
/*********************************************************************************************\
 * Princess Fan driver using serial connection to fan controller
\*********************************************************************************************/

#define XDRV_88                    88
#define HARDWARE_FALLBACK          2

#ifdef ESP8266
static const uint16_t BUFFER_SIZE = 64;
#else
static const uint16_t BUFFER_SIZE = INPUT_BUFFER_SIZE;
#endif
static constexpr uint32_t PRINCESS_FAN_PACKET_SIZE = 4;


const char kPrincessFanCommands[] PROGMEM = "|"  // No prefix
  D_CMND_SSERIALSEND;

void (* const PrincessFanCommands[])(void) PROGMEM = {
  &CmndPrincessSend };

#include <TasmotaSerial.h>

static TasmotaSerial *PrincessFanSerial = nullptr;

static unsigned long polling_window = 0;
static char *buffer = nullptr;
static int in_byte_counter = 0;

/********************************************************************************************/

bool SetPrincessFanBegin(void) {
  return PrincessFanSerial->begin(9600);  // Reinitialize serial port with new baud rate
}


void PrincessFanPrintf(PGM_P formatP, ...) {
  if (Settings->sbflag1.serbridge_console && buffer) {
    va_list arg;
    va_start(arg, formatP);
    char* data = ext_vsnprintf_malloc_P(formatP, arg);
    va_end(arg);
    if (data == nullptr) { return; }

    PrincessFanSerial->printf(data);
    free(data);
  }
}

/********************************************************************************************/
void decode_fan_message() {
  
}

void PrincessFanInput(void) {
  while (PrincessFanSerial->available()) {
    yield();
    uint8_t serial_in_byte = PrincessFanSerial->read();

    if (in_byte_counter == 0 && serial_in_byte != 0xaa) {   // sync, first byte must be 0xaa
      continue;
    }

    if (in_byte_counter < PRINCESS_FAN_PACKET_SIZE) {       // Add char to string if it still fits
      buffer[in_byte_counter++] = serial_in_byte;
    } 
    
    if (in_byte_counter == PRINCESS_FAN_PACKET_SIZE) {      // message complete
      decode_fan_message();

      Response_P(PSTR("{\"" D_JSON_FANSERIALRECEIVED "\":"));
      ResponseAppend_P("0x%02x 0x%02x 0x%02x 0x%02x ", buffer[0], buffer[1], buffer[2], buffer[3]);
      ResponseJsonEnd();


      MqttPublishPrefixTopicRulesProcess_P(RESULT_OR_TELE, PSTR(D_JSON_FANSERIALRECEIVED));
      in_byte_counter = 0;
    }

    polling_window = millis();                 // Wait for more data
  }
}

/********************************************************************************************/

void PrincessFanInit(void) {
  AddLog(LOG_LEVEL_DEBUG, PSTR("Princess Fan check serial pins")); 
  if (PinUsed(GPIO_PRINCESS_FAN_RX) && PinUsed(GPIO_PRINCESS_FAN_TX)) {
    PrincessFanSerial = new TasmotaSerial(Pin(GPIO_PRINCESS_FAN_RX), Pin(GPIO_PRINCESS_FAN_TX), HARDWARE_FALLBACK);
    AddLog(LOG_LEVEL_DEBUG, PSTR("Princess Fan create serial")); 
    if (SetPrincessFanBegin()) {
      if (PrincessFanSerial->hardwareSerial()) {
        AddLog(LOG_LEVEL_DEBUG, PSTR("Princess Fan start hardware serial")); 
        ClaimSerial();
        buffer = TasmotaGlobal.serial_in_buffer;  // Use idle serial buffer to save RAM
      } else {
        buffer = (char*)(malloc(BUFFER_SIZE));
      }
      PrincessFanSerial->flush();
      PrincessFanPrintf("hello princess\r\n");
    }
  }
}

/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

void CmndPrincessSend(void) {
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
    PrincessFanPrintf(PSTR("Princess Fan message to serial"));
  }
  else if (buffer) {
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
