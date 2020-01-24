#include <OneWire.h>
//OneWire ds(D4);
OneWire ds(D6);
#include <Sensor.h>

bool Sensor::searchSensor() {
  if (!ds.search(addr)) {
    ds.reset_search();
    return false;
  }
  
  if (OneWire::crc8(addr, 7) != addr[7]) return false;
  
  switch (addr[0]) {
    case 0x10:
      chip = "DS18S20";
      type_s = 1;
      break;
    case 0x28:
      chip = "DS18B20";
      type_s = 0;
      break;
    case 0x22:
      chip = "DS1822";
      type_s = 0;
      break;
    default:
      return false;
  } 

  char buffer[17];
  for (unsigned int i = 0; i < 8; i++)
  {
    byte nib1 = (addr[i] >> 4) & 0x0F;
    byte nib2 = (addr[i] >> 0) & 0x0F;
    buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
  }
  buffer[16] = '\0';
  addrStr = String(buffer);

  return true;
}

bool Sensor::crcCheck(){
  return OneWire::crc8(addr, 7) == addr[7];
}

float Sensor::getTemperature() {
  byte i;
  byte present = 0;
  byte data[12];
  
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1); // start conversion, with parasite power on at the end
  
  delay(1000); // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE); // Read Scratchpad

  for ( i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = ds.read();
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;

  json = "{\"Chip\":\"" + chip + "\",\"Code\":\"" + addrStr + "\",\"Celsius\":\"" + celsius + "\"}";

  return celsius;
}
