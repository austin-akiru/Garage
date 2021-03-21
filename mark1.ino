

// Software Serial to communicate with ESP8266
#include <SoftwareSerial.h>
#define RX 10
#define TX 11
SoftwareSerial esp(RX, TX);


// function prootype with optional parameter
void sendtoesp(String command, int delay, int silent=0);


int RelayPins[] = {4, 7, 8, 12};



void setup() {
  initHardwareSerial();
  initSoftwareSerial();
  initRelay();
  WiFiInit();
}

// onboard serial for debugging
void initHardwareSerial() {
  Serial.begin(9600);
  while (!Serial) ;
  Serial.println("Uno Serial [OK]");
}

// softwre serial for ESP interface
void initSoftwareSerial() {
  Serial.print("ESP Serial");
  esp.begin(9600);
  long int time=millis();
  while ((time+10000)>millis()) {
    if(Serial) {
      Serial.println(" [OK]");
      return;
    }
  }
  error();
}


// Set GPIO pins to output for comms to relay
void initRelay() {
    for (int p = 0; p < 4; p++) {
    pinMode(RelayPins[p], OUTPUT);
  }
}


// request ESP join WIFI and run as server
void WiFiInit() {
  Serial.println("WiFi INIT");

  // Init from scratch
  //sendtoesp("AT+CWMODE=1",1000); // Join existing AP
  //sendtoesp("AT+CWJAP=\""+AP+"\",\""+PW+"\"",10000);
  //getIP();

  // Assume it already has the correct settings
  sendtoesp("AT+CWMODE=1",1000); // Join existing AP
  sendtoesp("AT+CIPMUX=1",1000); // enable multiple connections
  sendtoesp("AT+CIPSERVER=1,80",5000); // open server on port 80 
  getIP();
}


void sendtoesp(String command,int delay,int silent) {
  esp.println(command);
  String response = "";
  Serial.print(command);
  long int time=millis();
  while ((time+delay)>millis()) {
    if(esp.available()) {
      char c = esp.read();
      response+=c;
      if(response.indexOf("OK")>=0) {
          Serial.println(" [OK]");
        return;
      }
    }
  }
  if(silent==1) {
      Serial.println("[SHRUG]");
  } else {
    error();
  }
}



void error() {
  Serial.println(" [ERROR]");
  Serial.println("[HALT]");
  while(1) ;
}


void loop() {
  String readstring;
  while(esp.available()) {
    delay(3);
    char c =esp.read();
    readstring+=c;
  }
  if(readstring.length()>0) {
    if(readstring.indexOf("/RELAY")>=0) {
      int i=readstring.indexOf("/RELAY");
      String q=readstring.substring(i+6,i+7);
      Serial.print("Received RELAY command: ");
      triggerRelay(q);
      String response="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nOK";
      String command="AT+CIPSEND=0,"+String(response.length());
      sendtoesp(command,1000,1);
      sendtoesp("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nOK",100);
      //sendtoesp("OK",1000);
      sendtoesp("AT+CIPCLOSE=0",1000);  
      readstring="";
    } else if (readstring.indexOf("IPD")>=0) {
      sendtoesp("AT+CIPSEND=0,6",1000);
      sendtoesp("GOAWAY",1000);
      sendtoesp("AT+CIPCLOSE=0",1000);
    }
  }
}




void getIP() {
  String response = "";
  esp.println("AT+CIFSR");
  long int time=millis();
  while ((time+1000)>millis()) {
    while(esp.available()) {
      char c = esp.read();
      response+=c;
    }
  }
  //debug(response);
  Serial.print("IP ADDR: ");
  int a = response.indexOf("STAIP") + 7;
  String ip3= response.substring(a,response.indexOf("\"",a));
  Serial.println(ip3);
}

void triggerRelay(String s) {
  int i = s.toInt();
  Serial.println(i);
  if(i==0) {
    Serial.print("invalid command: ");
    Serial.println(s);
  }
  if(i>0 && i<=4) {
    digitalWrite(RelayPins[i-1],HIGH);
    delay(2000);
    digitalWrite(RelayPins[i-1],LOW);    
  } else {
    Serial.print("invalid relay: ");
    Serial.println(i);
  }
}
