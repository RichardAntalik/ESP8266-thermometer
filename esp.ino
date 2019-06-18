#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFiMulti.h>
#include <FS.h>

ESP8266WiFiMulti WiFiMulti;

//IP settings
IPAddress local_ip(192,168,66,6);
IPAddress gateway(192,168,66,6);
IPAddress subnet(255,255,255,0);

//ports
WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266WebServer server(80);
WiFiClient client;
File fsUploadFile;

String getContentType(String filename);
bool handleFileRead(String path);
String header;

int anal;
char buf[5];

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  for (uint8_t t = 4; t>0; t--){
    Serial.printf("BOOTING...\t%d \n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP("192-168-66-6");
  Serial.println("Soft AP running");
  Serial.flush();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Web Sockets running");
  Serial.flush();

  MDNS.begin("esp8266");
  Serial.println("DNS server running");
  Serial.flush();

  SPIFFS.begin();
  Serial.println("FS running");
  Serial.flush();


  server.on("/upload", HTTP_GET, [](){if (!handleFileRead("/upload.html"))server.send(404);});
  server.on("/upload", HTTP_POST, [](){server.send(200);}, handleFileUpload);
  server.onNotFound([](){if(!handleFileRead(server.uri())) server.send(404, "text/plain", "404: Not Found");});
  server.begin();
  Serial.println("HTTP server running");
  Serial.flush();

  WiFiMulti.addAP("ANTALNET");
  while(WiFiMulti.run() != WL_CONNECTED){
    Serial.println("connecting to SSID \"ANTALNET\"");
    Serial.flush();
    delay(100);
  }

}

void loop()
{
  webSocket.loop();
  server.handleClient();
}

String getContentType (String filename)
{
  if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path)
{
  Serial.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";

  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz)) path +=".gz";

    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    Serial.println("Sent file: " + path);
    return true;
  }
  Serial.println("file not found: " + path);
  return false;
}

void handleFileUpload()
{
  HTTPUpload &upload = server.upload();

  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.println("handle upload: " + filename);
    fsUploadFile = SPIFFS.open(filename, "w");
  }
  else if (upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END){
    if(fsUploadFile){
      fsUploadFile.close();
      Serial.println("Upload done");
  
      server.sendHeader("Location", "/");
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch(type){
    case WStype_DISCONNECTED:
      Serial.println("WS: Disconnected");
      break;

    case WStype_TEXT:
      Serial.println("WS: got msg");

      if(payload[0] == 'F'){
        FSInfo fs_info;
        SPIFFS.info(fs_info);
        Dir dir = SPIFFS.openDir("/");

        while(dir.next()){
          String fileInfo = "F|";
          fileInfo += dir.fileName() + "|";
          File f = dir.openFile("r");
          fileInfo += f.size();

          webSocket.sendTXT(num, fileInfo);
        }
        webSocket.sendTXT(num, "F");
      } else {
        anal = analogRead(17);
        itoa(anal, buf, 10);
        webSocket.sendTXT(num, buf);
      }
      break;
  }
}
