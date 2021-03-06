#include "RtcDS3231.h"
#include "Wire.h"
#include <time.h> // Estas bibliotecas são exclusivas do sistema. Estão presentes nos packets hardware ESP32.
#include <sys/time.h> // Utilizam o contador interno do EPS32.
#include "WiFi.h"
#include "TinyGPS.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "LoRa.h"
#include "dht22.h"
#include "Adafruit_ADS1015.h" // modificado a bilioteca em #include <Wire.h> para #include "Wire.h"
//ESP.restart();
//////////////////////////////// Wifi configuration ////////////////////////////////
const char* ssid     = "Alpha";//"Your_SSID"; // Insira aqui os dados de sua rede Wi-Fi
const char* password = "BattleFieldBadCompany28";//"Your_PASSWORD";
///////////////////////////////// Time configuration ///////////////////////////////
long timezone = -4;
struct tm timeinfo;
unsigned long dly = 0 ;
////////////////////////////// SD Card configuration ///////////////////////////////
#define SD_CS 27 //default 15 
#define SD_SCK 14 // default 14
#define SD_MOSI 12 // default 13
#define SD_MISO 13 // 12
SPIClass sd_spi(HSPI);
String SendMsgData = "\0";
String SaveMsgData = "\0";
String MsgPacket = "\0";
String DataDht22 = "\0";
String TimeLocal = "\0";
String MsgErro = "\0";
String Erro = "\0";
/////////////////////////////////// Functions //////////////////////////////////////
void LoRaTask(void *Parametro);
void appendFile(fs::FS &fs, const char * path, const char * message);
void GetTimeLocal();
void ErrorMsg();
void cbk(int packetSize);
double temp(double x);
void Pt100();
void Dht22();
void GPSdata();
/////////////////////////////// Lora configuration /////////////////////////////////
#define BAND    433E6
#define PABOOST true
#define LoRa_SS 5
#define LoRa_MOSI 18
#define LoRa_MISO 19
#define LoRa_SCK 21
#define LoRa_RST 26
#define Lora_DIO0 15
/////////////////////////////// RTC configuration /////////////////////////////////
RtcDS3231<TwoWire> Rtc(Wire);
/////////////////////////////// ADS configuration /////////////////////////////////
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
float Vin, R1, Vref = 0;
float R2 = 214.9;//218.91;
int16_t adc0, adc1 = 0;
String temp100;
/////////////////////////////// GPS configuration /////////////////////////////////
TinyGPS gps;
String SinalGPS = "\0";
////////////////////////////// DHT22 configuration ////////////////////////////////
dht22 dht01(26); // cria objeto dht01 e indica a porta para leitura do sensor Ex. dht22 NomeDoObjeto(nº_do_pino)
dht22 dht02(25);
dht22 dht03(33);


void setup() {
  ///////////////////////////////////// UART Init ////////////////////////////////////
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 4, 15); //rx , tx - GPS
  Serial.flush();
  delay(50);
  String DateCompile = ("Compiled in: " + String(__DATE__) + " " + String(__TIME__) + "\n");
  Serial.println(DateCompile);
  pinMode(2, OUTPUT);
  ////////////////////////////////// SD Card Init ////////////////////////////////////
  sd_spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sd_spi)) {
    Serial.println("Card Mount Failed");
    digitalWrite(2, HIGH);
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    digitalWrite(2, HIGH);
    return;
  }
  ///////////////////////////////////// Wifi Init ////////////////////////////////////
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  delay(1000);
  for (int i = 0; i < 3; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      digitalWrite(2, HIGH);
      delay(1000);
      digitalWrite(2, LOW);
      Serial.println("WiFi connected");
      MsgErro = "WiFi connected. ";
      appendFile(SD, "/logTX.txt", MsgErro.c_str());
      MsgErro = "\0";
      MsgErro = "IP address: ";
      MsgErro += WiFi.localIP().toString().c_str();
      MsgErro += "\n";
      Serial.print(MsgErro);
      appendFile(SD, "/logTX.txt", MsgErro.c_str());
      MsgErro = "\0";
      break;
    }
    Serial.print("Reconnecting to ");
    Serial.print(ssid);
    long inicio = (micros() + 10000000);
    while (inicio > micros()) {
      //if (wifi != "CONNECTED") {
      if (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      } else break;
    }
    Serial.println("\nConnection fail");
    MsgErro = "Wi-Fi Connection fail\n";
    appendFile(SD, "/logTX.txt", MsgErro.c_str());
    MsgErro = "\0";
  }
  ///////////////////////////////////// RTC Init ////////////////////////////////////
  Wire.begin(16, 17); //SDA, SCL
  Rtc.Begin();
  if (WiFi.status() != WL_CONNECTED) {
jump:
    MsgErro = "RTC used time\n";
    appendFile(SD, "/logTX.txt", MsgErro.c_str());
    MsgErro = "\0";
    timeval tv;//Cria a estrutura temporaria para funcao abaixo.
    tv.tv_sec = (Rtc.GetDateTime() + 946684800);//Atribui a data atual.
    settimeofday(&tv, NULL);//Configura o RTC para manter a data atribuida
    if ((RtcDateTime(__DATE__, __TIME__) + 120) > Rtc.GetDateTime()) {
      appendFile(SD, "/logTX.txt", DateCompile.c_str());
      appendFile(SD, "/dataTX.txt", "Date Sist,Time Sis,LAT,LOG,SAT,Precisão,Altura,Date GPS,Time GPS\n");
    }
  }
  else {
    Serial.println("Contacting Time Server");
    int cont = 0;
repeat:
    configTime(3600 * timezone, 3600, "pool.ntp.org");
    getLocalTime(&timeinfo, 5000);
    Serial.println(&timeinfo, "Now is: %A, %B %d %Y %H:%M:%S");
    int ano = timeinfo.tm_year + 1900;
    int mes = timeinfo.tm_mon + 1;
    int dia = timeinfo.tm_mday;
    int hora = timeinfo.tm_hour;
    int minuto = timeinfo.tm_min;
    int segundo = timeinfo.tm_sec;
    cont++;
    if (cont > 10)goto jump;
    if (ano < 2020) goto repeat;
    RtcDateTime HorarioServer = RtcDateTime(ano, mes, dia, hora, minuto, segundo);
    if (!Rtc.IsDateTimeValid()) {
      if (Rtc.LastError() != 0) {
        MsgErro = "RTC communications error = " + Rtc.LastError();
        appendFile(SD, "/logTX.txt", MsgErro.c_str());
        MsgErro = "\0";
        Serial.print("RTC communications error = ");
        Serial.println(Rtc.LastError());
      }
      else {
        MsgErro = "RTC lost confidence in the DateTime!";
        appendFile(SD, "/logTX.txt", MsgErro.c_str());
        MsgErro = "\0";
        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(HorarioServer);
      }
    }
    if (!Rtc.GetIsRunning()) {
      MsgErro = "RTC was not actively running, starting now";
      appendFile(SD, "/logTX.txt", MsgErro.c_str());
      MsgErro = "\0";
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
    }
    if ( ((Rtc.GetDateTime() + 60) < HorarioServer) && (ano >= 2020))  {
      MsgErro = "RTC is older than server time!  (Updating DateTime)";
      appendFile(SD, "/logTX.txt", MsgErro.c_str());
      MsgErro = "\0";
      Serial.println("RTC is older than server time!  (Updating DateTime)");
      Rtc.SetDateTime(HorarioServer);
    }
    if ( (Rtc.GetDateTime()  > (HorarioServer + 60)) && (ano >= 2020))  {
      MsgErro = "RTC is advanced than server time!  (Updating DateTime)";
      appendFile(SD, "/logTX.txt", MsgErro.c_str());
      MsgErro = "\0";
      Serial.println("RTC is advanced than server time!  (Updating DateTime)");
      Rtc.SetDateTime(HorarioServer);
    }
    if ((RtcDateTime(__DATE__, __TIME__) + 120) > HorarioServer) {
      appendFile(SD, "/logTX.txt", DateCompile.c_str());
      appendFile(SD, "/gpsTX.txt", "LAT,LOG,SAT,Precisão,Altura,Date GPS,Time GPS\n");

    }
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  //////////////////////////////////// LoRa Init /////////////////////////////////////
  SPI.begin(LoRa_SCK, LoRa_MISO, LoRa_MOSI, LoRa_SS);
  LoRa.setPins(LoRa_SS, LoRa_RST, Lora_DIO0); // default (5,14,26)
  if (!LoRa.begin(BAND, PABOOST)) {
    Serial.print("Starting LoRa failed!\r\n");
    MsgErro = "Starting LoRa failed!";
    ErrorMsg();
    digitalWrite(2, HIGH);
    delay(10000);
    digitalWrite(2, LOW);
  }
  else {
    //LoRa.setSpreadingFactor(11); //default SF11 {6-12}
    Serial.print("LoRa Initial success!\r\n");
    MsgErro = "LoRa Initial success!";
    ErrorMsg();
  }
  ///////////////////////////////// Create task LoRa /////////////////////////////////
  xTaskCreatePinnedToCore (LoRaTask, "LoRa", 1024, NULL, 0, NULL, 0);
  ///////////////////////////////// Config ADS1115 ///////////////////////////////////
  Wire.begin(22, 23);
  ads.setGain(GAIN_ONE);// 1x gain   +/- 4.096V  1 bit = 0.125mV
  ads.begin();
  //////////////////////////////////// END SETUP /////////////////////////////////////
  GetTimeLocal();
  MsgErro = ("Start system in: " + TimeLocal + "\n");
  appendFile(SD, "/logTX.txt", MsgErro.c_str());
  MsgErro = "\0";
}
void loop()
{
  dly = micros();
  GPSdata();
  Pt100 ();
  Dht22();
  GetTimeLocal();
  SaveMsgData = (TimeLocal + "," + DataDht22 + "," + temp100 + "," + SinalGPS + "\n");
  appendFile(SD, "/DataTX.txt", SaveMsgData.c_str());
  dly = (micros() - dly) / 1000;
  if (dly < 60000) delay(60000 - dly);
}

void LoRaTask(void *Parametro)
{
  (void) Parametro;
  int counter = 0;
  for (;;) // A Task shall never return or exit.
  {
    if (SendMsgData != MsgPacket)
    {
      digitalWrite(2, HIGH);
      LoRa.beginPacket();
      LoRa.print(SendMsgData);
      LoRa.endPacket();
      Serial.print(SendMsgData);
      MsgPacket = SendMsgData;
      digitalWrite(2, LOW);
    }
  }
}

void ErrorMsg() {
  if (MsgErro != Erro) {
    Erro = MsgErro;
    GetTimeLocal();
    TimeLocal += " => ";
    TimeLocal += MsgErro + "\n";
    appendFile(SD, "/logTX.txt", TimeLocal.c_str());
    TimeLocal = "\0";
  }
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  sd_spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sd_spi)) {
    Serial.println("Card Mount Failed");
    digitalWrite(2, HIGH);
    while (true);
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    digitalWrite(2, HIGH);
    while (true);
  }
  //Serial.printf("Appending to file: %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    digitalWrite(2, HIGH);
    while (true);
  }
  if (file.print(message)) {
    //Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
    digitalWrite(2, HIGH);
    while (true);
  }
  file.close();
}

void GetTimeLocal()
{
  if (!getLocalTime(&timeinfo)) {
    MsgErro = "Failed to obtain time";
    ErrorMsg();
    return;
  }
  String dia = String (timeinfo.tm_mday);
  if ((timeinfo.tm_mday) < 10) dia = + "0" + dia;
  String mes = String (timeinfo.tm_mon + 1);
  if ((timeinfo.tm_mon + 1) < 10) mes = "0" + mes;
  String ano = String (timeinfo.tm_year + 1900); //1900
  ano = ano.substring(2, 4);
  String hora = String (timeinfo.tm_hour);
  if ((timeinfo.tm_hour) < 10) hora = "0" + hora;
  String minuto = String (timeinfo.tm_min);
  if ((timeinfo.tm_min) < 10) minuto = "0" + minuto;
  String segundo = String (timeinfo.tm_sec);
  if ((timeinfo.tm_sec) < 10) segundo = "0" + segundo;
  TimeLocal = dia + "/" + mes + "/" + ano + "," + hora + ":" + minuto + ":" + segundo;
  //Serial.println(TimeLocal);
}

void Dht22() {
  dht01.dht22Data();
  DataDht22 = String(dht01.Temp) + ",";
  DataDht22 += String(dht01.Hum) + ",";
  if (dht01.MsgError != "SuccessData") {
    MsgErro = dht01.MsgError + ":dht01";
    ErrorMsg();
  }
  dht02.dht22Data();
  DataDht22 += String(dht02.Temp) + ",";
  DataDht22 += String(dht02.Hum) + ",";
  if (dht02.MsgError != "SuccessData") {
    MsgErro = dht02.MsgError + ":dht02";
    ErrorMsg();
  }
  dht03.dht22Data();
  DataDht22 += String(dht03.Temp) + ",";
  DataDht22 += String(dht03.Hum) + ",";
  if (dht03.MsgError != "SuccessData") {
    MsgErro = dht03.MsgError + ":dht03";
    ErrorMsg();
  }
}

void Pt100() {
  int cont = 0;
rpt:
  //Serial.println ("repitiu");
  adc0 = ads.readADC_SingleEnded(0);
  adc1 = ads.readADC_SingleEnded(1);
  Vref = adc1;
  Vref = Vref * 0.0001309 * 2;
  Vin = adc0;
  Vin = Vin * 0.000125;
  R1 = (Vin * R2) / (Vref - Vin);
  cont++;
  if (cont > 50)goto nan;
  if ( R1 < 90 || R1 > 130 )goto rpt;
nan:
  if (cont > 50)temp100 = "nan";
  else temp100 = (String)temp(R1);
  //printf("Tensão: %.3fV - Tensão Ref: %.3fV - Resistência: %.2fOhms - Temperatura: %.2fºC\n", Vin, Vref, R1, temp(R1));
  //printf("Tensão do Sistema: %.3fV\n", Vref);
}

// correção retirada de Copyright (c) 2019, P. Lutus -- http://arachnoid.com. All Rights Reserved.
double temp(double x) {
  double terms[] = {
    -4.4277499439427243e+002,
    6.9012656109555630e+000,
    -5.4203211695891609e-004,
    -5.4242492920976264e-004,
    1.4493080701111702e-006,
    1.7821172291658785e-008,
    1.7227205127942579e-010,
    -1.8484057190693898e-012,
    -1.2045312329567013e-014,
    1.5940889037491267e-016,
    -6.8429376874253937e-019,
    2.5960304042778199e-021,
    -6.5642792240198388e-024
  };
  double t = 1;
  double r = 0;
  for (double c : terms) {
    r += c * t;
    t *= x;
  }
  return r;
}

void GPSdata()
{
  bool newData = false;
  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (Serial2.available())
    {
      char c = Serial2.read();
      // Serial.write(c); // uncomment this line if you want to see the GPS data flowing
      if (gps.encode(c)) // Did a new valid sentence come in?
        newData = true;
    }
  }
  if (newData)
  {
    float flat, flon, alt;
    int year;
    byte month, day, hour, minute, second, hundredths, sat, prec;
    unsigned long age;
    gps.f_get_position(&flat, &flon, &age);
    gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
    sat = gps.satellites();
    prec = gps.hdop();
    alt = gps.f_altitude();
    if (hour > 3)hour = (hour - 4);
    else {
      hour = (hour + 20);
      day = (day - 1);
    }
    char sz[60];
    sprintf(sz, "%f,%f,%d,%d,%0.2f,%02d/%02d/%02d,%02d:%02d:%02d",
            flat, flon, sat, prec, alt, day, month, year, hour, minute, second);
    SinalGPS = sz;
    SinalGPS += "\n";
    appendFile(SD, "/gpsTX.txt", SinalGPS.c_str());
    char sz1[21];
    sprintf(sz1, "%f,%f",
            flat, flon);
    SinalGPS = sz1;
  }
}
