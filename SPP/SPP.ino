/*********************************************************************************
PUBNUB PUBLISH DATA USING REST API
*********************************************************************************/
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <Ticker.h>

#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
// Initialize the OLED display using Wire library
SSD1306  display(0x3c, D3, D4);

#include "EmonLib.h"                   // Include Emon Library
EnergyMonitor emon1;                   // Create an instance

unsigned long g_startMillis;
unsigned long g_switchTimeMillis;
boolean       g_heaterInHighPhase;

const char* ssid       = "AndroidAP";
const char* password   = "1234";
const char* host       = "pubsub.pubnub.com";
const char* pubKey     = "<your-pubnub-publish-key>";
const char* subKey     = "<your-pubnub-subscribe-key>";
const char* channel    = "channel_pwr";
String      timeToken    = "0";

const long sub_interval = 600; 
unsigned long previousMillis = 0;  

float callibrate = 4.3;  // use 100 for 20A Module and 66 for 30A Module
float voltage = 238.1;
double Irms = 0.00; 
double Power =  0.00 ;
String SPP_status = "(en)";
boolean SPP_enabled = true;
DynamicJsonBuffer jsonBufPWR;

// Commands sent through Web Socket
const char LEDON[] = "{\"SPP_device_state\":\"on\"}}";
const char LEDOFF[] = "{\"SPP_device_state\":\"off\"}}";
const char PWR_DATA[] = "{\"pwr\":\" \"}"; 

// this constant won't change:
const int  buttonPin = D1;    // the pin that the pushbutton is attached to
const int ledPin = D0;       // the pin that the LED is attached to
// Current LED status
bool LEDStatus;

// Variables will change:
String device_status = "OFF";   // counter for the number of button presses
int buttonState = 0;         // current state of the button
int lastButtonState = 0;     // previous state of the button
int Mode = 0;

String url;

WiFiClient client;
Ticker tickRCV;
String inputString = ""; 

typedef enum RET{
  FAILURE = 0,
  SUCCESS
}RET_VALUE;

void setup(void){
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();  
  
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // initialize the button pin as a input:
  pinMode(buttonPin, INPUT);
  // initialize the LED as an output:
  pinMode(ledPin, OUTPUT);    
  //tickRCV.attach(5, recv);
  emon1.current(A0, callibrate);
   
  ////////initially Turn off the device
   device_status = "OFF";
   digitalWrite(ledPin, LOW);
   
   StaticJsonBuffer<200> jsonBufferInit;   
   JsonObject& root = jsonBufferInit.parseObject(LEDOFF); 
   String output;
   root.printTo(output);
   Serial.println(output);           
   send_PN(client, output, channel);     
}

/*********************************************************************************
Function Name     : loop
Description       : Publish the data to the hello_world channel using the 
                    GET Request 
Parameters        : void
Return            : void
*********************************************************************************/

void loop()
{

  unsigned long currentMillis = millis();  
  if (currentMillis - previousMillis >= sub_interval) {
      recv();
      previousMillis = currentMillis;
  }
  control_device();   
  calc_power();
  display.clear();
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      Serial.println("GOT:" + inputString.substring(0,inputString.length()-1));
      //send_PN(client, inputString.substring(0,inputString.length()-1), channel); 
      //recv();
      inputString = "";
      
    }
  }
}

void send_PN(WiFiClient c, String msg, String ch){
          if (!c.connect(host, 80)) {
            Serial.println("Pubnub Connection Failed");
            return;
          }//else{
            //Serial.println("Pubnub Connection Success");       
          //}
          String url = "/publish/";
          url += pubKey;
          url += "/";
          url += subKey;
          url += "/0/";
          url += ch;
          url += "/0/";
          url += msg;
          url +=  "?uuid=SPP";
          url +=  "&auth=SPP";
          
          c.print(String("GET ") + url + " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" + 
                     "Connection: close\r\n\r\n");
          delay(50); 
          
          //while(c.available()){
          //   String line = c.readStringUntil('\r');
          //   Serial.print(line);
          // }
          //Serial.print("Send:" + msg);  
}

void recv(){
  //Serial.println("test");
    if (!client.connect(host, 80))
  {
    delay(20);
    Serial.println("connection failed");
    client.stop();
    return;
  }
  //DATA FORMAT : http://pubsub.pubnub.com /publish/pub-key/sub-key/signature/channel/callback/message
  //SUB FORMAT :  http://pubsub.pubnub.com /subscribe/sub-key/channel/callback/timetoken
  //yield();
  url = "/subscribe/";
  url += subKey;
  url += "/";
  url += "channel_SPP";
  url += "/0/";
  url += timeToken;
  url +=  "?uuid=SPP";
  url +=  "&auth=SPP";

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");     
                        


  while (client.available())
  {
    String line = client.readStringUntil('\r');
    //Serial.println(line);
    if (line.endsWith("]"))
    {
      Serial.print("RECV>>>");
      Serial.println(line);
      json_handler(string_parser(line));
      client.flush();
    }
  }
  client.flush();
  //delay(100); 
  //Serial.println("listening");   
}

void calc_power(){
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 20, "Amps:" + String(Irms));
  display.drawString(0, 40, "Power(W):" + String(Power));
  display.display();   
  Irms = emon1.calcIrms(1480);  // Calculate Irms only
  Power = Irms * voltage;
  //Serial.println(Power);
       
  DynamicJsonBuffer jsonBufPWR; 
  JsonObject& root = jsonBufPWR.parseObject(PWR_DATA);       
  
  root[String("pwr")] = Power; 
  String output;
  root.printTo(output);
  //Serial.println(output);  
  //send_PN(client, d, channel);  
  send_PN(client, output, "channel_chart");
}

static void control_device(){

  // read the pushbutton input pin:
  buttonState = digitalRead(buttonPin);
    //display.clear();
    display.setFont(ArialMT_Plain_16);
    //display.drawString(0, 0, "                ");
    display.drawString(0, 0, "Status:" + device_status + SPP_status);
    display.display(); 
  if (SPP_enabled == true){   
  // compare the buttonState to its previous state
  if (buttonState != lastButtonState) {
     StaticJsonBuffer<200> jsonBuffer;
    // if the state has changed, increment the counter
    if (buttonState == HIGH) {
      // if the current state is HIGH then the button
      // wend from off to on:
      if (device_status == "OFF"){
         Serial.println("D HIGH");
         digitalWrite(ledPin, HIGH);
         device_status = "ON";
                  
         JsonObject& root = jsonBuffer.parseObject(LEDON);       
         String output;        
         root.printTo(output);
         Serial.println(output);           
         send_PN(client, output, channel);
      }else{
         device_status = "OFF";
         digitalWrite(ledPin, LOW);
         Serial.println("D LOW");
         JsonObject& root = jsonBuffer.parseObject(LEDOFF); 
         String output;
         root.printTo(output);
         Serial.println(output);           
         send_PN(client, output, channel);     

      }
    } else {
      // if the current state is LOW then the button
      // wend from on to off:
      //Serial.println("released");
    }
    // Delay a little bit to avoid bouncing
    delay(50);
  }
  // save the current state as the last state,
  //for next time through the loop
  lastButtonState = buttonState;
  }
}


uint8_t json_handler(String p_json) {
  //String key_parsed = "";
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json_parsed = jsonBuffer.parseObject(p_json);
  if (!json_parsed.success())
  {
    Serial.println("parseObject() failed");
    timeToken = "0";
    return FAILURE;
  }
  Serial.print("json_parsed:");
  String outputT;
  json_parsed.printTo(outputT);  
  Serial.println(outputT);
  //Serial.print("size:");
  //Serial.println((json_parsed));  
  if (json_parsed.containsKey("SPP_device_state")) {
    String key_parsed = json_parsed["SPP_device_state"];
    Serial.println(key_parsed);
    if (key_parsed == "on"){
         //Serial.println("GOT LED ON");
         digitalWrite(ledPin, HIGH);
         device_status = "ON";
         
    }else if (key_parsed == "off"){
         //Serial.println("GOT LED OFF");
         digitalWrite(ledPin, LOW);
         device_status = "OFF"   ;    
    }    
  }
  
  if (json_parsed.containsKey("SPP_state")) {
    String key_parsed = json_parsed["SPP_state"];
    Serial.println(key_parsed);
    if (key_parsed == "enable"){
         //Serial.println("GOT LED ON");
         //digitalWrite(ledPin, HIGH);
         SPP_status = " (en)";
         SPP_enabled = true;
         
    }else if (key_parsed == "disable"){
         //Serial.println("GOT LED OFF");
         digitalWrite(ledPin, LOW);
         device_status = "OFF";
         SPP_enabled = false; 
         SPP_status = " (dis)";    
    }    
  }

    //display.setFont(ArialMT_Plain_16);
    //display.drawString(0, 0, "                ");
    //display.drawString(0, 0, "Status:" + device_status + SPP_status);
    //display.display();   
  return SUCCESS;
}

String string_parser(String data) {
  char bufferr[200];
  memset(bufferr, '\0', 200);

  int flower_count = 0;
  String json;

  typedef enum {
    STATE_INIT = 0,
    STATE_START_JSON,
    STATE_END_JSON,
    STATE_SKIP_JSON,
    STATE_INIT_TIME_TOKEN,
    STATE_START_TIME_TOKEN,
    STATE_END_TIME_TOKEN
  } STATE;
  STATE state = STATE_INIT;

  data.toCharArray(bufferr, 200);

  for (int i = 1; i <= strlen(bufferr); i++) {
    if (bufferr[i] == '[' && bufferr[i + 2] == '{') {
      state = STATE_START_JSON;
      json = "\0";
      continue;
    }
    else if (bufferr[i] == '[' && bufferr[i + 4] == '"') {
      state = STATE_INIT_TIME_TOKEN;
      json = "{}";
      timeToken = "\0";
      continue;
    }
    switch (state) {
      case STATE_START_JSON:
        if (bufferr[i] == '[') {
          break;
        }
        else if (bufferr[i] == '{') {
          flower_count++;
          json += bufferr[i];
        }
        else if (bufferr[i] == '}') {
          flower_count--;
          json += bufferr[i];
          if (flower_count <= 0) {
            state = STATE_END_JSON;
          }
        }
        else {
          json += bufferr[i];
          if (bufferr[i] == '}') {
            state = STATE_END_JSON;
          }
        }
        break;
      case STATE_END_JSON:
        if (bufferr[i] == ']' && bufferr[i + 2] == '"') {
          state = STATE_INIT_TIME_TOKEN;
          timeToken = "\0";
        }
        break;
      case STATE_INIT_TIME_TOKEN:
        if (bufferr[i] == '"') {
          state = STATE_START_TIME_TOKEN;
          timeToken = "\0";
        }
        break;
      case STATE_START_TIME_TOKEN:
        if (bufferr[i] == '"' && bufferr[i + 1] == ']') {
          state = STATE_END_TIME_TOKEN;
          break;
        }
        timeToken += bufferr[i];
        break;
      case STATE_END_TIME_TOKEN:
        break;
      default:
        break;
    }
  }
  return json;
}

//End of the Code
