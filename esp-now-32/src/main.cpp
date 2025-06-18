#include <WiFi.h>
#include <esp_now.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <deque>
#include <vector>

AsyncWebServer server(80);
std::deque<String> messageLog;
const size_t maxMessages = 10;

uint8_t peerMac[] = {0xEC, 0x64, 0xC9, 0xCA, 0x6D, 0xA1}; // ESP8266 MAC

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len)
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

  // Initialize WiFi in AP mode
  WiFi.mode(WIFI_AP_STA); // Changed to AP_STA mode for better compatibility
  WiFi.softAP("ESP32_Chat", NULL, 1);
  delay(1000); // Short delay to ensure WiFi is initialized

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("AP MAC address: ");
  Serial.println(WiFi.softAPmacAddress());

  // Initialize ESP-NOW
  Serial.println("Initializing ESP-NOW...");
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW init failed");
    return;
  }
  Serial.println("ESP-NOW initialized successfully");

  esp_now_register_recv_cb(onDataRecv);

  // Add peer
  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(esp_now_peer_info_t));
  memcpy(peer.peer_addr, peerMac, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    Serial.println("Peer MAC address:");
    for (int i = 0; i < 6; i++)
    {
      Serial.print(peerMac[i], HEX);
      if (i < 5)
        Serial.print(":");
    }
    Serial.println();
    return;
  }
  Serial.println("Peer added successfully");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String page = "<html><head><title>ESP32 Chat</title></head><body>";
    page += "<h2>ESP32 Chat</h2>";
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
    page += "  }, 1000);";
    page += "</script>";
    page += "</body></html>";
    request->send(200, "text/html", page); });

  server.on("/send", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("msg", true)) {
      String msg = request->getParam("msg", true)->value();
      Serial.println(msg);
      Serial.print("Sending message: ");
      Serial.println(msg);
      esp_err_t result = esp_now_send(peerMac, (uint8_t *)msg.c_str(), msg.length());
      if (result != ESP_OK) {
        Serial.println("Error sending the data");
      } else {
        Serial.println("Message sent successfully");
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
}

void loop() {}