#include <ESP8266WiFi.h>
extern "C"
{
#include <espnow.h>
}
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <deque>
#include <vector>

AsyncWebServer server(80);

std::deque<String> messageLog;
const size_t maxMessages = 10;

uint8_t peerMac[] = {0xA0, 0xDD, 0x6C, 0x03, 0xC8, 0xCC}; // ESP32 MAC

void onDataRecv(uint8_t *mac, uint8_t *data, uint8_t len)
{
  String newMsg = "";
  for (int i = 0; i < len; i++)
    newMsg += (char)data[i];

  messageLog.push_back(newMsg);
  if (messageLog.size() > maxMessages)
    messageLog.pop_front();
}

void setup()
{
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize
  Serial.println("\n[DEBUG] ESP8266 Starting up...");

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP8266_Chat", NULL, 1);
  Serial.println("[DEBUG] WiFi AP Mode initialized");
  Serial.println("[DEBUG] AP SSID: ESP8266_Chat");
  Serial.printf("[DEBUG] AP MAC Address: %s\n", WiFi.macAddress().c_str());

  Serial.println("[DEBUG] Initializing ESP-NOW...");
  if (esp_now_init() != 0)
  {
    Serial.println("[ERROR] ESP-NOW initialization failed!");
    return;
  }
  Serial.println("[DEBUG] ESP-NOW initialized successfully");

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  Serial.println("[DEBUG] ESP-NOW role set to COMBO");

  esp_now_register_recv_cb(onDataRecv);
  Serial.println("[DEBUG] Receive callback registered");

  int8_t result = esp_now_add_peer(peerMac, ESP_NOW_ROLE_COMBO, 0, NULL, 0);
  Serial.printf("[DEBUG] Adding peer %02X:%02X:%02X:%02X:%02X:%02X - %s\n",
                peerMac[0], peerMac[1], peerMac[2], peerMac[3], peerMac[4], peerMac[5],
                result == 0 ? "Success" : "Failed");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String page = "<html><head><title>ESP8266 Chat</title></head><body>";
    page += "<h2>ESP8266 Chat</h2>";
    page += "<p><b>Board MAC:</b> " + WiFi.macAddress() + "</p>";
    page += "<form action='/send' method='POST'>";
    page += "<input type='text' name='msg'><input type='submit' value='Send'>";
    page += "<p><b>Last Received:</b><br><span id='lastMsg'></span></p>";
    page += "<script>";
    page += "  setInterval(() => {";
    page += "    fetch('/latest')";
    page += "      .then(res => res.text())";
    page += "      .then(text => {";
    page += "        document.getElementById('lastMsg').innerHTML = text;";
    page += "      });";
    page += "  }, 2000);";
    page += "</script>";
    page += "</body></html>";
    request->send(200, "text/html", page); });

  server.on("/send", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("msg", true)) {
      String msg = request->getParam("msg", true)->value();
      Serial.print("[DEBUG] Sending message: ");
      Serial.println(msg);
      uint8_t result = esp_now_send(peerMac, (uint8_t *)msg.c_str(), msg.length());
      if (result != 0) {
        Serial.println("[ERROR] Failed to send message");
      } else {
        Serial.println("[DEBUG] Message sent successfully");
      }
    }
    request->redirect("/"); });

  server.on("/latest", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String response = "";
    for (const String &msg : messageLog)
      response += msg + "<br>";
    request->send(200, "text/html", response); });

  server.begin();
  Serial.println("[DEBUG] Web server started");
}

void loop() {}