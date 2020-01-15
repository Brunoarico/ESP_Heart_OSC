#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>

#define MY_ID "/BPM1"
#define WARNING_MOCK_LED LED_BUILTIN

char ssid[] = "Teste-Lab";          // your network SSID (name)
char pass[] = "makerLab*";         // your network password

WiFiUDP Udp;

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


/* YOU WILL NEED TO CHANGE THIS TO YOUR COMPUTER'S IP! */
const IPAddress outIp(192,168,0,231);        // remote IP of your computer

//this should match the port to listen on in the python sketch
const unsigned int outPort = 5005;          // remote port to receive OSC
const unsigned int localPort = 5005;        // local port to listen for OSC packets (actually not used for sending)

void reconnect (){
  while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
}

void send_OSC (int value) {
  OSCMessage msg(MY_ID);
  msg.add(value);
  Udp.beginPacket(outIp, outPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}

void send_OSC_bang () {
  OSCMessage msg(MY_ID);
  msg.add("800 1");
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
      send_OSC_bang();
      Serial.println("Beat! " + String(millis() - last) + " "+  String(1000.0 / (beatAvg/60.0)));
      last = millis();
    }
  }
}

void get_BPM_mock() {
  //send_OSC(80);
  send_OSC_bang();
  Serial.println("send");
  delay(750);
}

void setup() {
  pinMode(WARNING_MOCK_LED, OUTPUT);
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  reconnect ();
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
}

void loop() {
  reconnect ();
  if(mock) get_BPM_mock();
  else get_BPM();

}
