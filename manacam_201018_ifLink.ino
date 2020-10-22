#define MCU 1 // 0: M5StickC, 1: ESP32DevKitC, 2: ESP32DevKitC with Serial(0)

#include <Arduino.h>
//ifLink対応のためにBLE接続のためのライブラリを追加
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "0000181a-0000-1000-8000-00805f9b34fb" // org.bluetooth.service.environmental_sensing
#define CHARACTERISTIC_UUID "00002a6e-0000-1000-8000-00805f9b34fb" // org.bluetooth.characteristic.unspecified


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

// const char* ssid = "TP-Link_CB98";//gikenbase-guest,SPWH_H32_0724F74,Umi,303ZTa-DFEEB9,TB-Link_CB98
// const char* passwd = "98049950";//g20190911,r92dt9i08byfn10,20612061,4437957a,98049950
// const char* host = "api.thinger.io";
// const char* thinger_user = "FutuRocket";
// const char* thinger_bucket = "manacam08";
// const char* token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiJtYW5hY2FtMDgiLCJ1c3IiOiJGdXR1Um9ja2V0In0.Vkljvk0Hl0EraVdVLkhqh481pjW1ldrmSOwdUv9PcpQ";
// const char* device_name = "manacam10@Home";

#if MCU != 2
HardwareSerial serial_ext(2);
#endif

static const int RX_BUF_SIZE = 20000;
static const uint8_t packet_begin[3] = { 0xFF, 0xD8, 0xEA };

void setup() {
#if MCU == 1
  Serial.begin(115200);
  Serial.flush();
  delay(50);
  Serial.print("ESP32DevKitC initializing...");
#endif

  // Create the BLE Device
  BLEDevice::init("ManaCam");

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

#if MCU != 2
  // UART RX=32(white), TX=33(yellow) Maix Bit 35 to 16, 34 to 17
  serial_ext.begin(115200, SERIAL_8N1, 16, 17);
#endif

}

void loop() {
  if (!Serial.available()) {
    return;
  }

  uint8_t rx_buffer[10];
  int rx_size = Serial.readBytes(rx_buffer, 10);

  if (rx_size != 10) return;
  if (!((rx_buffer[0] == packet_begin[0]) && (rx_buffer[1] == packet_begin[1]) && (rx_buffer[2] == packet_begin[2]))) return;

  // cmdSendPersonCount();

  // 人数カウントを取得
  int pcount = (uint32_t)(rx_buffer[4] << 16) | (rx_buffer[5] << 8) | rx_buffer[6];

#if MCU == 1
  Serial.print("recv pcount: ");
  Serial.println(pcount);
#else
  // nothing
#endif

  uint8_t data_buff[2];  // データ通知用バッファ
  
  if (deviceConnected) {
    // 値を設定してNotifyを発行
    data_buff[0] = (int16_t)(pcount * 100.0) & 0xff;
    data_buff[1] = ((int16_t)(pcount * 100.0) >> 8);
    pCharacteristic->setValue((uint8_t*)&value, 4);//data_buff, 2
    pCharacteristic->notify();
    value++;
    delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }

  // cloudに送信
  // postPersonCount(pcount);

#if MCU == 1
  Serial.println("uploaded to cloud");
#else
  // nothing
#endif

  vTaskDelay(10 / portTICK_RATE_MS);

}
