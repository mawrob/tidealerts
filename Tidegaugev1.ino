/*
  Send range and temperature data to ThingSpeak using cellular modem
  
  ThingSpeak ( https://www.thingspeak.com ) is a free IoT service for prototyping
  systems that collect, analyze, and react to their environments.

*/
// Acknowlegements:
// This sketch includes sample code from https://github.com/sparkfun/MG2639_Cellular_Shield/tree/V_1.0
// One wire code from https://github.com/PaulStoffregen/OneWire
// Maxbotix code from https://github.com/Diaoul/arduino-Maxbotix/blob/master/Maxbotix.cpp

// The SparkFun MG2639 Cellular Shield uses SoftwareSerial
// to communicate with the MG2639 module. Include that
// library first:
#include <SoftwareSerial.h>

/* Include the MG2639 Cellular Shield library
  **** NOTE *****
  The SFE_MG2639CellShield library does not work with the Mega 2560
  without modification because the library uses SoftwareSerial on
  pins 2 and 3 of the Mega 2560.  SoftwareSerial requires pins with
  hardware interrupt capability and not all pins on the Mega and Mega 2560
  support change interrupts, so only the following can be used for 
  RX: 10, 11, 12, 13, 14, 15, 50, 51, 52, 53, A8 (62), A9 (63),
  A10 (64), A11 (65), A12 (66), A13 (67), A14 (68), A15 (69). 
  To overcome this limitation the MG2639 library must be modified
  by replacing the following two lines of code:
#define CELL_SW_RX	3	// Cellular module UART0 RXI goes to Arduino pin 3
#define CELL_SW_TX	2	// Cellular module UART0 TXO goes to Arduino pin 2
  with
#define CELL_SW_RX	11	// Cellular module UART0 RXI goes to Arduino pin 11 for Mega
#define CELL_SW_TX	10	// Cellular module UART0 TXO goes to Arduino pin 10 for Mega
 and wire jumpers to connect pins 3 & 11 and 2 & 10 on the MG2639 shield must be added.
*/
#include <SFE_MG2639_CellShield.h> // Make sure this is modified as described above

// The OneWire library is used to read the DS18B20 temperature sensor
#include <OneWire.h>
OneWire ds(13);

// One wire variable Setup
#define DS18S20_ID 0x10
#define DS18B20_ID 0x28
 
float temp;
int sonarControl = 22; // Used to control the Maxbotix sensor
int cellControl = 7; // Available to control the cellular shield
int sample_size = 99; // Number of range samples per measurement
int sample[99];

boolean messageSent = true;

int Range; // Variable for range reading in mm

unsigned long updateInterval = 60000; // Time in ms between posts to ThingSpeak
unsigned long lastUpdateTime = 0;
unsigned long timr = 0;


IPAddress myIP; // IP address to store local IP

// To define your destination server, you can either set the
// IP address explicitly:
IPAddress serverIP(184, 106, 153, 149);
// Or define the domain of the server and use DNS to look up
// the IP for you:
const char server[] = "api.thingspeak.com";

/*
  *****************************************************************************************
  **** Visit https://www.thingspeak.com to sign up for a free account and create
  **** a channel.  The video tutorial http://community.thingspeak.com/tutorials/thingspeak-channels/ 
  **** has more information. You need to change this to your channel, and your write API key
  **** IF YOU SHARE YOUR CODE WITH OTHERS, MAKE SURE YOU REMOVE YOUR WRITE API KEY!!
  *****************************************************************************************/

String getStringOne, getStringTwo, getStringThree, getStringFour, stringRange;
char tempString[8];

int status;

void setup() {
  Serial.begin(9600); // Used for debug messages
  Serial1.begin(9600); // Used to read Maxbotix serial range data
  pinMode(2, INPUT); // Set pin as input to avoid contention
  pinMode(3, INPUT); // Set pin as input to avoid contention
  pinMode(cellControl, OUTPUT);
  digitalWrite(cellControl, HIGH);
  pinMode(sonarControl, OUTPUT);
  digitalWrite(sonarControl, LOW);
  rebootCell();
}

void loop() {
//  Serial.println("Start");  
  //Read Sonar
   timr = lastUpdateTime + updateInterval; 
    if (millis()>timr){
        lastUpdateTime = millis();    
        readSample();
        Range = getSampleMedian();
        stringRange = String(Range, DEC);
        getTemperature();
        String string1 = String("field1="+stringRange);
        dtostrf(temp,7,2,tempString);
        String string12 = String(string1+"&field2=");
        String string3 = String(string12+tempString);
        do
        {
        messageSent = updateThingSpeak(string3);
        } while (!messageSent);
    }

}

void rebootCell()
{
  digitalWrite(cellControl, LOW);
  delay(3000);
  digitalWrite(cellControl, HIGH);
  delay(1000);
  digitalWrite(cellControl, LOW);
  delay(3000);
  digitalWrite(cellControl, HIGH);
  delay(10000);
}


boolean updateThingSpeak(String tsData)
{  
  // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
  // pieces of information in a channel.  Here, we write to field 1 with the API key embedded in the GET.
  
  getStringOne = String("GET /update?api_key=L47SKJDM83TEJ9OY&"); // Use your own key here
  getStringTwo = String(" HTTP/1.1\nHost: api.thingspeak.com\n\n");
  getStringThree = getStringOne + tsData;
  getStringFour = getStringThree + getStringTwo;
  
// Call cell.begin() to turn the module on and verify
  // communication.
  int beginStatus = cell.begin();
  if (beginStatus <= 0)
  {
    Serial.println(F("Unable to communicate with shield. Rebooting cell modem."));
    //while(1)
    //  ;
    rebootCell();
    return false;
  }
  
  // gprs.open() enables GPRS. This function should be called
  // before doing any TCP stuff. It should only be necessary 
  // to call this function once.
  // gprs.open() can take up to 30 seconds to successfully
  // return. It'll return a positive value upon success.
  int openStatus = gprs.open();
  if (openStatus <= 0)
  {
    Serial.println(F("Unable to open GPRS. Rebooting cell modem."));
    //while (1)
    //  ;
    rebootCell();
    return false;    
  }
  
  // gprs.status() returns the current GPRS connection status.
  // This function will either return a 1 if a connection is
  // established, or a 0 if the module is disconnected.
  int GPRSStatus = gprs.status();
  if (GPRSStatus == 1)
    Serial.println(F("GPRS Connection Established"));
  else
    Serial.println(F("GPRS disconnected"));
  
  // gprs.localIP() gets and returns the local IP address 
  // assigned to the MG2639 during this session.
  myIP = gprs.localIP();
  Serial.print(F("My IP address is: "));
  Serial.println(myIP);
  
  // gprsHostByName(char * domain, IPAddress * ipRet) looks
  // up the DNS value of a domain name. The pertinent return
  // value is passed back by reference in the second 
  // parameter. The return value is an negative error code or
  // positive integer if successful.
  int DNSStatus = gprs.hostByName(server, &serverIP);
  if (DNSStatus <= 0)
  {
    Serial.println(F("Couldn't find the server IP. Rebooting cell modem."));
    //while (1)
    //  ;
    rebootCell();
    return false;
  }
  Serial.print(F("Server IP is: "));
  Serial.println(serverIP);
  
  // gprs.connect(IPAddress remoteIP, port) establishes a TCP
  // connection to the desired host on the desired port.
  // We looked up serverIP in the previous step. Port 80 is
  // the standard HTTP port.
  int connectStatus = gprs.connect(serverIP, 80);
  // a gprs.connect(char * domain, port) is also defined.
  // It takes care of the DNS lookup for you. For example:
  //gprs.connect(server, 80); // Connect to a domain
  if (connectStatus <= 0)
  {
    Serial.println(F("Unable to connect. Rebooting cell modem."));
   // while (1)
   //   ;
    rebootCell();
    return false;
  }
  Serial.println(F("Connected! Sending HTTP GET"));
  Serial.println();
  
  
  Serial.print(getStringFour);
  gprs.print(getStringFour);
  
  // gprs.available() returns the number of bytes available
  // sent back from the server. If it's 0 there is no data
  // available.
  
  unsigned long waitForResponse = millis()+1000;
  
  while (millis()<waitForResponse)
  {
    // gprs.read() can be called to read the oldest data
    // available in the module's FIFO.
    if (gprs.available())
      {
      Serial.write(gprs.read());
      }
  }
  // You can close here, but then won't be able to read any
  // responses from the server in the loop().

  status = gprs.close();
  if (status <= 0)
    {
      Serial.println(F("Unable to close GPRS."));
    }
  else
    {
    Serial.println("GPRS closed.");  
    }
   return true; 
}


unsigned short readSensorSerial(uint8_t length)
{
    char buffer[length];

    // flush and wait for a range reading
   Serial1.flush();

   while (!Serial1.available() || Serial1.read() != 'R');


    // read the range
    for (int i = 0; i < length; i++) {
        while (!Serial1.available());
//Serial.println(n);
        buffer[i] = Serial1.read();
     //  Serial.println(Serial1.read()); 
    }

    return atoi(buffer);
}


void readSample()
{
    // read
    digitalWrite(sonarControl, HIGH);
    for (int i = 0; i < sample_size; i++) {
        sample[i] = readSensorSerial(4);
    }

    for (int i = 0; i < sample_size; i++) {
      //Serial.print(sample[i]);
      //Serial.print(" ");
      int rem = i % 10;
      if (rem == 0){
      //Serial.println(" ");
      digitalWrite(sonarControl, LOW);
    }

    }

    // sort
    sortSample();
}

void sortSample()
{
    for (int i = 1; i < sample_size; i++) {
        int j = sample[i];
        int k;

        for (k = i - 1; (k >= 0) && (j < sample[k]); k--)
            sample[k + 1] = sample[k];

        sample[k + 1] = j;
    }
}

int getSampleMedian()
{
    return sample[sample_size / 2];
}



 boolean getTemperature(){
 byte i;
 byte present = 0;
 byte data[12];
 byte addr[8];
 //find a device
 if (!ds.search(addr)) {
 ds.reset_search();
 return false;
 }
 if (OneWire::crc8( addr, 7) != addr[7]) {
 return false;
 }
 if (addr[0] != DS18S20_ID && addr[0] != DS18B20_ID) {
 return false;
 }
 ds.reset();
 ds.select(addr);
 // Start conversion
 ds.write(0x44, 1);
 // Wait some time...
 delay(850);
 present = ds.reset();
 ds.select(addr);
 // Issue Read scratchpad command
 ds.write(0xBE);
 // Receive 9 bytes
 for ( i = 0; i < 9; i++) {
 data[i] = ds.read();
 }
 // Calculate temperature value
 temp = ( (data[1] << 8) + data[0] )*0.0625;

 ds.reset_search();
 return true;
 }
