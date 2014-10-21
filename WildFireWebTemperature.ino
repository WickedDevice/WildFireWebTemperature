/*
  Uses a Wicked Device WildFire and a DHT22 temperature sensor to make
  temperature data available on the web. The DHT22 output should be connected
  to pin 8. An LED will light up if the temperature in Fahrenheit drops below 
  tempThreshold.

  This sketch is adapted from Nathan Chantrell's WildFire_WebServer and
  ladyada's DHTtester. It requires the Adafruit DHT library.
  
  Adapted by Velthur Pendell for Wicked Device
  2014
  
  wildfire.wickeddevice.com for more information
 */

#include <WildFire_CC3000.h>
#include <SPI.h>
#include <WildFire.h>
#include "DHT.h"
#include <avr/wdt.h>

WildFire wf;

WildFire_CC3000 cc3000; // you can change this clock speed but DI

// You should replace this network information with your own

#define WLAN_SSID       "my_network"           // cannot be longer than 32 characters!
#define WLAN_PASS       "my_password"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2
#define DHTTYPE DHT22
#define DHTPIN 8

// Initialize the CC3000 server library
// with the port you want to use 
// (port 80 is default for HTTP):
WildFire_CC3000_Server server(80);

//Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

int tempThreshold;
char threshUnit;

unsigned long time;
#define soft_reset() \
do \
{ \
wdt_enable(WDTO_15MS); \
for(;;) \
{ \
} \
} while(0)

String input;

// Function Prototype
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));

// Function Implementation
void wdt_init(void)
{
MCUSR = 0;
wdt_disable();
return;
}

void setup() {
  
  pinMode(6, OUTPUT);

  wf.begin();

  Serial.begin(115200);
  Serial.println(F("Welcome to WildFire!\n")); 

  Serial.println(F("\nInitialising the CC3000 ..."));
  if (!cc3000.begin()) {
    Serial.println(F("Unable to initialise the CC3000! Check your wiring?"));
    for(;;);
  }
  
  Serial.println(F("\nDeleting old connection profiles"));
  if (!cc3000.deleteProfiles()) {
    Serial.println(F("Failed!"));
    while(1);
  }

  /* Attempt to connect to an access point */
  char *ssid = WLAN_SSID;             /* Max 32 chars */
  Serial.print(F("\nAttempting to connect to ")); Serial.println(ssid);
  
  /* NOTE: Secure connections are not available in 'Tiny' mode! */
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
   
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP()) {
    delay(100); // ToDo: Insert a DHCP timeout!
  }

  /* Display the IP address DNS, Gateway, etc. */  
  while (!displayConnectionDetails()) {
    delay(1000);
  }

  // Start listening for connections
  server.begin();
  
  Serial.println(F("Listening for connections..."));

  dht.begin();
  
  time = millis();
  tempThreshold = 0;
  
 }


void loop() {
  // listen for incoming clients
  input = String();
  WildFire_CC3000_ClientRef client = server.available();
  if (client) {
    Serial.println(F("new client"));
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if(c=='\n') {
          if(input.indexOf("GET /?ThresholdTemp=") > -1) {
            while(input.lastIndexOf(" ")>input.lastIndexOf("ThresholdTemp=")){
              input=input.substring(input.lastIndexOf("ThresholdTemp"),input.lastIndexOf(" "));
              Serial.println(input);
            }
            tempThreshold = input.substring(1+input.lastIndexOf("=")).toInt();
          }
          input = String();
        }
        else {
          input += c;
        }
        // if you have got to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.fastrprintln("HTTP/1.1 200 OK");
          client.fastrprintln("Content-Type: text/html");
          client.fastrprintln("Connection: close");  // the connection will be closed after completion of the response
	  client.fastrprintln("Refresh: 30");  // refresh the page automatically every 5 sec
          client.fastrprintln("");
          client.fastrprintln("<!DOCTYPE HTML>");
          client.fastrprintln("<html>");
          
          // Reading temperature or humidity takes about 250 milliseconds!
          // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
          float h = dht.readHumidity() + 0.5;
          // Read temperature as Celsius
          float t = dht.readTemperature() + 0.5;
          // Read temperature as Fahrenheit
          float f = dht.readTemperature(true) + 0.5;
          
           // Check if any reads failed and exit early (to try again).
          if (isnan(h) || isnan(t) || isnan(f)) {
            client.fastrprint("Failed to read from DHT sensor!");
            return;
          }

          // Compute heat index
          // Must send in temp in Fahrenheit!
          float hi = dht.computeHeatIndex(f, h);

          client.fastrprint("Humidity: "); 
          char hChar[1];
          client.fastrprint(itoa((int) h, hChar, 10));
          client.fastrprint(" %\t");
          client.fastrprint("Temperature: "); 
          char tChar[1];
          client.fastrprint(itoa((int) t, tChar, 10));
          client.fastrprint(" &#176C ");
          char fChar[1];
          client.fastrprint(itoa((int)f, fChar, 10));
          client.fastrprint(" &#176F ");
          client.fastrprint("Heat index: ");
          char hiChar[1];
          client.fastrprint(itoa((int) hi, hiChar, 10));
          client.fastrprintln(" &#176F");
          
          client.fastrprintln("<br />");       
          
          client.fastrprintln("<form>");
          client.fastrprintln("Threshold Temperature (&#176F): <input type='text' name='ThresholdTemp'><br>");
          client.fastrprintln("</form>");
          client.fastrprintln("Current threshold: ");
          char unitChar[1];
          client.fastrprint(itoa(tempThreshold, unitChar, 10));
          client.fastrprintln(" &#176F <br>");
          client.fastrprintln(" \n");
          client.fastrprintln("</html>");
          
          if(f<tempThreshold) {
            digitalWrite(6,HIGH);
          }
          else {
            digitalWrite(6,LOW);
          }
          break;
        }
        
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.close();
    Serial.println(F("client disconnected"));
    time = millis();
  }
  //reset if connection has been interrupted for more than 5 minutes
  else if(millis() - time > 300000) {
    soft_reset();
  }
}

/**************************************************************************/
/*!
    @brief  Tries to read the IP address and other connection details
*/
/**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}
