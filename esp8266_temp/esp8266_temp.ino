#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
OneWire oneWire(2);
DallasTemperature sensors(&oneWire);
DeviceAddress sn_18b20;

extern "C" {
#include "user_interface.h"
}


#define HOSTNAME "wifi-temp" ///< Hostename. The setup function adds the Chip ID at the end.

const char* ap_default_ssid = "wifi-temp"; ///< Default SSID. 不能作为wifi客户端连接到ap时，主动开的ap。
const char* ap_default_psk = ""; ///< Default PSK.

bool loadConfig(String *ssid, String *pass)
{
  // 打开文件读取.
  File configFile = SPIFFS.open("/cl_conf.txt", "r");
  if (!configFile)
  {
    Serial.println("Failed to open cl_conf.txt.");

    return false;
  }

  // 读取全部内容.
  String content = configFile.readString();
  configFile.close();

  content.trim();

  //  查找第二行的位置.
  int8_t pos = content.indexOf("\r\n");
  uint8_t le = 2;
  // 可能是linux或者 mac格式.
  if (pos == -1)
  {
    le = 1;
    pos = content.indexOf("\n");
    if (pos == -1)
    {
      pos = content.indexOf("\r");
    }
  }

  // 如果不是两行: 部分信息丢失.
  if (pos == -1)
  {
    Serial.println("Infvalid content.");
    Serial.println(content);

    return false;
  }

  // 将SSID和PSK写入.
  *ssid = content.substring(0, pos);
  *pass = content.substring(pos + le);

  ssid->trim();
  pass->trim();


  return true;
} // loadConfig


bool saveConfig(String *ssid, String *pass)
{
  // Open config file for writing.
  File configFile = SPIFFS.open("/cl_conf.txt", "w");
  if (!configFile)
  {
    Serial.println("Failed to open cl_conf.txt for writing");

    return false;
  }

  // Save SSID and PSK.
  configFile.println(*ssid);
  configFile.println(*pass);

  configFile.close();

  return true;
} // saveConfig

float temp = -999;
void setup()
{
  String station_ssid = "";
  String station_psk = "";

  wdt_disable();
  Serial.begin(115200);

  sensors.begin();
  if (sensors.getAddress(sn_18b20, 0))
  {
    sensors.setResolution(sn_18b20, 12);
    sensors.setWaitForConversion(true);
    sensors.requestTemperatures(); // Send the command to get temperatures
  }

  Serial.println("\r\n");
  Serial.print("Chip ID: 0x");
  Serial.println(ESP.getChipId(), HEX);

  if (!SPIFFS.begin())
  {
    Serial.println("Failed to mount file system");
    return;
  }

  if (! loadConfig(&station_ssid, &station_psk))
  {
    Serial.println("wifi set is fail.");
    AP(); //打开ap，等待客户端链接上来进行设置。
    return;
  }

  // Set Hostname.
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);

  // Print hostname.
  Serial.println("Hostname: " + hostname);

  // Check WiFi connection
  // ... check mode
  if (WiFi.getMode() != WIFI_STA)
  {
    WiFi.mode(WIFI_STA);
    delay(10);
  }

  // ... Compare file config with sdk config.
  if (WiFi.SSID() != station_ssid || WiFi.psk() != station_psk)
  {
    Serial.println("WiFi config changed.");

    // ... Try to connect to WiFi station.
    WiFi.begin(station_ssid.c_str(), station_psk.c_str());

    // ... Pritn new SSID
    Serial.print("new SSID: ");
    Serial.println(WiFi.SSID());

    // ... Uncomment this for debugging output.
    //WiFi.printDiag(Serial);
  }
  else
  {
    // ... Begin with sdk config.
    WiFi.begin();
  }

  Serial.println("Wait for WiFi connection.");

  // ... Give ESP 10 seconds to connect to station.
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000)
  {
    Serial.write('.');
    //Serial.print(WiFi.status());
    delay(500);
  }
  Serial.println();

  // Check connection
  if (WiFi.status() == WL_CONNECTED)
  {
    // ... print IP Address
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("Can not connect to WiFi station.");
    AP();
  }

}
void AP() {
  // Go into software AP mode.
  struct softap_config cfgESP;
  Serial.println("Go into AP mode.\r\nssid:wifi-temp\r\npasswd:none");
  WiFi.mode(WIFI_AP);

  while (!wifi_softap_get_config(&cfgESP)) {
    system_soft_wdt_feed();
  }
  cfgESP.authmode = AUTH_OPEN;//无密码模式
  wifi_softap_set_config(&cfgESP);
  delay(10);

  WiFi.softAP(ap_default_ssid, ap_default_psk);

  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());

  if (!MDNS.begin("temp")) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
}
uint32_t lasttime = 0;
void loop()
{
  if (lasttime + 10000 < millis()) {
    lasttime = millis() / 1000 * 1000;
    temp = sensors.getTempC(sn_18b20);
    Serial.print("temp=");
    Serial.print(temp);
    Serial.println("C");
    sensors.requestTemperatures();
  }
  system_soft_wdt_feed();
}

