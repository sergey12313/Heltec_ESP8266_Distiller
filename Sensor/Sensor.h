class Sensor {
  public:
  byte addr[8];
  String chip;
  String addrStr;
  String json;
  byte type_s;
  float celsius;
  
  bool searchSensor();
  bool crcCheck();
  float getTemperature();
};

