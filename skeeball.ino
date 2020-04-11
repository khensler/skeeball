//
// A simple server implementation showing how to:
//  * serve static messages
//  * read GET and POST parameters
//  * handle missing pages / 404s
//

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char* ssid = "SunShine";
const char* password = "IcarusMelts";
const char* PARAM_MESSAGE = "message";

const byte score_pin = 23;
const byte ball_pin = 33;
const byte game_pin =32;

int score = 0;
int old_millis = 0;
int old_millis_ball = 0;
int debounce_score = 15;
int debounce_ball = 15;
int ball = 0;
int balls_game = 9;
int game_on = 0;
char data[400];
bool send_data=false;

// Port defaults to 3232
// ArduinoOTA.setPort(3232);
// Hostname defaults to esp3232-[MAC]
// ArduinoOTA.setHostname("myesp32");

// No authentication by default
// ArduinoOTA.setPassword("admin");

// Password can be set with it's md5 value as well
// MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
// ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

String processor(const String& var)
{
  if(var == "SCORE_TEMPLATE")
    return String(score);
  if(var == "BALL_TEMPLATE")
    return String(ball);
  if(var == "GAME_TEMPLATE")
    return String(game_on);
  return String();
}


void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){

  if(type == WS_EVT_CONNECT){

    Serial.println("Websocket client connection received");
   // ws.printfAll("Connect");
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
  
}

void do_send_data(){
  send_data = true;
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void do_start_game(){
  if (game_on == 0){
    game_on = 1;
    score = 0;
    ball = 0;
    Serial.print("Game Start");
    //ws.pingAll();
    //ws.printfAll("{\"G\":\"%d\"}",game_on);
    do_send_data();
  }
  ball_release();
}

void do_score(){
  int diff = millis()-old_millis;
  if (diff> debounce_score || millis() < old_millis){
    old_millis = millis();
    Serial.printf("TIME: %d\n", diff);
    score=score+10;
    Serial.printf("Score: %d\n", score);
    do_send_data();
  }
}

void do_ball(){
  int diff = millis()-old_millis_ball;
  //Serial.printf("TIME: %d\n", diff);
  if (diff> debounce_ball || millis() < old_millis_ball){
    old_millis_ball = millis();
    Serial.printf("TIME: %d\n", diff);
    ball= ball + 1;
    if (ball >= balls_game){
      //score = 0;
      //ball = 0;
      game_on = 0;
      //ws.printfAll("{\"G\":\"%d\"}",game_on);
    }
    Serial.printf("Ball: %d\n", ball);
    //ws.printfAll("{\"B\":\"%d\"}",ball);
    do_send_data();
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  pinMode(score_pin, INPUT_PULLUP);
  attachInterrupt(score_pin, do_score, RISING);
  pinMode(ball_pin, INPUT_PULLUP);
  attachInterrupt(ball_pin, do_ball, RISING);
  pinMode(game_pin, INPUT_PULLUP);
  attachInterrupt(game_pin, do_start_game, RISING);
  
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  //server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send(200, "text/plain", "Hello, world");
  //  sprintf(data, "<html><head> <meta http-equiv='refresh' content='5'></head><body><h1>Score: %d<br>Ball: %d<br>Game On: %d</h1><br><a href='/reset'>Reset</a></body></html>", score, ball, game_on);
  //  request->send(200, "text/html", data);
  //});
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send(200, "text/plain", "Hello, world");
    //sprintf(data, "<html><head></head><body><h1>Score: %d<br>Ball: %d<br>Game On: %d</h1><br><a href='/reset'>Reset</a></body></html>", score, ball, game_on);
    //request->send(200, "text/html", data);
    //request->send(SPIFFS, "/index.html", String(), false, processor);
    request->send(SPIFFS, "/index.html");
  });
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<html><head> <meta http-equiv='refresh' content='10;url=/'></head><body><h1>RESET!!</h1></body></html>");
    ESP.restart();
  });
  server.on("/sevenSeg.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/sevenSeg.js");
  });
  server.onNotFound(notFound);

  server.begin();

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
  delay(1000);
  score=0;
  ball=0;
  game_on=0;
}

void loop() {
  ArduinoOTA.handle();
  ws.cleanupClients();
  if (send_data == true){
    ws.printfAll("{\"S\":\"%d\"}",score);
    ws.printfAll("{\"B\":\"%d\"}",ball);
    ws.printfAll("{\"G\":\"%d\"}",game_on);
    send_data = false;

  }
}
