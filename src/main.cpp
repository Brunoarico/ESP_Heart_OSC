#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <OSCMessage.h>
#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#define BPM_ADDRESS "/BPM"
#define WARNING_MOCK_LED LED_BUILTIN

char ssid[] = "Teste-Lab";          // your network SSID (name)
char pass[] = "makerLab*";         // your network password

WiFiUDP Udp;

ESP8266WebServer server(80);

MAX30105 particleSensor;
bool mock = false;
const byte RATE_SIZE = 20; //Increase this for more averaging. 4 is good.
float rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred
float beatsPerMinute;
float init_val = 80;
float beatAvg = init_val;
unsigned long int last = 0;

uint8_t nodeIndex = 0;

/* YOU WILL NEED TO CHANGE THIS TO YOUR COMPUTER'S IP! */
const IPAddress outIp(192,168,0,231);        // remote IP of your computer

//this should match the port to listen on in the python sketch
const unsigned int outPort = 5005;          // remote port to receive OSC
const unsigned int localPort = 5005;        // local port to listen for OSC packets (actually not used for sending)

String mainPage(){
  String ptr = "<!DOCTYPE html>\n";
  ptr +="<html>\n";
  ptr +="<head>\n";
  ptr +="<title>Empatias mapeadas</title>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>Empatias mapeadas</h1>\n";
  ptr +="<form action=\"/submit\"method=\"POST\">\n";
  ptr +="<p>Numero do modulo:</p>\n";
  ptr +="<input type=\"textarea\" name=\"number\">\n";
  ptr +="<p>Mock?</p>\n";
  ptr +="<input type=\"checkbox\" name=\"mock\">\n";
  ptr +="<p></p>\n";
  ptr +="<input type=\"submit\" value=\"Done\">\n";
  ptr +="</form>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

void serveMainPage() {
  server.send(200, "text/html", mainPage());
}

/**
 *  readEEPROM - starts EEPROM, fetches data and reads it back from RAM
 */
void readEEPROM() {
  EEPROM.begin(1);
  Serial.printf("\n\nReading EEPROM:\n");
  EEPROM.get(0, nodeIndex);
  Serial.printf("nodeIndex = %d\n",nodeIndex);
  EEPROM.end();
}

/**
 *  saveEEPROM - starts EEPROM, stores data and reads it back from emulated EEPROM
 */
void saveEEPROM() {
  EEPROM.begin(1);
  Serial.printf("\n\nStoring EEPROM:\n");
  EEPROM.put(0, nodeIndex);
  Serial.printf("nodeIndex = %d\n", EEPROM.get(0, nodeIndex));
  EEPROM.end();
}

void handleSubmit() {
  if (server.args() > 0 ) {
    // strncpy(ssid, server.arg("ssid").c_str(), 32);
    // strncpy(password, server.arg("password").c_str(), 32);
    // address = server.arg("address").toInt();
    // FIX: AP/station mode will mark config byte
    // strncpy(projectName, server.arg("projectName").c_str(), 32);
    // server.send(200, "text/html", statusPage());
    nodeIndex = (uint8_t) server.arg("number").toInt();
    if (server.arg("mock") == "on") mock = true;
    else mock = false;

    saveEEPROM();
    // delay(2000);
    // WiFi.disconnect();
    // WiFi.softAPdisconnect(true);
    // delay(100);
    // ESP.restart();
    server.send(200, "text/plain", "Done!");
  }
  else server.send(200, "text/plain", "Empty arguments, try again!");
}

void startHTTP() {
  server.on("/", serveMainPage);
  server.on("/submit", handleSubmit);
  server.begin();
  Serial.println("HTTP server started");
}

void reconnect (){
  while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
}

void OTA_config() {
  ArduinoOTA.setHostname("empatias1");
  //ArduinoOTA.setPassword((const char *)"wannarockyou");
  ArduinoOTA.begin();
}

void send_OSC (int value) {
  char address[32];
  sprintf(address, "/BPM%d", nodeIndex);
  OSCMessage msg(address);
  msg.add(value);
  Udp.beginPacket(outIp, outPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}

void send_OSC_bang (int val) {
  char address[32];
  sprintf(address, "/BPM%d", nodeIndex);
  OSCMessage msg(address);
  msg.add(val);
  Udp.beginPacket(outIp, outPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}

void get_BPM() {
  long irValue = particleSensor.getIR();
  //Check for a beat
  if (checkForBeat(irValue) == true) {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60 / (delta / 1000.0);
    //sanity check of BPM
    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable
      //BMP average calc
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++) beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }

  //finger verification
  if (irValue > 50000) {
    //mocking hearbeat
    if(millis() - last > (1000.0 / (beatAvg/60.0))) {
      //send_OSC(beatAvg);
      send_OSC_bang(1000.0 / (beatAvg/60.0));
      Serial.println("Beat! IBI: "+  String(1000.0 / (beatAvg/60.0)));
      last = millis();
    }
  }
}

void get_BPM_mock() {
  //send_OSC(80);
  send_OSC_bang(750);
  Serial.println("send");
  delay(750);
}

void setup() {
  pinMode(WARNING_MOCK_LED, OUTPUT);
  digitalWrite(WARNING_MOCK_LED, HIGH);
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  reconnect();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30105 not found, please check connections/power");   // Report if not found
    Serial.println("Mocking...");
    digitalWrite(WARNING_MOCK_LED, LOW);
    mock = true;
  }
  else {
    particleSensor.setup(); //Configure sensor with default settings
    particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
    particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED*/
  }
  last = millis();
  readEEPROM();
  startHTTP();
  OTA_config();
}

void loop() {
  reconnect ();
  ArduinoOTA.handle();
  server.handleClient();
  if (mock) get_BPM_mock();
  else get_BPM();
}
