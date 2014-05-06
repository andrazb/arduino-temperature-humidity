#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include "DHT.h"
#include <EthernetUdp.h>
#include <Time.h> 

#define DHTPIN 5     // what pin we're connected to
#define DHTTYPE DHT21   // DHT21 AM2301

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   20
#define TEMP_CORRECTION  0//5.5

DHT dht(DHTPIN, DHTTYPE);

IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov

time_t prevDisplay = 0; // when the digital clock was displayed

EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

// MAC address from Ethernet shield sticker under board
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,1,201);   // IP address, may need to change depending on network
EthernetServer server(8080);       // create a server at port 80
File logFile;                    // handle to log file on SD card
char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer

EthernetClient client;

long previousMillis = 0;        // will store last time LED was updated
// the follow variables is a long because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long interval = 900000;          // interval at which to blink (milliseconds) -> 15 minutes
int counter = 96; // 1 day = 96 times x 15 minutes

void setup()
{
    Serial.begin(9600);
    // disable Ethernet chip
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    
    dht.begin();
    
    // initialize SD card
    if (!SD.begin(4)) {
        return;    // init failed
    }
    Ethernet.begin(mac, ip);  // initialize Ethernet device
    /*Udp.begin(localPort);
    setSyncProvider(getNtpTime);
    while(timeStatus()== timeNotSet)   
     ; // wait until the time is set by the sync provider*/
    server.begin();           // start to listen for clients
    //setTime(hr,min,sec,day,month,yr);
    setTime(10,10,00,6,5,2014);
    Serial.print("Server is at ");
    Serial.println(Ethernet.localIP());
  
}

void loop()
{       
    unsigned long currentMillis = millis();
    int state = digitalRead(10);    
    if((currentMillis - previousMillis > interval) && (state == HIGH)) {
      // save the last time you blinked the LED 
      previousMillis = currentMillis;   
      float h = dht.readHumidity();
      float t = dht.readTemperature() - TEMP_CORRECTION;
      logFile = SD.open("log.txt", FILE_WRITE);
      saveTimestamp();
      logFile.print("\t");
      logFile.print(t);
      logFile.print("\t");
      logFile.println(h);
      logFile.close();
      counter--;
      if (counter == 0) {
        if( now() != prevDisplay) //update the display only if the time has changed
        {
          prevDisplay = now();
        }
        counter = 96;
      }
    } 
  
    client = server.available();  // try to get client    
    if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                //Timer1.detachInterrupt();
                char c = client.read(); // read 1 byte (character) from client
                // buffer first part of HTTP request in HTTP_req array (string)
                // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
                if (req_index < (REQ_BUF_SZ - 1)) {
                    HTTP_req[req_index] = c;          // save HTTP request character
                    req_index++;
                }        
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header

                    // open requested web page file
                    if (StrContains(HTTP_req, "GET / ")) {
                        //         || StrContains(HTTP_req, "GET /index.htm")) {    Ce bi imel htm zacetno stran
                                                
                        float h = dht.readHumidity();
                        float t = dht.readTemperature() - TEMP_CORRECTION;   
                        
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connection: close");
                        client.println();
                        // send web page
                        client.println("<!DOCTYPE html>");
                        client.println("<html>");
                        client.println("<head>");
                        client.println("<title>Temperature/Humidity</title>");
                        client.println("</head>");
                        client.println("<body>");
                        client.println("<p>");
                        digitalClockDisplay();
                        client.println("</p>");
                        client.print("<p>Temperature: ");
                        client.print(t);
                        client.println(" *C</p>");
                        client.print("<p>Humidity: ");
                        client.print(h);
                        client.println(" %</p>");
                        client.println("<p> Open <a href=\"log.txt\">log file</a>.</p>");
                        client.println("</body>");
                        client.println("</html>");                        
                    }
                    else if (StrContains(HTTP_req, "GET /log.txt")) {
                        state = digitalRead(10);
                        if (state == HIGH) {                         
                          logFile = SD.open("log.txt");        // open log file
                          
                          if (logFile) {
                            while(logFile.available()) {
                                client.write(logFile.read());
                            }                      
                          }
                          logFile.close();
                       }
                    }
                    
                    // reset buffer index and all buffer elements to 0
                    req_index = 0;
                    StrClear(HTTP_req, REQ_BUF_SZ);
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}

// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
    char found = 0;
    char index = 0;
    char len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}

void digitalClockDisplay(){
  // digital clock display of the time
  client.print(hour());
  printDigits(minute());
  printDigits(second());
  client.print(" ");
  client.print(day());
  client.print(".");
  client.print(month());
  client.print(".");
  client.print(year());
  client.println();
}

void printDigits(int digits){
  // utility function for clock display: prints preceding colon and leading 0
  client.print(":");
  if(digits < 10)
    client.print('0');
  client.print(digits);
}

void saveTimestamp() {
  logFile.print(hour());
  printDigitsFile(minute());
  printDigitsFile(second());
  logFile.print("\t");
  logFile.print(day());
  logFile.print(".");
  logFile.print(month());
  logFile.print(".");
  logFile.print(year());
}

void printDigitsFile(int digits){
  // utility function for clock display: prints preceding colon and leading 0
  logFile.print(":");
  if(digits < 10)
    logFile.print('0');
  logFile.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + adjustDstEurope();
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

int adjustDstEurope()
{
  // last sunday of march
  int beginDSTDate=  (31 - (5* year() /4 + 4) % 7);
  //Serial.println(beginDSTDate);
  int beginDSTMonth=3;
  //last sunday of october
  int endDSTDate= (31 - (5 * year() /4 + 1) % 7);
  //Serial.println(endDSTDate);
  int endDSTMonth=10;
  // DST is valid as:
  if (((month() > beginDSTMonth) && (month() < endDSTMonth))
      || ((month() == beginDSTMonth) && (day() >= beginDSTDate)) 
      || ((month() == endDSTMonth) && (day() <= endDSTDate)))
  return 7200;  // DST europe = utc +2 hour
  else return 3600; // nonDST europe = utc +1 hour
}

