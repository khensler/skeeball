#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>
#include <FS.h>
#include <string.h>
#include <FastLED.h>

//7 Segment setup
#define NUM_LEDS 7
#define DATA_PIN 32
CRGB leds[NUM_LEDS];

// Setup Async Servers

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

//Setup Pins

const byte score_pin = 23;
const byte ball_pin = 22;
const byte game_pin = 21;
const byte ball_release_pin = 19;

//Global Variables

int score = 0;
int old_millis = 0;
int old_millis_ball = 0;
int old_millis_game = 0;
int debounce_score = 50;
int debounce_ball = 150;
int ball = 0;
int balls_game = 9;
int game_on = 0;
int number_loop_millis = 0;
int number_loop = 0;
char data[400];
bool send_data=false;

//Timer 

hw_timer_t * timer = NULL;

//Handle Websocket Events

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){

  if(type == WS_EVT_CONNECT){

    //Websocket Connect 

    Serial.println("Websocket client connection received");

    //Send Values to all clients

    ws.printfAll("{\"S\":\"%d\"}",score);
    ws.printfAll("{\"B\":\"%d\"}",ball);
    ws.printfAll("{\"G\":\"%d\"}",game_on);

  } else if(type == WS_EVT_DISCONNECT){
    Serial.println("Client disconnected");

  } else if(type == WS_EVT_DATA){

    Serial.println("Data received: ");

    for(int i=0; i < len; i++) {
      Serial.print(data[i]);
      Serial.print("|");
    }

    Serial.println();
  }
}

void ball_release(){
  //Trigger Ball Relese Solenoid

  Serial.println("Ball Release");
  digitalWrite(ball_release_pin,1);
  
  //Enable Timer to turn off Solenoid
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000000, false);
  timerAlarmEnable(timer);
}

void onTimer() {
  //Turn Off Ball Release Solenoid
  Serial.println("End Release");
  digitalWrite(ball_release_pin,0);

  //Reset Timer Configuration

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000000, false);
}

void do_send_data(){
  //Set Send Data from Inturrupt Handler
  //Cannot send from here due to ISR Reboot 
  //Probably a way to do this via volitale envionrment calls but this is easier

  send_data = true;
}

void notFound(AsyncWebServerRequest *request) {
  //Handle 404
  request->send(404, "text/plain", "Not found");
}

void do_start_game(){
  //Inturrupt Handler for game start button press
  //Debounce
  int diff = millis()-old_millis_game;
  if (diff> debounce_score || millis() < old_millis){
    old_millis_game = millis();
    //Check if game is already enabled
    if (game_on == 0){
      //Reset all values and start new game
      game_on = 1;
      score = 0;
      ball = 0;
      Serial.print("Game Start");
    }
    do_send_data();
    //Release the balls
    ball_release();
  }
}

void do_score(){
  //Inturrupt Handler for score pins
  //Check if game is enabled
  if(game_on ==1){
    //Debounce
    int diff = millis()-old_millis;
    Serial.printf("Diff: %d\n",diff);
    if (diff> debounce_score || millis() < old_millis){
      old_millis = millis();
      //Add 10 to score
      //All score switches connect to the same pin and each switch is worth 10 points
      //As the ball travels down the path it will trigger the appropriate number of switches
      score=score+10;
      Serial.printf("Score: %d\n", score);
      //Send data to connected Websocket clients
      do_send_data();
    }
  }
}

void do_ball(){
  //Inturrupt Handler for ball count pin
  //Debounce
  int diff = millis()-old_millis_ball;
  if (diff> debounce_ball || millis() < old_millis_ball){
    old_millis_ball = millis();
    //Count Ball
    ball= ball + 1;
    //If played balls is >= to balls/game disable game
    if (ball >= balls_game){
      game_on = 0;
    }
    Serial.printf("Ball: %d\n", ball);
    //Send Data to connected Websocket Clients
    do_send_data();
    //Display Ball Counter
    //displayNumber(0,ball);
    //FastLED.show();
  }
}

void displayNumber(int startindex, int number) {
  //startindex in digit position.  Multiplied by 7 for each digit. 
  //number is number to display (not wired in stanard 7 segment display because i'm silly
  startindex = startindex*7;
  byte numbers[] = {
    0b01110111, // 0    
    0b01100000, // 1
    0b01011101, // 2
    0b01111001, // 3
    0b01101010, // 4
    0b00111011, // 5
    0b00111110, // 6
    0b01100001, // 7
    0b01111111, // 8
    0b01101011, // 9   
  };

  for (int i = 0; i < 7; i++) {
    leds[i + startindex] = ((numbers[number] & 1 << i) == 1 << i) ? CRGB::Red : CRGB::Black;
  }
}

void displayNumerLoop(int number){
  int startindex = 0;
  byte numbers[] = {
    0b01110110,
    0b00110111,
    0b01010111,
    0b01100111,
    0b01110011,
    0b01110101,
  };
  for (int i = 0; i < 7; i++) {
    leds[i + startindex] = ((numbers[number] & 1 << i) == 1 << i) ? CRGB::Red : CRGB::Black;
  }
}
void setup() {
  Serial.begin(115200);
  //Check for SPIFFS file system to host web content
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  //Read wifi creds from SPIFFS 
  File wificreds = SPIFFS.open("/wifi.txt");
  String file_ssid = wificreds.readStringUntil('\n');
  String file_ssid_key = wificreds.readStringUntil('\n');
  file_ssid.trim();
  file_ssid_key.trim();
  //Convert to char * for wifi connect
  char * ssid = new char [file_ssid.length()+1];
  file_ssid.toCharArray(ssid,file_ssid.length()+1);
  char * password = new char [file_ssid_key.length()+1];
  file_ssid_key.toCharArray(password,file_ssid_key.length()+1);
  //close file
  wificreds.close();


  //Set WIFI to station momde and connect
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }

  //Print IP.  Also avalbile via multicast DNS from OTA
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  //Set pin modes

  //pinMode(score_pin, INPUT_PULLDOWN);
  pinMode(score_pin, INPUT_PULLDOWN);
  pinMode(ball_pin, INPUT_PULLDOWN);
  pinMode(game_pin, INPUT_PULLUP);
  pinMode(ball_release_pin, OUTPUT);

  //Make sure Solenoid doesn't release 

  digitalWrite(ball_release_pin,0);
  
  //Setup LEDS
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
  displayNumber(0,0);
  FastLED.show();
  //Add webserver and sebsocket Handlers

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  //Define root request on webserver

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    //Send index.html from SPIFFS
    request->send(SPIFFS, "/index.html");
  });
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    //Restart Micro Controller
    request->send(200, "text/html", "<html><head> <meta http-equiv='refresh' content='10;url=/'></head><body><h1>RESET!!</h1></body></html>");
    ESP.restart();
  });
  server.on("/sevenSeg.js", HTTP_GET, [](AsyncWebServerRequest *request){
    //Send sevenSeg.js from SPIFFS
    request->send(SPIFFS, "/sevenSeg.js");
  });
  server.onNotFound(notFound);

  server.begin();

  //ArduinoOTA Code from example
    ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  //Delya to prevent false inital score and ball count

  delay(1000);
  
  //Setup ball release Solenoid timer 

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000000, false);
  
  //Reset Values 

  score=0;
  ball=0;
  game_on=0;

  //attachInterrupts to handle ball and game start actions

  attachInterrupt(digitalPinToInterrupt(score_pin), do_score, FALLING);
  attachInterrupt(digitalPinToInterrupt(ball_pin), do_ball, HIGH);
  attachInterrupt(digitalPinToInterrupt(game_pin), do_start_game, HIGH);
}

void loop() {
  //All game logic is handled in Inturrupts 
  //OTA Handel
  ArduinoOTA.handle();
  //Clean up improperly disconnect WebSockets
  ws.cleanupClients();
  //Send data to WebSockets
  if (send_data == true){
    displayNumber(0,ball);
    FastLED.show();
    if(score == 0){
      ws.printfAll("{\"S\":\"000\"}");
    }else{
    ws.printfAll("{\"S\":\"%d\"}",score);  
    }
    ws.printfAll("{\"B\":\"%d\"}",ball);
    ws.printfAll("{\"G\":\"%d\"}",game_on);
    send_data = false;
  }
  //Serial.println(digitalRead(score_pin));
  if (game_on==0){
    if (number_loop_millis < millis()-250){
      number_loop_millis = millis();
      displayNumerLoop(number_loop);
      FastLED.show();
      number_loop++;
      if (number_loop == 6){
        number_loop=0;
      }
    }
  }
}
