#define MCU 1 // 0: M5StickC, 1: ESP32DevKitC, 2: ESP32DevKitC with Serial(0)

#include <Arduino.h>
#if MCU == 0
#include <M5StickC.h>
#endif
#include <WiFi.h>
#include <ssl_client.h>
#include <WiFiClientSecure.h>
#include "WiFiManager.h"  

// WiFi instanse
WiFiManager wifiManager;
WiFiClient client;
IPAddress ipadr;

// const char* ssid = "Umi";//gikenbase-guest,SPWH_H32_0724F74,Umi,303ZTa-DFEEB9,TB-Link_CB98
// const char* passwd = "20612061";//g20190911,r92dt9i08byfn10,20612061,4437957a,98049950
const char* host = "api.thinger.io";
const char* thinger_user = "FutuRocket";
const char* thinger_bucket = "manacam08";
const char* token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiJtYW5hY2FtMDgiLCJ1c3IiOiJGdXR1Um9ja2V0In0.Vkljvk0Hl0EraVdVLkhqh481pjW1ldrmSOwdUv9PcpQ";
const char* device_name = "manacam12@Home";

#if MCU != 2
HardwareSerial serial_ext(2);
#endif

static const int RX_BUF_SIZE = 20000;
static const uint8_t packet_begin[3] = { 0xFF, 0xD8, 0xEA };

// ------------------------------

// void cmdSendPersonCount() {
//     M5.Lcd.setCursor(0, 6, 2);
//     M5.Lcd.println("recv data");
    
//     // 人数カウントを取得
//     uint8_t rx_buffer[4];
//     int rx_size = serial_ext.readBytes(rx_buffer, 4);
//     // serial_ext.readBytes(rx_buffer, 4);
//     int pcount = atoi((const char *)rx_buffer);

    
//     M5.Lcd.setCursor(0, 12, 2);
//     M5.Lcd.println("send to cloud...");
    
//     // cloudに送信
//     postPersonCount(pcount);
    
//     M5.Lcd.setCursor(0, 18, 2);
//     M5.Lcd.println("finished");
// }

// ------------------------------

void setup() {
#if MCU == 0
    M5.begin();
    M5.Lcd.init();
    M5.Lcd.setRotation(3);
    M5.Lcd.setCursor(0, 0, 2);
    M5.Lcd.println("init..");
#elif MCU == 1
    Serial.begin(115200);
    Serial.flush();
    delay(50);
    Serial.print("ESP32DevKitC initializing...");
#else
    Serial.flush();
#endif

//    setup_wifi();
  // WiFiManagerによる接続（接続できない場合はManaCamConfigAPというAPが立ち上がる）
  wifiManager.autoConnect("ManaCamConfigAP");

   //if you get here you have connected to the WiFi
  ipadr = WiFi.localIP();
  Serial.println("connected!");
  Serial.println(WiFi.SSID());
  Serial.println(ipadr);

#if MCU != 2
    // UART RX=32(white), TX=33(yellow) Maix Bit 35 to 16, 34 to 17
    serial_ext.begin(115200, SERIAL_8N1, 16, 17);
#endif

#if MCU == 0
    M5.Lcd.println("OK");
#elif MCU == 1
    Serial.println("Initialize OK");
#else
    // Nothing
#endif

}

void loop() {
#if MCU == 0
    M5.update();
#endif

#if MCU != 2
    if (!serial_ext.available()) {
        return;
    }
#else
    if (!Serial.available()) {
        return;
    }
#endif

    uint8_t rx_buffer[10];
#if MCU != 2
    int rx_size = serial_ext.readBytes(rx_buffer, 10);
    
    for (int i=0; i<10; i++) {
      Serial.print(rx_buffer[i]); 
      Serial.print(", ");
    }
    Serial.println("");
#else
    int rx_size = Serial.readBytes(rx_buffer, 10);
#endif

    if (rx_size != 10) return;
    if (!((rx_buffer[0] == packet_begin[0]) && (rx_buffer[1] == packet_begin[1]) && (rx_buffer[2] == packet_begin[2]))) return;

    // cmdSendPersonCount();

    // 人数カウントを取得
    int pcount = (uint32_t)(rx_buffer[4] << 16) | (rx_buffer[5] << 8) | rx_buffer[6];
    
#if MCU == 0
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 6, 2);
    M5.Lcd.println("recv data");
    M5.Lcd.setCursor(0, 12, 2);
    M5.Lcd.println("send to cloud...");
#elif MCU == 1
    Serial.print("recv pcount: ");
    Serial.println(pcount);
#else
    // nothing
#endif
    
    // cloudに送信
    postPersonCount(pcount);

#if MCU == 0
    M5.Lcd.setCursor(0, 18, 2);
    M5.Lcd.println("finished");
#elif MCU == 1
    Serial.println("uploaded to cloud");
#else
    // nothing
#endif

    vTaskDelay(10 / portTICK_RATE_MS);

#if MCU == 0
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0, 2);
    M5.Lcd.println("waiting...");
#endif
}

//void setup_wifi() {
    // We start by connecting to a WiFi network
//#if MCU != 2
//    Serial.println();
//    Serial.print("Connecting to ");
//    Serial.println(ssid);
//#endif
//    WiFi.begin(ssid, passwd);
//    while (WiFi.status() != WL_CONNECTED) {
//        delay(500);
//#if MCU != 2
//        Serial.print(".");
//#endif
//    }
//#if MCU != 2
//    Serial.println("");
//    Serial.print("WiFi connected. Current IP address is:");
//    Serial.println(WiFi.localIP());
//#endif
//}

bool postPersonCount(uint8_t person_count) {
    WiFiClientSecure client;

    int try_count = 0;
//    Serial.print("connect to server");
    while (!client.connect(host, 443, 30000)) { //timeout 20 sec
        delay(500);
//        Serial.print(".");
        if (1 < try_count) { //2回までOK
#if MCU != 2
            Serial.println("[ERROR] can't connect to host");
#endif
            return false;
        }
        try_count++;
    }
//    Serial.println();
    
    String body = "{\"device\": \"" + String(device_name) + "\", \"count\": " + String(person_count) + ", \"datetime\": null}\r\n\r\n";
    size_t body_length = body.length();
//    Serial.println(body);

    String http_head = "POST /v1/users/" + String(thinger_user) + "/buckets/" + String(thinger_bucket) + "/data HTTP/1.1";

    client.println(http_head);
    client.println("Host: api.thinger.io");
    client.println("Authorization: Bearer " + String(token));
    client.println("User-Agent: " + String("M5Stack"));
    client.println("Connection: close");
    client.println("Cache-Control: no-cache");
    client.println("Content-Length: " + String(body_length));
    client.println("Content-Type: application/json");
    client.println();
    client.println(body);
    delay(50);

    String response = client.readString();
#if MCU != 2
    Serial.println(response);
#endif
    client.stop();

//    int bodypos =  response.indexOf("\r\n\r\n") + 4;
    //return response.substring(bodypos);
    return true; //一旦、送信さえできればOKとする。
}
