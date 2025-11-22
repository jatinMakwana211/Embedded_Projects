#include <PubSubClient.h>
#define MQTT_MAX_PACKET_SIZE 4096
#include <ArduinoJson.h>  
#include <WebServer.h>
#include "AWS_cred.h"
#include "mqtt_html.h"
#include "main_html.h"
#include "audio_settings.h"
#include "json.h"
#include "qrcode.h"
#include "function.h"
#include <string.h>
#include <TFT_eSPI.h>
#include <FS.h>
#include <LittleFS.h> // Changed from SPIFFS.h to LittleFS.h
#include <TJpg_Decoder.h> // JPEG decoder library
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <WiFiClientSecure.h>

TFT_eSPI tft = TFT_eSPI();
HTTPClient httpClient;
WiFiClientSecure espClient;

#define SCREEN_WIDTH 480  // Adjusted for 90° rotation (landscape)
#define SCREEN_HEIGHT 320
#define MAX_AUDIO_QUEUE 20

String IP;
String modifiedPage_html;
String modifiedPage_AWS;
String m1;
String m2;
String m3;
String m4;
String m5;
int temp_check;
int flag = 1;
int flag_1;
int temp_hotspot;
String mac_id = WiFi.macAddress();

String esid  = "";      // will hold the SSID we try to connect to
String epass = ""; 
String staticIP = "";
String subnetMask = "";
String gateway = "";

const unsigned int MAX_MESSAGE_LENGTH = 409;
static char message[MAX_MESSAGE_LENGTH];
String command;
char *strings[6];
char *ptr = NULL;

#define I2S_DIN  11   // Data in pin
#define I2S_BCLK 13   // Bit clock pin
#define I2S_LRC  12   // Left/Right clock pin

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceLittleFS *file = nullptr;
AudioFileSourceLittleFS *fileSource = nullptr;
AudioOutputI2S *out = nullptr;
bool isAudioPlaying = false;

bool playbackEnabled = true;
const char* mp3Directory = "/mp3";

// Image-related variables
std::vector<String> imageFiles;
int imageIndex = 0;
unsigned long lastImageChange = 0;
unsigned long displayTime = 5000; // Default 5 seconds, now configurable
bool autoRotate = false;

enum DisplayState {
  STATE_NONE,
  STATE_WIFI,
  STATE_QR_CODE,
  STATE_WELCOME,
  STATE_TOTAL,
  STATE_STATUS,
  STATE_IMAGE
};
DisplayState currentState = STATE_WIFI;
DisplayState lastState = STATE_NONE;

Preferences preferences;
//WiFiClient espClient;
PubSubClient client(espClient);

const char* domain = "a15dxhuue46y9n-ats.iot.us-east-1.amazonaws.com";
int port = 8883;
const char* AWSUser = "bonrix";
const char* AWSPassword = "bonrix123456789";

String temp_domain;
String temp_port;
//String temp_user;
//String temp_password;
String temp_pub_topic;
String temp_subscribe_topic;
String temp_global_subscribe_topic;
String temp_clientID;
const String topicROM = "glQpx7vTynreYch2T5Xgn8kDcLy8D9zB";

const char* awsEndpoint = "a15dxhuue46y9n-ats.iot.us-east-1.amazonaws.com"; 
//const char* awsEndpoint = temp_domain.c_str(); // AWS IoT endpoint
const int awsPort = 8883;  // Default port for secure MQTT
//const int awsPort = temp_port.toInt();
//const char* clientID = "DQRX00002";  // Unique client ID
const char* clientID ="";
bool awsconnected=false;
// Topics
//const char* subscribeTopic = "devices/DQRX00002/message";  // Topic to subscribe to
const char* subscribeTopic ="";
const char* publishTopic ="";  // Topic to publish to

uint64_t chipId = ESP.getEfuseMac();

String new_cred;
String ip;

WebServer server(80);

struct message {
  String inputMessage_1;
  String inputMessage_2;
  String inputMessage_3;
  String inputMessage_4;
  String inputMessage_5;
  String inputMessage_6;
  String inputMessage_7;
  uint16_t port_1;
  String Total;
} msg;

unsigned long lastLoopTime = 0;
const unsigned long LOOP_INTERVAL = 50;

int currentVolume = 21;
int global_saved_volume=21;  // At the top
float gain = 1.0f;


unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 60000;  
bool awsConfigFetched = false;
bool awsConfigTried   = false;

String rootfilename;
size_t rootfilesize;

String privetfilename;
size_t privetfilesize;

String cirtificatefilename;
size_t cirtificatefilesize;

String rootCACert = "";
String deviceCert = "";
String privateKey = "";


String amountAudioQueue[MAX_AUDIO_QUEUE];
int amountFileIndex = 0;
bool amountShouldPlayNext = false;
bool isAmountAudioPlaying = false;
bool wifiAudioCompleted = false; // Add this at the top with other global variables
bool ipAudioCompleted = false; // Tracks completion of IP audio queue

bool isDigits(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) {
            return false;
        }
    }
    return true;
}


void handleFileUploadMP3();
void handleFileUploadImage();
void displayImage(const char* filename);
void resizeAndDisplayImage(const char* filename);
void displayNextImage();
void loadImagesFromLittleFS(); // Changed from loadImagesFromSPIFFS
void saveImageList();
void handleListImages();
void handleDeleteImage();
void handleMoveImage();

void createWebServer();
void attemptWiFiConnection();
void setupAP();
void createWebServer();
void displayHotspotQRCode(const char* qrText, const char* ipText);
void printQRCodeAndDetails(const char* qrText, const char* ipText, const char* ssidText);
void displayConnectingScreen(const char* ssid, int attempt);
void configureStaticIP();
void handleResetWiFi();

void AWS_connection_files();
void publishMessage();
void reconnect();
void listFiles();
void downloadFile(const String& url, const char* destPath);
bool fetchAndStoreAWSConfig();
void displayAWSConnectedImage();
void handleAWSFileUploadCirtificate();
void handleAWSFileUploadPrivet();
void handleAWSFileUploadAmazon();

void setup() {
    Serial.begin(230400);

    // Initialize TFT display
    tft.begin();
    tft.setRotation(90);
    tft.fillScreen(TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(5, 20);
    tft.println("Bonrix Embedded Innovation");
    tft.setCursor(5, 60);
    tft.println("Model: BEI-CFD-002-WIFI");
    tft.setCursor(5, 100);
    tft.println("Version: 1.0.0");
    tft.setTextSize(1);
    tft.setCursor(15, 430);
    tft.println("www.embedded-innovations.com");

    // Initialize   LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("An error has occurred while mounting LittleFS");
        tft.setCursor(5, 200);
        tft.setTextSize(2);
        tft.println("LittleFS Mount Failed");
        while (1);
    }
    
      uint64_t chipId = ESP.getEfuseMac();  // Get unique MAC address
      Serial.println("\n ESP32 Chip ID: " + String(chipId));
      delay(2000);
    // List files
    Serial.println("LittleFS mounted successfully");
    Serial.println("Total space: " + String(LittleFS.totalBytes()));
    Serial.println("Used space: " + String(LittleFS.usedBytes()));
    File root = LittleFS.open("/");
    File fsFile = root.openNextFile();

    while (fsFile) {
        Serial.println("File: " + String(fsFile.name()));
        fsFile = root.openNextFile();
    }

    // Ensure MP3 directory exists
    if (!LittleFS.exists(mp3Directory)) {
        LittleFS.mkdir(mp3Directory);
    }

    // Initialize audio output
    out = new AudioOutputI2S();
    out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
    out->SetGain(global_saved_volume /21.0f);
    playbackEnabled = true;

    // Play default.mp3
    String defaultFilePath = String(mp3Directory) + "/default.mp3";
    if (LittleFS.exists(defaultFilePath)) {
        Serial.println("Playing: " + defaultFilePath);
        file = new AudioFileSourceLittleFS(defaultFilePath.c_str());
        mp3 = new AudioGeneratorMP3();
        mp3->begin(file, out);
        isAudioPlaying = true;
    } else {
        Serial.println("default.mp3 not found: " + defaultFilePath);
        tft.setCursor(5, 200);
        tft.setTextSize(2);
        tft.println("default.mp3 Not Found");
    }

    // Splash screen or audio playback for 5 seconds
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) {
        if (mp3 && mp3->isRunning()) {
            mp3->loop();
        }
    }

    // Clean up playback
    if (mp3 && mp3->isRunning()) mp3->stop();
    delete mp3;  mp3 = nullptr;
    delete file; file = nullptr;
    isAudioPlaying = false;

    // Reapply volume settings from saved int
    preferences.begin("audioPrefs", false);
    int savedVolume = preferences.getInt("volume", 21);
    global_saved_volume = savedVolume;
    Serial.println(global_saved_volume);
    playbackEnabled = preferences.getBool("playback_enabled", true);
    gain = constrain(global_saved_volume / 21.0f, 0.0f, 1.0f); //
    out->SetGain(gain);
    preferences.end();
    // Initialize image decoder
    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(tft_output);
    TJpgDec.setSwapBytes(true);
    loadImagesFromLittleFS();

    // Load display settings
    autoRotate = preferences.getBool("autoRotate", false);
    displayTime = preferences.getULong("displayTime", 5000);
    
    wifiAudioCompleted = false; // Ensure wifi.mp3 plays before AWS fetch

  preferences.begin("wifiCreds", false);
  mac_id.replace(":", "");
  preferences.end();
  preferences.begin("credentials", false);
  temp_hotspot = preferences.getInt("flag", flag);
  preferences.end();
  
    preferences.begin("wifi",true);
    esid     = preferences.getString("ssid", "");
    epass    = preferences.getString("pass", "");
    staticIP = preferences.getString("staticIP", "");
    subnetMask = preferences.getString("subnetMask", "");
    gateway    = preferences.getString("gateway", "");
    preferences.end();

    Serial.println("Loaded WiFi credentials:");
    Serial.println("SSID: " + esid);

      if (esid.length() > 0 && epass.length() > 0){
          attemptWiFiConnection();
          if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Connected to WiFi");
            IPAddress ip = WiFi.localIP();
            printQRCodeAndDetails(ip.toString().c_str(), ip.toString().c_str(), esid.c_str());
            Serial.println(ip);
            delay(1000);
            
          } else {
            Serial.println("Failed to connect to WiFi, entering hotspot mode");
            setupAP();
          }
       }
       else {
            Serial.println("No WiFi credentials, entering hotspot mode");
            setupAP();
        }
    
    if (temp_hotspot != 1) {
    IPAddress ip = WiFi.localIP();
    printQRCodeAndDetails(ip.toString().c_str(), ip.toString().c_str(), esid.c_str());
    }

    server.on("/", handleRoot);
    server.onNotFound(notFound);
   
    server.begin();
}

void loop() {

    server.handleClient();

    if (Serial.available()) {
        String message = Serial.readStringUntil('\n');
        message.trim();
        if (message.length() == 0) return;

        // Amount-only input shortcut
        if (isDigits(message.c_str())) {
            Serial.print("Amount entered: ");
            Serial.println(message);
            prepareAmountAudioQueue(message);
            amountFileIndex = 0; // Reset index
            amountShouldPlayNext = true; // Trigger playback
            isAmountAudioPlaying = false;
        } else {
            // Existing parsing of AWS/display commands
            static unsigned int message_pos = 0;
            char inByte = message[message_pos];
            if (inByte != '\n' && (message_pos < MAX_MESSAGE_LENGTH - 1)) {
                message[message_pos] = inByte;
                message_pos++;
            } else {
                message[message_pos] = '\0';
                Serial.print("Received Serial: ");
                Serial.println(message);
                message_pos = 0;
                byte index = 0;
                char messageBuffer[MAX_MESSAGE_LENGTH];
                strncpy(messageBuffer, message.c_str(), MAX_MESSAGE_LENGTH - 1);
                messageBuffer[MAX_MESSAGE_LENGTH - 1] = '\0';
                char* ptr = strtok(messageBuffer, "*");
                while (ptr != NULL && index < 6) {
                    strings[index] = ptr;
                    index++;
                    ptr = strtok(NULL, "*");
                }
                main_code();
            }
        }
    }

        if (mp3 && mp3->isRunning()) {
          if (!mp3->loop()) {
              mp3->stop();
              if (isAudioPlaying && !isAmountAudioPlaying) {
                  // wifi.mp3 just finished
                  isAudioPlaying = false;
                  wifiAudioCompleted = true;
                  Serial.println("wifi.mp3 playback completed");
                  // Start IP audio queue only if it hasn’t been completed yet
                  if (!ipAudioCompleted) {
                      prepareIPAudioQueue(WiFi.localIP().toString());
                      amountFileIndex = 0;
                      amountShouldPlayNext = true;
                      isAmountAudioPlaying = false;
                  }
              } else if (isAmountAudioPlaying) {
                  // IP audio or amount audio finished
                  isAmountAudioPlaying = false;
                  if (amountAudioQueue[amountFileIndex] != "") {
                      // More files in the queue
                      amountShouldPlayNext = true;
                  } else {
                      // IP audio queue completed
                      ipAudioCompleted = true;
                      Serial.println("IP audio queue playback completed");
                  }
              }
              delete mp3;
              mp3 = nullptr;
              delete file;
              file = nullptr;
          }
      } else if (amountShouldPlayNext) {
          amountShouldPlayNext = false;
          playNextAmountFile();
      }

      if (WiFi.status() == WL_CONNECTED && wifiAudioCompleted && ipAudioCompleted){
          if (!awsConfigTried) {
              Serial.println("WiFi connected — trying AWS config from preferences…");
              awsConfigTried = true;
              fetch_AWS_data();
              if (temp_domain.length() > 0 && temp_port.length() > 0 && temp_subscribe_topic.length() > 0 && temp_clientID.length() > 0){
                  AWS_connection_files();
                  reconnect();
                  awsConfigFetched = true;
              } else {
                  Serial.println("No AWS config in preferences — fetching from API…");
                  awsConfigFetched = fetchAndStoreAWSConfig();
                  if (awsConfigFetched) {
                      AWS_connection_files();
                      reconnect();
                  }
              }
          }
      } else if (WiFi.status() != WL_CONNECTED) {
          // If you lose WiFi, reset and try again next time
          awsConfigTried = false;
          wifiAudioCompleted = false; // Reset audio flag on WiFi disconnect
          ipAudioCompleted = false;
      }
    client.loop();
    // Publish every publishInterval milliseconds
    unsigned long now = millis();
    if(awsconnected){
    if (now - lastPublishTime >= publishInterval) {
      publishMessage();
      lastPublishTime = now;
      }
    }
  
  unsigned long currentTime = millis();

  // Throttle other operations
  if (currentTime - lastLoopTime < LOOP_INTERVAL) return;
  lastLoopTime = currentTime;

  while (Serial.available() > 0) {
    static unsigned int message_pos = 0;
    char inByte = Serial.read();
    if (inByte != '\n' && (message_pos < MAX_MESSAGE_LENGTH - 1)) {
      message[message_pos] = inByte;
      message_pos++;
    } else {
      message[message_pos] = '\0';
      Serial.print("Received Serial: ");
      Serial.println(message);
      message_pos = 0;
      byte index = 0;
      char* ptr = strtok(message, "*");
      while (ptr != NULL && index < 6) {
        strings[index] = ptr;
        index++;
        ptr = strtok(NULL, "");
      }
      main_code();
    }
  }
  displayNextImage();
}
// TFT JPEG Decoder callback (unchanged)
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

bool drawJpegFromLittleFS(const char* filename, int x, int y) {
    fs::File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.print("ERROR: Failed to open file: ");
        Serial.println(filename);
        return false;
    }

    // Verify JPEG header
    uint8_t header[2];
    if (file.read(header, 2) != 2 || header[0] != 0xFF || header[1] != 0xD8) {
        Serial.println("ERROR: Not a valid JPEG file: " + String(filename));
        file.close();
        return false;
    }
    file.seek(0); // Reset file pointer

    TJpgDec.setCallback(tft_output);
    bool result = TJpgDec.drawFsJpg(x, y, file);
    file.close();

    if (result) {
        Serial.println("JPEG drawn successfully: " + String(filename));
    } else {
        Serial.println("ERROR: Failed to draw JPEG: " + String(filename));
    }
    return result;
}

// Modified displayImageFromLittleFS (from reference code, adapted)
void displayImageFromLittleFS(const char* filename, int delayTime) {
    tft.setRotation(90); // Ensure consistent rotation
    if (!drawJpegFromLittleFS(filename, 0, 0)) {
        Serial.println("Failed to display image: " + String(filename));
        return;
    }

    unsigned long startTime = millis();
    while (millis() - startTime < delayTime) {
        if (Serial.available()) {
            return;
        }
    }
}

void temp() {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
}

void ssid_print(String ss_id, String ss_ip, String ss_mac) {
  unsigned long start = millis();
  tft.drawRect(10, 120, 300, 350, TFT_BLUE);
  tft.setTextSize(4);
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(20, 130);
  tft.print("SSID");
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(20, 170);
  tft.setTextSize(3);
  tft.print(ss_id);
  tft.setCursor(20, 220);
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(4);
  tft.print("IP");
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(20, 260);
  tft.setTextSize(3);
  tft.print(ss_ip);
  tft.setCursor(20, 310);
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(4);
  tft.print("MAC");
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(20, 350);
  tft.setTextSize(3);
  tft.print(ss_mac);
  Serial.printf("ssid_print took %lu ms\n", millis() - start);
}

void fetch_AWS_data(){
    /* open the SAME namespace and keys tha */
    preferences.begin("AWS", true);        // true = read-only
    temp_domain         = preferences.getString("domain",           "");
    temp_port           = preferences.getString("port",             "");
    temp_subscribe_topic = preferences.getString("topic",   "");
    temp_global_subscribe_topic=preferences.getString("global_sub_topic",   "");
    temp_pub_topic      = preferences.getString("publish_topic",     "");
    temp_clientID       = preferences.getString("clientid",     "");
    temp_check    = preferences.getInt   ("flag_1",   0);
    rootfilename        =preferences.getString("rootfilename",     "");
    rootfilesize        =preferences.getString("rootfilesize",     "").toInt();

    privetfilename        =preferences.getString("privetfilename",     "");
    privetfilesize        =preferences.getString("privetfilesize",     "").toInt();

    cirtificatefilename        =preferences.getString("cirtificatefilename",     "");
    cirtificatefilesize        =preferences.getString("cirtificatefilesize",     "").toInt();

    preferences.end();

    Serial.println("Fetched AWS credentials:");
    Serial.println("Domain          : " + temp_domain);
    Serial.println("Port            : " + temp_port); 
    Serial.println("Subscribe Topic : " +  temp_subscribe_topic);
    Serial.println("Global Subscribe Topic : " +  temp_global_subscribe_topic);
    Serial.println("Publish Topic   : " + temp_pub_topic);
    Serial.println("Client ID       : " + temp_clientID);
    Serial.println("Flag   : " + String(temp_check));

    printFileNamesAndSizes("/my-python-device-01");
}
    
void notFound() {
  server.send(404, "text/plain", "Not found");
}

void handleRoot() {
    modifiedPage_html = String(main_html);
    server.send(200, "text/html", modifiedPage_html);

  server.on("/home", [] () {
    unsigned long start = millis();
    msg.inputMessage_1 = server.arg("status");
    
  if (msg.inputMessage_1 == "0") {
    setupAP();
  }
    else if (msg.inputMessage_1 == "1") {
      fetch_AWS_data(); 
      modifiedPage_AWS = String(AWS_cred);
      modifiedPage_AWS.replace("##domain##", temp_domain);
      modifiedPage_AWS.replace("##port##", temp_port);
      modifiedPage_AWS.replace("##topic##",temp_subscribe_topic);
      modifiedPage_AWS.replace("##publish_topic##",temp_pub_topic);
      modifiedPage_AWS.replace("##global_sub_topic##",temp_global_subscribe_topic);
      modifiedPage_AWS.replace("##clientid##",temp_clientID);
      modifiedPage_AWS.replace("##rootfilename##",rootfilename);
      modifiedPage_AWS.replace("##rootfilesize##", String(rootfilesize));

      modifiedPage_AWS.replace("##privetfilename##",privetfilename);
      modifiedPage_AWS.replace("##privetfilesize##", String(privetfilesize));

      modifiedPage_AWS.replace("##cirtificatefilename##",cirtificatefilename);
      modifiedPage_AWS.replace("##cirtificatefilesize##", String(cirtificatefilesize));

      if (temp_check == 1) {
        modifiedPage_AWS.replace("##on##", "checked");
        modifiedPage_AWS.replace("##off##", "");
      } else if(temp_check == 0) {
        modifiedPage_AWS.replace("##on##", "");
        modifiedPage_AWS.replace("##off##", "checked");
      }
      Serial.println("ok_AWS*Redirect to AWS configuration Page " + IP + "*" + chipId);
      server.send(200, "text/html", modifiedPage_AWS);
      
      server.on("/AWS", [] () {
        msg.inputMessage_2 = server.arg("status");
        if (msg.inputMessage_2 == "1") {
          flag_1 = preferences.putInt("flag_1", 1);
          temp_check = preferences.getInt("flag_1", flag_1);  
          modifiedPage_AWS.replace("##on##", "checked");
          modifiedPage_AWS.replace("##off##", "");
          server.send(200, "text/html", "on");
          fetch_AWS_data();
          AWS_connection_files();
          reconnect();       
        }else{
          flag_1 = preferences.putInt("flag_1", 0); 
          temp_check = preferences.getInt("flag_1", flag_1); 
          modifiedPage_AWS.replace("##on##", "");
          modifiedPage_AWS.replace("##off##", "checked");
          server.send(200, "text/html", "off");
          
        }
      });
      
      server.on("/DisplayAWS",[] () {
    String newDomain = server.arg("Domain");
    String newPort   = server.arg("Port");
    String newTopic  = server.arg("Topic");
    String newglobalsubtopic  = server.arg("global_sub_topic");
    String newPublishTopic   = server.arg("PublishTopic");
    String newclientid=server.arg("clientid");
    /* 2. store them in (namespace AWS”) */
    preferences.begin("AWS", false);          // open for R/W
    preferences.putString("domain",   newDomain);
    preferences.putString("port",     newPort);
    preferences.putString("topic",    newTopic);
    preferences.putString("global_sub_topic",newglobalsubtopic);
    preferences.putString("publish_topic",newPublishTopic);
    preferences.putString("clientid",newclientid);
    preferences.end();                         // commit & close

    /* 3. refresh the RAM copies so the rest of the code
          sees the new values immediately */
    temp_domain   = newDomain;
    temp_port     = newPort;
    temp_pub_topic= newPublishTopic;
    temp_subscribe_topic=newTopic;
    temp_global_subscribe_topic= newglobalsubtopic;
    temp_clientID=newclientid;
    /* 4. log what we stored */
    Serial.println("┌──────────────────────────────────────────");
    Serial.println("│ AWS credentials saved to :");
    Serial.println("│   Domain           : " + temp_domain);
    Serial.println("│   Port             : " + temp_port);
    Serial.println("│   Topic            : " + temp_subscribe_topic);
    Serial.println("│  global Sub topic  : " + temp_global_subscribe_topic);
    Serial.println("│   Publish Topic    : " + temp_pub_topic);
    Serial.println("│   Client ID        : " + temp_clientID);
    Serial.println("└──────────────────────────────────────────");
    Serial.println("AWS*AWS_Saved*" + temp_port + "*" + WiFi.SSID() + "*" + WiFi.localIP().toString() + "*" + mac_id);
    
    server.send(200, "text/html", "AWS credentials Saved");
    });

    } 
    else if (msg.inputMessage_1 == "2") {
      modifiedPage_html = String(mqtt_html);
      modifiedPage_html.replace("<ip>", IP);
      server.send(200, "text/html", modifiedPage_html);
      
      server.on("/msg", [] () {
        unsigned long start = millis();
        msg.inputMessage_1 = server.arg("title");
        displayWelcomeMsg(msg.inputMessage_1.c_str());
        server.send(200, "text/html", "ok");
        Serial.printf("/msg took %lu ms\n", millis() - start);
      });

      server.on("/DisplayWelcomeMessage", [] () {
        unsigned long start = millis();
        msg.inputMessage_1 = server.arg("title");
        displayWelcomeScreen(msg.inputMessage_1.c_str());
        server.send(200, "text/html", "ok");
        Serial.printf("/DisplayWelcomeMessage took %lu ms\n", millis() - start);
      });

      server.on("/DisplayTotal", [] () {
        unsigned long start = millis();
        msg.inputMessage_1 = server.arg("totalamount");
        msg.inputMessage_2 = server.arg("savingdiscount");
        msg.inputMessage_3 = server.arg("taxamount");
        msg.inputMessage_4 = server.arg("grandtotal");
        displayTotalScreen(msg.inputMessage_1.c_str(), msg.inputMessage_2.c_str(), msg.inputMessage_3.c_str(), msg.inputMessage_4.c_str());
        server.send(200, "text/html", "ok");
        Serial.printf("/DisplayTotal took %lu ms\n", millis() - start);
      });

      server.on("/Displayqrcode", [] () {
        msg.inputMessage_1 =  server.arg("upiurl"); const char* msg_1 = msg.inputMessage_1.c_str();
        msg.inputMessage_2 =  server.arg("amount"); const char* msg_2 = msg.inputMessage_2.c_str();
        msg.inputMessage_3 =  server.arg("companyname"); const char* msg_3 = msg.inputMessage_3.c_str();
        msg.inputMessage_4 =  server.arg("shopnm"); const char* msg_4 = msg.inputMessage_4.c_str();
        msg.Total = "DisplayQRCodeScreen**" + msg.inputMessage_1 + "**" + msg.inputMessage_2 + "**" + msg.inputMessage_3 +"**" + msg.inputMessage_4 ;
        Serial.println(msg.Total);
        displayQRCodeScreen(msg.inputMessage_1.c_str(), msg.inputMessage_2.c_str(), msg.inputMessage_1.c_str(), msg.inputMessage_4.c_str()); // Pass "9" as a string
        server.send(200, "text/html", "ok");  
      });

      server.on("/DisplayQRCodeFailStatus", [] () {
        unsigned long start = millis();
        msg.inputMessage_1 = server.arg("orderid");
        msg.inputMessage_2 = server.arg("bankrrn");
        msg.inputMessage_3 = server.arg("date");
        displayFailQRCodeScreen(msg.inputMessage_1.c_str(), msg.inputMessage_2.c_str(), "0", msg.inputMessage_3.c_str());
        server.send(200, "text/html", "ok");
        Serial.printf("/DisplayQRCodeFailStatus took %lu ms\n", millis() - start);
      });

      server.on("/DisplayQRCodeCancelStatus", [] () {
        unsigned long start = millis();
        msg.inputMessage_1 = server.arg("orderid");
        msg.inputMessage_2 = server.arg("bankrrn");
        msg.inputMessage_3 = server.arg("date");
        displayCancelQRCodeScreen(msg.inputMessage_1.c_str(), msg.inputMessage_2.c_str(), "0", msg.inputMessage_3.c_str());
        server.send(200, "text/html", "ok");
        Serial.printf("/DisplayQRCodeCancelStatus took %lu ms\n", millis() - start);
      });

      server.on("/DisplayQRCodeSucessStatus", [] () {
        unsigned long start = millis();
        msg.inputMessage_1 = server.arg("orderid");
        msg.inputMessage_2 = server.arg("bankrrn");
        msg.inputMessage_3 = server.arg("date");
        displaySuccessQRCodeScreen(msg.inputMessage_1.c_str(), msg.inputMessage_2.c_str(), "0", msg.inputMessage_3.c_str()); 
      });
    } 
    else if (msg.inputMessage_1 == "3") {
      String mp3UploadPage = "<!DOCTYPE html><html><head>"
                            "<title>MP3 and Image Management</title>"
                            "<style>"
                            "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: rgb(177, 177, 177); }"
                            ".box { background-color: rgba(143, 224, 224, 0.836); padding: 20px; border: 5px solid black; border-radius: 30px; max-width: 600px; margin: 20px auto; }"
                            "h2 { text-align: center; text-decoration: underline; margin-bottom: 50px; font-size: 60px; }"
                            "h3 { text-align: center; margin: 20px 0; }"
                            "input[type='file'], input[type='submit'], button, input[type='number'] { margin: 10px 0; }"
                            "input[type='submit'], button { background-color: #ffffff; color: black; border: 2px solid black; padding: 10px 20px; border-radius: 5px; cursor: pointer; }"
                            "input[type='submit']:hover, button:hover { background-color: #ffa500; }"
                            "ul { list-style: none; padding: 0; }"
                            "li { margin: 10px 0; }"
                            "a { margin: 0 10px; text-decoration: none; color: #0066cc; }"
                            "a:hover { text-decoration: underline; }"
                            ".image-item { display: flex; justify-content: space-between; align-items: center; }"
                            ".button-group { display: flex; gap: 10px; }"
                            ".delete { background-color: #ffffff; border: 2px solid black; }"
                            ".delete:hover { background-color: #ffa500; }"
                            "</style>"
                            "<script>"
                            "function loadMP3List() {"
                            "  fetch('/listMP3').then(response => response.text()).then(data => {"
                            "    document.getElementById('mp3-list').innerHTML = data;"
                            "  });"
                            "}"
                            "function playMP3(filename) {"
                            "  fetch('/playMP3?file=' + encodeURIComponent(filename))"
                            "    .then(response => response.text())"
                            "    .then(data => alert(data))"
                            "    .catch(error => alert('Error playing MP3: ' + error));"
                            "}"
                            "function deleteMP3(filename) {"
                            "  if (confirm('Are you sure you want to delete ' + filename + '?')) {"
                            "    fetch('/deleteMP3?file=' + encodeURIComponent(filename))"
                            "      .then(() => loadMP3List());"
                            "  }"
                            "}"
                            "function loadImageList() {"
                            "  fetch('/listImages').then(response => response.text()).then(data => {"
                            "    document.getElementById('image-list').innerHTML = data;"
                            "  });"
                            "}"
                            "function moveImage(filename) {"
                            "  fetch('/moveImage?filename=' + encodeURIComponent(filename) + '&direction=' + direction)"
                            "    .then(() => loadImageList())"
                            "    .catch(error => alert('Error moving image: ' + error));"
                            "}"
                            "function deleteImage(filename) {"
                            "  if (confirm('Are you sure you want to delete ' + filename + '?')) {"
                            "    fetch('/deleteImage?filename=' + encodeURIComponent(filename))"
                            "      .then(() => loadImageList())"
                            "      .catch(error => alert('Error deleting image: ' + error));"
                            "  }"
                            "}"
                            "function startRotation() {"
                            "  fetch('/startRotation').then(() => alert('Image rotation started'));"
                            "}"
                            "function stopRotation() {"
                            "  fetch('/stopRotation').then(() => alert('Image rotation stopped'));"
                            "}"
                            "function setInterval() {"
                            "  const interval = document.getElementById('interval').value;"
                            "  fetch('/setRotationInterval?interval=' + interval).then(() => alert('Interval set to ' + interval + ' seconds'));"
                            "}"
                            "window.onload = function() { loadMP3List(); loadImageList(); };"
                            "</script></head><body>"
                            "<div class='box'>"
                            "<h3>MP3 Upload Section</h3>"
                            "<form method='POST' action='/uploadMP3' enctype='multipart/form-data'>"
                            "<input type='file' name='file' accept='.mp3'><br>"
                            "<input type='submit' value='Upload MP3 to LittleFS'>" // Changed text
                            "</form></div>"
                            "<div class='box'><h3>Uploaded MP3 Files</h3>"
                            "<div id='mp3-list' class='mp3-list'></div>"
                            "<button onclick='loadMP3List()'>Refresh MP3 List</button>"
                            "</div>"
                            "</body></html>";
      server.send(200, "text/html", mp3UploadPage);
    }
    else if (msg.inputMessage_1 == "4") {
      String audioSettingsPage = "<!DOCTYPE html><html><head>"
                                "<title>Audio Settings</title>"
                                "<style>"
                                "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: rgb(177, 177, 177); }"
                                ".box { background-color: rgba(143, 224, 224, 0.836); padding: 20px; border: 5px solid black; border-radius: 30px; max-width: 600px; margin: 20px auto; }"
                                "h2 { text-align: center; text-decoration: underline; margin-bottom: 50px; font-size: 60px; }"
                                "h3 { text-align: center; margin: 20px 0; }"
                                "input[type='number'], input[type='submit'], input[type='radio'] { margin: 10px 0; }"
                                "input[type='submit'] { background-color: #ffffff; color: black; border: 2px solid black; padding: 10px 20px; border-radius: 5px; cursor: pointer; }"
                                "input[type='submit']:hover { background-color: #ffa500; }"
                                "label { margin-right: 20px; }"
                                "</style></head><body>"
                                "<div class='box'>"
                                "<h2>Audio Settings</h2>"
                                "<h3>Volume Control</h3>"
                                "<form method='POST' action='/saveAudioSettings'>"
                                "<label>Volume (0-21):</label>"
                                "<input type='number' name='volume' value='%VOLUME%' min='0' max='21'><br>"
                                "<h3>Playback Control</h3>"
                                "<label><input type='radio' name='playback' value='1' %PLAYBACK_ON%> Enabled</label>"
                                "<label><input type='radio' name='playback' value='0' %PLAYBACK_OFF%> Disabled</label><br>"
                                "<input type='submit' value='Save Settings'>"
                                "</form>"
                                "</div></body></html>";
      audioSettingsPage.replace("%VOLUME%", String(global_saved_volume));
      audioSettingsPage.replace("%PLAYBACK_ON%", playbackEnabled ? "checked" : "");
      audioSettingsPage.replace("%PLAYBACK_OFF%", playbackEnabled ? "" : "checked");
      server.send(200, "text/html", audioSettingsPage);
    }
    
    Serial.printf("/home took %lu ms\n", millis() - start);
  });

  // MP3 Endpoints
  server.on("/listMP3", HTTP_GET, []() {
    File root = LittleFS.open(mp3Directory); // Changed from SPIFFS.open
    if (!root || !root.isDirectory()) {
      server.send(500, "text/plain", "Failed to open MP3 directory");
      return;
    }

    String html = "<ul>";
    File fsFile  = root.openNextFile();
    while (fsFile ) {
      String fileName = fsFile .name();
      fileName = fileName.substring(fileName.lastIndexOf('/') + 1); // Remove /mp3/ prefix
      if (fileName.endsWith(".mp3")) {
        html += "<li>" + fileName + " ";
        html += "<a href='#' onclick='playMP3(\"" + fileName + "\")'>Play</a> ";
        html += "<a href='#' onclick='deleteMP3(\"" + fileName + "\")'>Delete</a>";
        html += "</li>";
      }
      fsFile  = root.openNextFile();
    }
    html += "</ul>";
    server.send(200, "text/html", html);
  });

  server.on("/playMP3", HTTP_GET, []() {
    if (server.hasArg("file")) {
        String fileName = server.arg("file");
        String filePath = String(mp3Directory) + "/" + fileName;
        if (LittleFS.exists(filePath)) {
            if (playbackEnabled) {
                if (mp3 && mp3->isRunning()) {
                    mp3->stop();
                    delete mp3;
                    mp3 = nullptr;
                    delete file;
                    file = nullptr;
                }
                file = new AudioFileSourceLittleFS(filePath.c_str());
                mp3 = new AudioGeneratorMP3();
                mp3->begin(file, out);
                isAudioPlaying = true;
                server.send(200, "text/plain", "Playing: " + fileName);
            } else {
                server.send(403, "text/plain", "Playback is disabled");
            }
        } else {
            server.send(404, "text/plain", "File not found: " + fileName);
        }
    } else {
        server.send(400, "text/plain", "Missing 'file' parameter");
    }
  });

  server.on("/deleteMP3", HTTP_GET, []() {
    if (server.hasArg("file")) {
      String fileName = server.arg("file");
      String filePath = String(mp3Directory) + "/" + fileName;
      if (LittleFS.exists(filePath)) { // Changed from SPIFFS.exists
        if (LittleFS.remove(filePath)) { // Changed from SPIFFS.remove
          server.send(200, "text/plain", "File deleted successfully: " + fileName);
        } else {
          server.send(500, "text/plain", "Failed to delete file: " + fileName);
        }
      } else {
        server.send(404, "text/plain", "File not found: " + fileName);
      }
    } else {
      server.send(400, "text/plain", "Missing 'file' parameter");
    }
  });

  server.on("/uploadMP3", HTTP_POST, []() {
    server.send(200, "text/plain", "MP3 file uploaded");
  }, handleFileUploadMP3);

  // Image Endpoints
  server.on("/uploadImage", HTTP_POST, []() {
    server.send(200, "text/plain", "Image uploaded to LittleFS"); // Changed text
  }, handleFileUploadImage);

// mainServer.on("/upload_LittleFS", HTTP_POST, []() {
//     mainServer.send(200, "text/plain", "File uploaded to LittleFS");
// }, handleFileUpload);


  server.on("/listImages", HTTP_GET, []() {
    String html = "";
    File root = LittleFS.open("/"); // Changed from SPIFFS.open
    if (!root || !root.isDirectory()) {
      server.send(500, "text/plain", "Failed to open directory");
      return;
    }

    File file = root.openNextFile();
    while (file) {
      String fileName = file.name();
      if (fileName.endsWith(".jpg") || fileName.endsWith(".jpeg")) {
        String displayName = fileName.startsWith("/") ? fileName.substring(1) : fileName;
        html += "<div class='image-item'>";
        html += displayName;
        html += "<div class='button-group'>";
        html += "<button onclick='moveImage(\"" + displayName + "\", \"up\")'>Up</button>";
        html += "<button onclick='moveImage(\"" + displayName + "\", \"down\")'>Down</button>";
        html += "<button class='delete' onclick='deleteImage(\"" + displayName + "\")'>Delete</button>";
        html += "</div></div>";
      }
      file = root.openNextFile();
    }
    server.send(200, "text/html", html);
  });

  server.on("/deleteImage", HTTP_GET, []() {
    if (server.hasArg("filename")) {
      String fileName = "/" + server.arg("filename");
      if (LittleFS.exists(fileName)) { // Changed from SPIFFS.exists
        if (LittleFS.remove(fileName)) { // Changed from SPIFFS.remove
          for (auto it = imageFiles.begin(); it != imageFiles.end(); ++it) {
            if (*it == fileName) {
              imageFiles.erase(it);
              break;
            }
          }
          saveImageList();
          server.send(200, "text/plain", "File deleted successfully: " + fileName);
        } else {
          server.send(500, "text/plain", "Failed to delete file: " + fileName);
        }
      } else {
        server.send(404, "text/plain", "File not found: " + fileName);
      }
    } else {
      server.send(400, "text/plain", "Missing 'filename' parameter");
    }
  });

  server.on("/moveImage", HTTP_GET, []() {
    if (server.hasArg("filename") && server.hasArg("direction")) {
      String fileName = "/" + server.arg("filename");
      String direction = server.arg("direction");
      auto it = std::find(imageFiles.begin(), imageFiles.end(), fileName);
      if (it != imageFiles.end()) {
        int index = std::distance(imageFiles.begin(), it);
        if (direction == "up" && index > 0) {
          std::swap(imageFiles[index], imageFiles[index - 1]);
        } else if (direction == "down" && index < imageFiles.size() - 1) {
          std::swap(imageFiles[index], imageFiles[index + 1]);
        }
        saveImageList();
        server.send(200, "text/plain", "Image moved");
      } else {
        server.send(404, "text/plain", "File not found in image list");
      }
    } else {
      server.send(400, "text/plain", "Missing 'filename' or 'direction' parameter");
    }
  });

server.on("/playImage", HTTP_GET, []() {
  if (server.hasArg("file")) {
    String fileName = "/" + server.arg("file");
    if (LittleFS.exists(fileName)) {
      // Use resizeAndDisplayImage for scaled display (default)
      resizeAndDisplayImage(fileName.c_str());
      // Alternatively, test with displayImageFromLittleFS for raw display
      // displayImageFromLittleFS(fileName.c_str(), 5000); // Uncomment to test
      server.send(200, "text/plain", "Displaying: " + fileName);
    } else {
      Serial.println("File not found in LittleFS: " + fileName);
      server.send(404, "text/plain", "File not found: " + fileName);
    }
  } else {
    server.send(400, "text/plain", "Missing 'file' parameter");
  }
});

  // Image Rotation APIs
  server.on("/startRotation", HTTP_GET, []() {
    autoRotate = true;
    preferences.putBool("autoRotate", autoRotate);
    server.send(200, "text/plain", "Image rotation started");
  });

  server.on("/stopRotation", HTTP_GET, []() {
    autoRotate = false;
    preferences.putBool("autoRotate", autoRotate);
    server.send(200, "text/plain", "Image rotation stopped");
  });

server.on("/setRotationInterval", HTTP_GET, []() {
  if (server.hasArg("interval")) {
    int intervalSeconds = server.arg("interval").toInt();
    intervalSeconds = constrain(intervalSeconds, 0, 60);   // clamp 0-60 s

    displayTime = intervalSeconds * 1000UL;                // ✅ millis
    preferences.putULong("displayTime", displayTime);
    Serial.println("New Image Rotation interval is :" + displayTime);

    lastImageChange = millis();                            // start new cycle
    server.send(200, "text/plain",
                "Rotation interval set to " + String(intervalSeconds) + " seconds");
  } else {
    server.send(400, "text/plain", "Missing 'interval' parameter");
  }
});

  // Start Image Rotation
  server.on("/api/startRotation", HTTP_GET, []() {
    if (imageFiles.empty()) {
      loadImagesFromLittleFS(); // Changed from loadImagesFromSPIFFS
      if (imageFiles.empty()) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No images found in LittleFS\"}"); // Changed text
        return;
      }
    }
    autoRotate = true;
    preferences.putBool("autoRotate", autoRotate);
    lastImageChange = millis(); // Reset timer to start rotation immediately
    imageIndex = 0; // Start from the first image
    resizeAndDisplayImage(imageFiles[imageIndex].c_str()); // Display first image immediately
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Image rotation started\"}");
  });

  // Stop Image Rotation
  server.on("/api/stopRotation", HTTP_GET, []() {
    autoRotate = false;
    preferences.putBool("autoRotate", autoRotate);
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Image rotation stopped\"}");
  });

  // Set Rotation Interval
  server.on("/api/setRotationInterval", HTTP_GET, []() {
    if (server.hasArg("interval")) {
      int intervalSeconds = server.arg("interval").toInt();
      if (intervalSeconds < 0) intervalSeconds = 0; // Minimum 0 seconds
      if (intervalSeconds > 60) intervalSeconds = 60; // Maximum 60 seconds
      displayTime = intervalSeconds * 1000; // Convert to milliseconds
      preferences.putULong("displayTime", displayTime);
      lastImageChange = millis(); // Reset timer to apply new interval immediately
      if (autoRotate && !imageFiles.empty()) {
        resizeAndDisplayImage(imageFiles[imageIndex].c_str()); // Show current image with new interval
      }
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Rotation interval set to " + String(intervalSeconds) + " seconds\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing interval parameter\"}");
    }
  });

  // Image Management API (supports .jpg and .jpeg)
  server.on("/api/deleteImage", HTTP_GET, []() {
    if (server.hasArg("filename")) {
      String fileName = "/" + server.arg("filename");
      String fileNameLower = fileName;
      fileNameLower.toLowerCase();
      if (!(fileNameLower.endsWith(".jpg") || fileNameLower.endsWith(".jpeg"))) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Only .jpg and .jpeg formats are supported\"}");
        return;
      }
      if (LittleFS.exists(fileName)) { // Changed from SPIFFS.exists
        LittleFS.remove(fileName); // Changed from SPIFFS.remove
        imageFiles.erase(std::remove(imageFiles.begin(), imageFiles.end(), fileName), imageFiles.end());
        saveImageList();
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Image deleted: " + fileName + "\"}");
      } else {
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"Image not found: " + fileName + "\"}");
      }
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing filename parameter\"}");
    }
  });

  // MP3 Management APIs
  server.on("/api/deleteMP3", HTTP_GET, []() {
    if (server.hasArg("filename")) {
      String filePath = String(mp3Directory) + "/" + server.arg("filename");
      String filePathLower = filePath;
      filePathLower.toLowerCase();
      if (!filePathLower.endsWith(".mp3")) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Only .mp3 format is supported\"}");
        return;
      }
      if (LittleFS.exists(filePath)) { // Changed from SPIFFS.exists
        LittleFS.remove(filePath); // Changed from SPIFFS.remove
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"MP3 deleted: " + server.arg("filename") + "\"}");
      } else {
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"MP3 not found: " + server.arg("filename") + "\"}");
      }
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing filename parameter\"}");
    }
  });

  server.on("/api/playMP3", HTTP_GET, []() {
    if (server.hasArg("filename")) {
      String filePath = String(mp3Directory) + "/" + server.arg("filename");
      String filePathLower = filePath;
      filePathLower.toLowerCase();
      if (!filePathLower.endsWith(".mp3")) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Only .mp3 format is supported\"}");
        return;
      }
      if (LittleFS.exists(filePath)) { // Changed from SPIFFS.exists
        if (playbackEnabled) {
          if (mp3 && mp3->isRunning()) {
  mp3->stop();
  delete mp3;
  mp3 = nullptr;
  delete file;
  file = nullptr;
}
          file = new AudioFileSourceLittleFS(filePath.c_str());
          mp3 = new AudioGeneratorMP3();
          mp3->begin(file, out);

          isAudioPlaying = true;
          server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Playing MP3: " + server.arg("filename") + "\"}");
        } else {
          server.send(403, "application/json", "{\"status\":\"error\",\"message\":\"Playback is disabled\"}");
        }
      } else {
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"MP3 not found: " + server.arg("filename") + "\"}");
      }
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing filename parameter\"}");
    }
  });

  // Volume Control APIs
  server.on("/api/setVolume", HTTP_GET, []() {
    if (server.hasArg("volume")) {
      int volume = server.arg("volume").toInt();
      if (volume >= 0 && volume <= 21) {

      float gain = constrain(volume / 21.0, 0.0, 1.0);
      out->SetGain(gain);
      currentVolume = volume;
      preferences.putInt("volume", currentVolume);

        preferences.putInt("volume", volume);
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Volume set to " + String(volume) + "\"}");
      } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Volume must be between 0 and 21\"}");
      }
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing volume parameter\"}");
    }
  });

  server.on("/api/increaseVolume", HTTP_GET, []() {
    int currentVolume = global_saved_volume;
    if (currentVolume < 21) {
      currentVolume++;
      float gain = constrain(currentVolume / 21.0, 0.0, 1.0);
      out->SetGain(gain);
      preferences.putInt("volume", currentVolume);
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Volume increased to " + String(currentVolume) + "\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Volume already at maximum (21)\"}");
    }
  });

  server.on("/api/decreaseVolume", HTTP_GET, []() {
    int currentVolume = global_saved_volume;
    if (currentVolume > 0) {
      currentVolume--;
      float gain = constrain(currentVolume / 21.0, 0.0, 1.0);
      out->SetGain(gain);
      preferences.putInt("volume", currentVolume);
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Volume decreased to " + String(currentVolume) + "\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Volume already at minimum (0)\"}");
    }
  });

  // Playback Enable/Disable API
  server.on("/api/setPlayback", HTTP_GET, []() {
    if (server.hasArg("enable")) {
      bool enable = server.arg("enable").toInt() == 1;
      playbackEnabled = enable;
      preferences.putBool("playback_enabled", playbackEnabled);
      if (!enable && isAudioPlaying) {
        if (mp3 && mp3->isRunning()) {
  mp3->stop();
  delete mp3;
  mp3 = nullptr;
  delete file;
  file = nullptr;
}
        isAudioPlaying = false;
      }
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Playback " + String(enable ? "enabled" : "disabled") + "\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing enable parameter (0 or 1)\"}");
    }
  });

  // Get Volume API
  server.on("/api/getVolume", HTTP_GET, []() {
    int currentVolume = global_saved_volume;
    String jsonResponse = "{\"status\":\"success\",\"volume\":" + String(currentVolume) + "}";
    server.send(200, "application/json", jsonResponse);
  });

  // List Images API (supports .jpg and .jpeg)
  server.on("/api/listImages", HTTP_GET, []() {
    long totalBytes = LittleFS.totalBytes(); // Changed from SPIFFS.totalBytes
    long usedBytes = LittleFS.usedBytes(); // Changed from SPIFFS.usedBytes
    long availableBytes = totalBytes - usedBytes;

    String filesJson = "[";
    File root = LittleFS.open("/"); // Changed from SPIFFS.open
    File file = root.openNextFile();
    while (file) {
      String fileName = file.name();
      String fileNameLower = fileName;
      fileNameLower.toLowerCase();
      if (file.size() > 0 && (fileNameLower.endsWith(".jpg") || fileNameLower.endsWith(".jpeg"))) {
        if (filesJson.length() > 1) filesJson += ",";
        filesJson += "{\"filename\":\"" + fileName + "\",\"size\":" + String(file.size()) + "}";
      }
      file = root.openNextFile();
    }
    filesJson += "]";

    String jsonResponse = "{\"status\":\"success\",\"data\":{";
    jsonResponse += "\"totalBytes\":" + String(totalBytes) + ",";
    jsonResponse += "\"usedBytes\":" + String(usedBytes) + ",";
    jsonResponse += "\"availableBytes\":" + String(availableBytes) + ",";
    jsonResponse += "\"files\":" + filesJson;
    jsonResponse += "}}";

    server.send(200, "application/json", jsonResponse);
  });

  // List MP3s API
  server.on("/api/listMP3", HTTP_GET, []() {
    long totalBytes = LittleFS.totalBytes(); // Changed from SPIFFS.totalBytes
    long usedBytes = LittleFS.usedBytes(); // Changed from SPIFFS.usedBytes
    long availableBytes = totalBytes - usedBytes;

    String filesJson = "[";
    File root = LittleFS.open(mp3Directory); // Changed from SPIFFS.open
    if (!root) {
      server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to open MP3 directory\"}");
      return;
    }
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory() && file.size() > 0) {
        String fileName = file.name();
        String fileNameLower = fileName;
        fileNameLower.toLowerCase();
        if (fileNameLower.endsWith(".mp3")) {
          if (filesJson.length() > 1) filesJson += ",";
          filesJson += "{\"filename\":\"" + fileName + "\",\"size\":" + String(file.size()) + "}";
        }
      }
      file = root.openNextFile();
    }
    filesJson += "]";

    String jsonResponse = "{\"status\":\"success\",\"data\":{";
    jsonResponse += "\"totalBytes\":" + String(totalBytes) + ",";
    jsonResponse += "\"usedBytes\":" + String(usedBytes) + ",";
    jsonResponse += "\"availableBytes\":" + String(availableBytes) + ",";
    jsonResponse += "\"files\":" + filesJson;
    jsonResponse += "}}";

    server.send(200, "application/json", jsonResponse);
  });

    server.on("/saveAudioSettings", HTTP_POST, []() {
        int new_volume = -1; // default invalid
        if (server.hasArg("volume")) {
            new_volume = server.arg("volume").toInt();
            if (new_volume < 0) new_volume = 0;
            if (new_volume > 21) new_volume = 21;

            float gain = constrain(new_volume / 21.0, 0.0, 1.0);

            // Save to Preferences (NVS)
            preferences.begin("audioPrefs", false);
            preferences.putInt("volume", new_volume);
            if (server.hasArg("playback")) {
                playbackEnabled = server.arg("playback").toInt() == 1;
                preferences.putBool("playback_enabled", playbackEnabled);
            }
            preferences.end();

            // Apply immediately
            out->SetGain(gain);
            global_saved_volume = new_volume;

            Serial.printf("Volume updated to %d and saved to Preferences\n", new_volume);
            server.send(200, "text/plain", "Audio settings saved");
        } else {
            server.send(400, "text/plain", "Volume must be between 0 and 21");
            return;
        }
    });

      server.on("/UploadAmazonRoot", HTTP_POST, []() {
      server.send(200, "text/plain", "File upload complete.");
    }, handleAWSFileUploadAmazon);

      server.on("/UploadPrivateKey", HTTP_POST, []() {
      server.send(200);
    }, handleAWSFileUploadPrivet);

      server.on("/UploadCertificate", HTTP_POST, []() {
      server.send(200);
    }, handleAWSFileUploadCirtificate);

}

// Image and MP3 Handling Functions
void handleFileUploadMP3() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = String(mp3Directory) + "/" + upload.filename;
    fs::File file = LittleFS.open(filename, "w"); // Changed from SPIFFS.open
    if (file) {
      Serial.println("Uploading file: " + filename);
    } else {
      Serial.println("Failed to open file for writing: " + filename);
    }
    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (upload.currentSize > 0) {
      fs::File file = LittleFS.open(String(mp3Directory) + "/" + upload.filename, "a"); // Changed from SPIFFS.open
      if (file) {
        file.write(upload.buf, upload.currentSize);
        Serial.printf("Wrote %d bytes to file\n", upload.currentSize);
        file.close();
      } else {
        Serial.println("Failed to append to file");
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (upload.totalSize > 0) {
      Serial.println("File upload complete. Total size: " + String(upload.totalSize));
    } else {
      Serial.println("Upload failed - no data received");
    }
  }
}

// Updated handleFileUploadImage (ensure file is closed and flushed)
void handleFileUploadImage() {
    HTTPUpload& upload = server.upload();
    static File file;

    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        // Sanitize filename
        filename.replace(" ", "_");
        filename.replace("(", "");
        filename.replace(")", "");
        filename.replace("[", "");
        filename.replace("]", "");
        filename.replace("{", "");
        filename.replace("}", "");
        filename.replace(",", "_");
        filename.replace(";", "_");
        filename.toLowerCase();
        filename = "/" + filename;
        Serial.print("Upload File Name: "); Serial.println(filename);
        if (LittleFS.exists(filename)) {
            LittleFS.remove(filename);
        }
        file = LittleFS.open(filename, FILE_WRITE);
        if (!file) {
            Serial.println("Failed to open file for writing: " + filename);
            server.send(500, "text/plain", "Failed to open file");
            return;
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (file) {
            file.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (file) {
            file.close();
            delay(100); // Ensure LittleFS flushes the file
            Serial.print("Upload File Size: "); Serial.println(upload.totalSize);
            String filename = "/" + upload.filename;
            // Apply same sanitization
            filename.replace(" ", "_");
            filename.replace("(", "");
            filename.replace(")", "");
            filename.replace("[", "");
            filename.replace("]", "");
            filename.replace("{", "");
            filename.replace("}", "");
            filename.replace(",", "_");
            filename.replace(";", "_");
            filename.toLowerCase();
            if (LittleFS.exists(filename)) {
                Serial.println("File uploaded successfully: " + filename);
                // Verify JPEG header
                File testFile = LittleFS.open(filename, "r");
                if (testFile) {
                    uint8_t header[2];
                    if (testFile.read(header, 2) == 2) {
                        if (header[0] == 0xFF && header[1] == 0xD8) {
                            Serial.println("Valid JPEG file detected (FF D8 header)");
                        } else {
                            Serial.println("ERROR: Uploaded file is not a valid JPEG");
                            LittleFS.remove(filename);
                            testFile.close();
                            server.send(400, "text/plain", "Invalid JPEG file");
                            return;
                        }
                    } else {
                        Serial.println("ERROR: Failed to read JPEG header");
                        LittleFS.remove(filename);
                        testFile.close();
                        server.send(400, "text/plain", "Failed to read file");
                        return;
                    }
                    testFile.close();
                } else {
                    Serial.println("ERROR: Failed to open file for header check: " + filename);
                    server.send(500, "text/plain", "Failed to verify file");
                    return;
                }
                if (std::find(imageFiles.begin(), imageFiles.end(), filename) == imageFiles.end()) {
                    imageFiles.push_back(filename);
                    Serial.println("Added to imageFiles: " + filename);
                    saveImageList();
                }
                // Set display state and display image
                currentState = STATE_IMAGE;
                lastState = STATE_IMAGE;
                resizeAndDisplayImage(filename.c_str());
                server.send(200, "text/plain", "Image uploaded and displayed");
            } else {
                Serial.println("File upload failed - not found after writing: " + filename);
                server.send(500, "text/plain", "File upload failed");
            }
        } else {
            Serial.println("Failed to open file for closing");
            server.send(500, "text/plain", "File upload failed");
        }
    }
}

// Modified displayNextImage (set display state)
void displayNextImage() {
    if (!autoRotate) return;
    unsigned long currentMillis = millis();
    if (currentMillis - lastImageChange >= displayTime) {
        lastImageChange = currentMillis;
        if (!imageFiles.empty()) {
            imageIndex = (imageIndex + 1) % imageFiles.size();
            String fileName = imageFiles[imageIndex];
            currentState = STATE_IMAGE;
            lastState = STATE_IMAGE;
            resizeAndDisplayImage(fileName.c_str());
        } else {
            Serial.println("No files found in LittleFS.");
        }
    }
}

// Updated resizeAndDisplayImage (fallback to no scaling if size retrieval fails)
void resizeAndDisplayImage(const char* filename) {
    String fileStr = String(filename);
    fileStr.toLowerCase();
    if (!fileStr.endsWith(".jpg") && !fileStr.endsWith(".jpeg")) {
        Serial.println("Unsupported image format: " + fileStr);
        return;
    }

    // Verify file existence
    if (!LittleFS.exists(filename)) {
        Serial.print("ERROR: File not found: "); Serial.println(filename);
        return;
    }

    // Set display state and clear screen
    currentState = STATE_IMAGE;
    lastState = STATE_IMAGE;
    tft.fillScreen(TFT_BLACK);

    // Try to get JPEG dimensions for scaling
    uint16_t jpegWidth, jpegHeight;
    int decodeResult = TJpgDec.getFsJpgSize(&jpegWidth, &jpegHeight, filename);
    if (decodeResult == 0) {
        // Successful size retrieval, apply scaling
        float scale = min((float)SCREEN_WIDTH / jpegWidth, (float)SCREEN_HEIGHT / jpegHeight);
        int scaleFactor = 1;
        if (scale >= 1.0) {
            scaleFactor = 1;
        } else if (scale >= 0.5) {
            scaleFactor = 2;
        } else {
            scaleFactor = 4;
        }
        TJpgDec.setJpgScale(scaleFactor);

        jpegWidth /= scaleFactor;
        jpegHeight /= scaleFactor;

        int offsetX = (SCREEN_WIDTH - jpegWidth) / 2;
        int offsetY = (SCREEN_HEIGHT - jpegHeight) / 2;

        Serial.print("Scaled width: "); Serial.println(jpegWidth);
        Serial.print("Scaled height: "); Serial.println(jpegHeight);
        Serial.print("Offset X: "); Serial.println(offsetX);
        Serial.print("Offset Y: "); Serial.println(offsetY);

        if (!drawJpegFromLittleFS(filename, offsetX, offsetY)) {
            Serial.println("Failed to display scaled JPEG: " + String(filename));
        }
    } else {
        // Size retrieval failed, try displaying without scaling
        Serial.print("ERROR: Failed to get JPEG size for: "); Serial.println(filename);
        Serial.println("Decoding result: " + String(decodeResult));
        Serial.println("Attempting to display without scaling...");
        TJpgDec.setJpgScale(1); // Reset to no scaling
        if (!drawJpegFromLittleFS(filename, 0, 0)) {
            Serial.println("Failed to display unscaled JPEG: " + String(filename));
        } else {
            Serial.println("Unscaled JPEG displayed successfully: " + String(filename));
        }
    }
}

void displayImage(const char* filename) {
    String file_dep = String(filename);
    file_dep.toLowerCase();
    if (!file_dep.endsWith(".jpg") && !file_dep.endsWith(".jpeg")) {
        Serial.println("Unsupported image format: " + file_dep);
        return;
    }

    // Verify file existence
    if (!LittleFS.exists(filename)) {
        Serial.print("ERROR: File not found: "); Serial.println(filename);
        return;
    }

    // Set display state
    currentState = STATE_IMAGE;
    lastState = STATE_IMAGE;
    tft.fillScreen(TFT_BLACK);

    // Display without scaling
    TJpgDec.setJpgScale(1);
    if (!drawJpegFromLittleFS(filename, 0, 0)) {
        Serial.println("Failed to display image: " + String(filename));
    }
}

void loadImagesFromLittleFS() {
  imageFiles.clear();
  File file = LittleFS.open("/image_list.txt", "r");
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() > 0 && LittleFS.exists(line)) {
        imageFiles.push_back(line);
      }
    }
    file.close();
    Serial.println("Loaded images from /image_list.txt:");
    for (const auto& img : imageFiles) {
      Serial.println(img);
    }
  }
  if (imageFiles.empty()) {
    Serial.println("No images in /image_list.txt or file missing, scanning LittleFS...");
    File root = LittleFS.open("/");
    File imgFile = root.openNextFile();
    while (imgFile) {
      String fileName = "/" + String(imgFile.name());
      if (fileName.endsWith(".jpg") || fileName.endsWith(".jpeg")) {
        if (LittleFS.exists(fileName)) {
          imageFiles.push_back(fileName);
          Serial.println("Found image: " + fileName);
        }
      }
      imgFile = root.openNextFile();
    }
    saveImageList();
    Serial.println("Images found in LittleFS:");
    for (const auto& img : imageFiles) {
      Serial.println(img);
    }
  }
}

void saveImageList() {
  File file = LittleFS.open("/image_list.txt", "w"); // Changed from SPIFFS.open
  if (file) {
    for (const auto& fileName : imageFiles) {
      file.println(fileName);
    }
    file.close();
  }
}

void handleListImages() {
  String fileList = "";
  for (size_t i = 0; i < imageFiles.size(); i++) {
    String displayFileName = imageFiles[i].substring(1);  // Remove the leading '/'
    fileList += "<div class='image-item'>"
                "<span class='image-name'>" + displayFileName + "</span>"
                "<div class='button-group'>"
                "<button onclick=\"moveImage('" + imageFiles[i] + "', 'up')\">Up</button>"
                "<button onclick=\"moveImage('" + imageFiles[i] + "', 'down')\">Down</button>"
                "<button class='delete' onclick=\"deleteImage('" + imageFiles[i] + "')\">Delete</button>"
                "</div></div>";
  }
  server.send(200, "text/html", fileList);
}

void handleDeleteImage() {
  if (server.hasArg("filename")) {
    String fileName = "/" + server.arg("filename");
    if (LittleFS.exists(fileName)) { // Changed from SPIFFS.exists
      LittleFS.remove(fileName); // Changed from SPIFFS.remove
      imageFiles.erase(std::remove(imageFiles.begin(), imageFiles.end(), fileName), imageFiles.end());
      saveImageList();
      server.send(200, "text/plain", "File deleted: " + fileName);
    } else {
      server.send(404, "text/plain", "File not found: " + fileName);
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleMoveImage() {
  if (server.hasArg("filename") && server.hasArg("direction")) {
    String filename = server.arg("filename");
    String direction = server.arg("direction");
    int index = -1;

    for (size_t i = 0; i < imageFiles.size(); i++) {
      if (imageFiles[i] == filename) {
        index = i;
        break;
      }
    }

    if (index != -1) {
      if (direction == "up" && index > 0) {
        std::swap(imageFiles[index], imageFiles[index - 1]);
        saveImageList();
        server.send(200, "text/plain", "File moved up");
      } else if (direction == "down" && index < imageFiles.size() - 1) {
        std::swap(imageFiles[index], imageFiles[index + 1]);
        saveImageList();
        server.send(200, "text/plain", "File moved down");
      } else {
        server.send(400, "text/plain", "Invalid move request");
      }
    } else {
      server.send(404, "text/plain", "File not found");
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// Existing Display Functions
// Updated displayWelcomeScreen (use LittleFS)
void displayWelcomeScreen(const char* message) {
  unsigned long start = millis();
  Serial.println("Displaying WelcomeScreen");
  currentState = STATE_WELCOME;

  String welcomeImagePath = "/default_image/welcome.jpg";
  if (LittleFS.exists(welcomeImagePath)) {
    resizeAndDisplayImage(welcomeImagePath.c_str());
  } else {
    Serial.println("welcome.jpg not found in LittleFS");
    tft.fillScreen(TFT_WHITE);  // Just clear screen if image not found
  }
  
  lastState = STATE_WELCOME;
  Serial.printf("displayWelcomeScreen took %lu ms\n", millis() - start);
}

void displayWelcomeMsg(const char* message) {
  unsigned long start = millis();
  if (currentState != STATE_WELCOME || lastState != STATE_WELCOME) {
    Serial.println("Displaying WelcomeMsg");
    currentState = STATE_WELCOME;
    tft.fillScreen(TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(45, 40);
    tft.setTextSize(3);
    tft.print(message);
    lastState = STATE_WELCOME;
  }
  Serial.printf("displayWelcomeMsg took %lu ms\n", millis() - start);
}


void displayTotalScreen(const char* totalAmount, const char* discount, const char* tax, const char* grandTotal) {
  unsigned long start = millis();
  if (currentState != STATE_TOTAL || lastState != STATE_TOTAL) {
    Serial.println("Displaying TotalScreen");
    currentState = STATE_TOTAL;
    temp();
    tft.setTextSize(6);
    tft.setCursor(40, 50);
    tft.print("DETAILS");
    tft.drawRect(15, 120, 300, 350, TFT_BLUE);
    tft.setTextSize(3);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(20, 130);
    tft.print("Total Amount:");
    tft.setCursor(20, 160);
    tft.print(totalAmount);
    tft.setCursor(20, 210);
    tft.print("Discount:");
    tft.setCursor(20, 240);
    tft.print(discount);
    tft.setCursor(20, 290);
    tft.print("Tax:");
    tft.setCursor(20, 320);
    tft.print(tax);
    tft.setTextColor(TFT_ORANGE);
    tft.setCursor(20, 370);
    tft.print("Grand Total:");
    tft.setCursor(20, 400);
    tft.print(grandTotal);
    lastState = STATE_TOTAL;
  }
  Serial.printf("displayTotalScreen took %lu ms\n", millis() - start);

}

void displayQRCodeScreen(const char* qrData, const char* amount, const char* upi, const char* shopnm) {
  unsigned long start = millis();
  Serial.println("Displaying QRCodeScreen");
  currentState = STATE_QR_CODE;
  temp();

  tft.setTextSize(4);
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(45, 20);
  tft.print("  To Pay  ");
  tft.setTextColor(TFT_ORANGE);

  int x = strlen(amount);
  int amount_x_offset;
  if (x >= 5) {
    amount_x_offset = 78;
  } else if (x == 4) {
    amount_x_offset = 86;
  } else if (x == 3) {
    amount_x_offset = 96;
  } else if (x == 2) {
    amount_x_offset = 108;
  } else {
    amount_x_offset = 116;
  }
  tft.setCursor(amount_x_offset, 65);
  tft.print("Rs.");
  tft.print(amount);

  int qr_ver = 10;
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(qr_ver)];
  qrcode_initText(&qrcode, qrcodeData, qr_ver, 0, (uint8_t *)qrData);

  uint32_t dt = millis() - start;
  Serial.print("QR Code Generation Time: ");
  Serial.print(dt);
  Serial.print("\n");
  Serial.println(qrcode.size);

  if (qr_ver == 6) {
    const unsigned int qr_size = 6;
    for (uint8_t y = 0; y < qrcode.size; y++) {
      for (uint8_t x = 0; x < qrcode.size; x++) {
        if (qrcode_getModule(&qrcode, x, y)) {
          tft.fillRect((x * qr_size) + 38, (y * qr_size) + 120, qr_size, qr_size, BLACK);
        } else {
          tft.fillRect((x * qr_size) + 38, (y * qr_size) + 120, qr_size, qr_size, WHITE);
        }
      }
    }
  } else if (qr_ver == 7) {
    const unsigned int qr_size = 6;
    for (uint8_t y = 0; y < qrcode.size; y++) {
      for (uint8_t x = 0; x < qrcode.size; x++) {
        if (qrcode_getModule(&qrcode, x, y)) {
          tft.fillRect((x * qr_size) + 28, (y * qr_size) + 120, qr_size, qr_size, BLACK);
        } else {
          tft.fillRect((x * qr_size) + 28, (y * qr_size) + 120, qr_size, qr_size, WHITE);
        }
      }
    }
  } else if (qr_ver == 8) {
    const unsigned int qr_size = 5;
    for (uint8_t y = 0; y < qrcode.size; y++) {
      for (uint8_t x = 0; x < qrcode.size; x++) {
        if (qrcode_getModule(&qrcode, x, y)) {
          tft.fillRect((x * qr_size) + 38, (y * qr_size) + 120, qr_size, qr_size, BLACK);
        } else {
          tft.fillRect((x * qr_size) + 38, (y * qr_size) + 120, qr_size, qr_size, WHITE);
        }
      }
    }
  } else if (qr_ver == 9) {
    const unsigned int qr_size = 5;
    for (uint8_t y = 0; y < qrcode.size; y++) {
      for (uint8_t x = 0; x < qrcode.size; x++) {
        if (qrcode_getModule(&qrcode, x, y)) {
          tft.fillRect((x * qr_size) + 30, (y * qr_size) + 120, qr_size, qr_size, BLACK);
        } else {
          tft.fillRect((x * qr_size) + 30, (y * qr_size) + 120, qr_size, qr_size, WHITE);
        }
      }
    }
  } else if (qr_ver == 10) {
    const unsigned int qr_size = 5;
    for (uint8_t y = 0; y < qrcode.size; y++) {
      for (uint8_t x = 0; x < qrcode.size; x++) {
        if (qrcode_getModule(&qrcode, x, y)) {
          tft.fillRect((x * qr_size) + 17, (y * qr_size) + 120, qr_size, qr_size, BLACK);
        } else {
          tft.fillRect((x * qr_size) + 17, (y * qr_size) + 120, qr_size, qr_size, WHITE);
        }
      }
    }
  }

  tft.setCursor(20, 420);
  tft.setTextSize(2);
  tft.setTextColor(TFT_ORANGE);
  tft.print("UPI: ");
  tft.print(shopnm);
  lastState = STATE_QR_CODE;
  Serial.printf("displayQRCodeScreen took %lu ms\n", millis() - start);
}
 
// Updated displayCancelQRCodeScreen (use LittleFS, enable text overlay)
void displayCancelQRCodeScreen(const char* orderId, const char* bankRef, const char* amount, const char* date) {
  unsigned long start = millis();
  Serial.printf("Displaying CancelQRCodeScreen: OrderId=%s, BankRef=%s, Amount=%s, Date=%s\n", orderId, bankRef, amount, date);
  currentState = STATE_STATUS;

  String cancelImagePath = "/default_image/cancel.jpg";
  if (LittleFS.exists(cancelImagePath)) {
    resizeAndDisplayImage(cancelImagePath.c_str());

  } else {
    Serial.println("cancel.jpg not found in LittleFS, skipping display");
  }
  lastState = STATE_STATUS;
  Serial.printf("displayCancelQRCodeScreen took %lu ms\n", millis() - start);
}

// Updated displayFailQRCodeScreen (use LittleFS, enable text overlay)
void displayFailQRCodeScreen(const char* orderId, const char* bankRef, const char* amount, const char* date) {
  unsigned long start = millis();
  Serial.printf("Displaying FailQRCodeScreen: OrderId=%s, BankRef=%s, Amount=%s, Date=%s\n", orderId, bankRef, amount, date);
  currentState = STATE_STATUS;

  String failImagePath = "/default_image/fail.jpg";
  if (LittleFS.exists(failImagePath)) {
    resizeAndDisplayImage(failImagePath.c_str());

  } else {
    Serial.println("fail.jpg not found in LittleFS, skipping display");
  }
  lastState = STATE_STATUS;
  Serial.printf("displayFailQRCodeScreen took %lu ms\n", millis() - start);
}

void displayPendingQRCodeScreen(const char* orderId, const char* bankRef, const char* amount, const char* date) {
  unsigned long start = millis();
  if (currentState != STATE_STATUS || lastState != STATE_STATUS) {
    Serial.println("Displaying PendingQRCodeScreen");
    currentState = STATE_STATUS;
    temp();
    tft.drawBitmap(240, 5, cancel, 80, 80, TFT_WHITE, TFT_ORANGE);
    tft.drawRect(10, 120, 300, 350, TFT_BLUE);
    tft.setTextSize(3);
    tft.setCursor(28, 30);
    tft.print("PENDING");
    tft.setCursor(20, 130);
    tft.print("Order ID:");
    tft.setCursor(20, 160);
    tft.print(orderId);
    tft.setCursor(20, 210);
    tft.print("Bank Reference:");
    tft.setCursor(20, 240);
    tft.print(bankRef);
    tft.setCursor(20, 300);
    tft.print("Amount:");
    tft.setCursor(20, 330);
    tft.print(amount);
    tft.setTextColor(TFT_ORANGE);
    tft.setCursor(20, 380);
    tft.print("Date:");
    tft.setCursor(20, 410);
    tft.print(date);
    lastState = STATE_STATUS;
  }
  Serial.printf("displayPendingQRCodeScreen took %lu ms\n", millis() - start);
}

// Updated displaySuccessQRCodeScreen (use LittleFS, enable text overlay)
void displaySuccessQRCodeScreen(const char* orderId, const char* bankRef, const char* amount, const char* date) {
  unsigned long start = millis();
  Serial.println("Displaying SuccessQRCodeScreen");
  currentState = STATE_STATUS;

  String successImagePath = "/default_image/successful.jpg";
  if (LittleFS.exists(successImagePath)) {
    resizeAndDisplayImage(successImagePath.c_str());

    tft.setTextColor(TFT_BLACK); // Black text to match image style
    tft.setTextSize(2); // Match the font size in the image
    tft.setCursor(30, 240); // Below Bank Ref
    tft.print("Amount: ");
    tft.print(amount); // Assuming amount is in a format like "₹5000"

    tft.setCursor(30, 280); // Below Amount
    tft.print("Bank Ref: ");
    tft.print(bankRef);
  } else {
    Serial.println("successful.jpg not found in LittleFS, skipping display");
  }
  lastState = STATE_STATUS;
  Serial.printf("displaySuccessQRCodeScreen took %lu ms\n", millis() - start);
}

void main_code() {
  if (strcmp("WelcomeScreen", strings[0]) == 0) {
    displayWelcomeScreen(strings[1]);
  } else if (strcmp("Welcome_msg", strings[0]) == 0) {
    displayWelcomeMsg(strings[1]);
  } else if (strcmp("DisplayQRCodeScreen", strings[0]) == 0) {
    displayQRCodeScreen(strings[1], strings[2], strings[3], strings[4]);
  } else if (strcmp("DisplayFailQRCodeScreen", strings[0]) == 0) {
    displayFailQRCodeScreen(strings[1], strings[2], strings[3] ? strings[3] : "0", strings[4] ? strings[4] : "");
  } else if (strcmp("DisplayCancelQRCodeScreen", strings[0]) == 0) {
    displayCancelQRCodeScreen(strings[1], strings[2], strings[3] ? strings[3] : "0", strings[4] ? strings[4] : "");
  } else if (strcmp("DisplayPendingQRCodeScreen", strings[0]) == 0) {
    displayPendingQRCodeScreen(strings[1], strings[2], strings[3] ? strings[3] : "0", strings[4] ? strings[4] : "");
  } else if (strcmp("DisplaySuccessQRCodeScreen", strings[0]) == 0) {
    displaySuccessQRCodeScreen(strings[1], strings[2], strings[3] ? strings[3] : "0", strings[4] ? strings[4] : "");
  } else if (strcmp("DisplayTotalScreen", strings[0]) == 0) {
    displayTotalScreen(strings[1], strings[2], strings[3], strings[4]);
  } else if (strcmp("ResetToWifi", strings[0]) == 0) {
    currentState = STATE_WIFI;
  }
}

void createWebServer() {
  server.on("/", []() {
    IPAddress ip = WiFi.softAPIP();
    String ipStr = ip.toString();
    String content = "<!DOCTYPE HTML>\r\n<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><style>";
    content += "body { font-family: Arial, sans-serif; background-color: #f2f2f2; }";
    content += "h1 { text-align: center; font-size: 1.5em; }";
    content += ".network-box { max-width: 400px; margin: 20px auto; padding: 20px; background-color: #fff; border-radius: 5px; }";
    content += ".network-link { color: #3366cc; cursor: pointer; }";
    content += "form { max-width: 400px; margin: 20px auto; padding: 20px; background-color: #fff; border-radius: 5px; }";
    content += "label { display: block; margin-bottom: 10px; }";
    content += "input { width: 100%; padding: 5px; margin-bottom: 10px; border-radius: 5px; }";
    content += "input[type='submit'] { background-color: #4CAF50; color: white; cursor: pointer; }";
    content += "@media (max-width: 600px) { h1 { font-size: 1.2em; } .network-box, form { width: 90%; } }";
    content += "</style></head><body>";
    content += "<h1>WiFi Credentials Update Page</h1>";
    content += "<form action=\"/scan\" method=\"POST\"><input type=\"button\" value=\"Scan\" onclick=\"location.reload()\"></form>";
    content += "<div class='network-box'>";

    int numNetworks = WiFi.scanNetworks();
    if (numNetworks == 0) {
      content += "<p>No networks found</p>";
    } else {
      for (int i = 0; i < numNetworks; ++i) {
        content += "<span class='network-link' onclick=\"document.getElementsByName('ssid')[0].value = '" + WiFi.SSID(i) + "';\">" + WiFi.SSID(i) + "</span><br>";
      }
    }

    content += "</div>";
    content += "<form name='wifiForm' method='get' action='/setting'>";
    content += "<label>SSID:</label><input name='ssid' type='text'><br>";
    content += "<label>Password:</label><input name='pass' type='password'><br>";
    content += "<label>Static IP (Optional):</label><input name='staticip' type='text'><br>";
    content += "<label>Subnet Mask (Optional):</label><input name='subnet' type='text'><br>";
    content += "<label>Gateway (Optional):</label><input name='gateway' type='text'><br>";
    content += "<input type='submit' value='Submit'></form></body></html>";

    server.send(200, "text/html", content);
  });

  server.on("/setting", []() {

    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String staticip = server.arg("staticip");
    String subnet = server.arg("subnet");
    String gateway = server.arg("gateway");

    if (ssid.length() > 0 && ssid.length() < 32 && pass.length() < 64) {
      preferences.begin("wifi", false);
      preferences.putString("ssid", ssid);
      preferences.putString("pass", pass);
      preferences.putString("staticip", staticip);
      preferences.putString("subnet", subnet);
      preferences.putString("gateway", gateway);

      preferences.end();

      // Attempt to connect to WiFi with the new credentials
      WiFi.begin(ssid.c_str(), pass.c_str());
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        Serial.println("Connected to WiFi with IP: " + ip.toString());
        server.send(200, "application/json", "{\"Success\":\"Credentials Saved\"}");
        delay(1000);
        ESP.restart();
      } else {
        server.send(404, "application/json", "{\"Error\":\"Failed to connect to WiFi\"}");
      }
    } else {
      server.send(404, "application/json", "{\"Error\":\"Invalid Input\"}");
    }
  });

  server.onNotFound([]() {
    server.send(404, "application/json", "{\"Error\":\"404 not found\"}");
  });

  // Start the server explicitly
  server.begin();
  Serial.println("Web server started in AP mode");
}

void setupAP() {
  Serial.println("Setting up AP mode");
  WiFi.mode(WIFI_AP);

  // Stop any existing servers
  server.stop();
  // Set the SSID and password for the soft AP
  const char* ssid = "Bonrix";
  const char* password = "";  // Consider setting a password for security

  // Start the soft AP
  if (WiFi.softAP(ssid, password)) {
    Serial.println("Soft AP started successfully");
    IPAddress ip = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(ip);

    // Show hotspot QR code with instructions
    displayHotspotQRCode(ip.toString().c_str(), ip.toString().c_str());
    // Create and start web server
    createWebServer();
    Serial.println("Web server started on IP: " + ip.toString());
  } else {
    Serial.println("Failed to start Soft AP");
  }
}

void printQRCodeAndDetails(const char* qrText, const char* ipText, const char* ssidText) {
  tft.fillScreen(TFT_WHITE);
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_BLACK);

  tft.setTextColor(0x2602);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.println("Wi-Fi Mode");

  String ipUrl = String("") + ipText + "";
  tft.setTextColor(TFT_BLUE);
  tft.setTextSize(2);
  tft.setCursor(10, 60);
  tft.println("IP:");
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(10, 80);
  tft.println(ipUrl);

  tft.setTextColor(TFT_BLUE);
  tft.setCursor(10, 110);
  tft.println("SSID:");
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(10, 130);
  tft.println(ssidText);

  tft.setTextColor(TFT_BLUE);
  tft.setCursor(10, 170);
  tft.println("MAC:");
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(10, 190);
  tft.println(WiFi.macAddress());

  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, 0, (uint8_t*)ipUrl.c_str());

  int scale = 6;
  int scaledSize = qrcode.size * scale;
  int offsetX = (tft.width() - scaledSize) / 2;
  int offsetY = 230;

  for (int y = 0; y < qrcode.size; y++) {
    for (int x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(offsetX + x * scale, offsetY + y * scale, scale, scale, TFT_BLACK);
      } else {
        tft.fillRect(offsetX + x * scale, offsetY + y * scale, scale, scale, TFT_WHITE);
      }
    }
  }

  tft.setCursor(18, 420);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(2);
  tft.println("Scan QR to configuration page.");

        if (isAudioPlaying) {
            if (mp3 && mp3->isRunning()) {
              mp3->stop();
              delete mp3;
              mp3 = nullptr;
              delete file;
              file = nullptr;
            }
            isAudioPlaying = false;
          }

          String wifiFilePath = String(mp3Directory) + "/wifi.mp3";
          if (playbackEnabled && LittleFS.exists(wifiFilePath)) {
            file = new AudioFileSourceLittleFS(wifiFilePath.c_str());
            mp3 = new AudioGeneratorMP3();
            mp3->begin(file, out);
            isAudioPlaying = true;
            Serial.println("Started playing wifi.mp3");
          } else {
            Serial.print("File does not exist in LittleFS or playback disabled: ");
            Serial.println(wifiFilePath);
            wifiAudioCompleted = true; // If file doesn't exist or playback is disabled, mark as completed
            prepareIPAudioQueue(String(ipText));
            amountFileIndex = 0;
            amountShouldPlayNext = true;
            isAmountAudioPlaying = false;
          }


}
void displayHotspotQRCode(const char* qrText, const char* ipText) {
  tft.fillScreen(TFT_WHITE);
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_BLACK);

  tft.setTextColor(0x2602);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.println("Bonrix Hotspot ");

  String ipUrl = String("http://") + ipText + "/";
  tft.setTextColor(TFT_BLUE);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.println("IP:");
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(10, 70);
  tft.println(ipUrl);

  tft.setTextColor(TFT_BLUE);
  tft.setCursor(10, 110);
  tft.println("SSID:");
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(10, 130);
  tft.println("Bonrix");

  tft.setTextColor(TFT_BLUE);
  tft.setCursor(10, 170);
  tft.println("MAC:");
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(10, 190);
  tft.println(WiFi.softAPmacAddress());

  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, 0, (uint8_t*)ipUrl.c_str());

  int scale = 6;
  int scaledSize = qrcode.size * scale;
  int offsetX = (tft.width() - scaledSize) / 2;
  int offsetY = 210;

  for (int y = 0; y < qrcode.size; y++) {
    for (int x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(offsetX + x * scale, offsetY + y * scale, scale, scale, TFT_BLACK);
      } else {
        tft.fillRect(offsetX + x * scale, offsetY + y * scale, scale, scale, TFT_WHITE);
      }
    }
  }

  tft.setTextColor(TFT_RED);
  tft.setTextSize(2);
  tft.setCursor((tft.width() - tft.textWidth("1. Connect with bonrix.")) / 2, 400);
  tft.println("1. Connect with bonrix.");
  tft.setCursor((tft.width() - tft.textWidth("   WiFi Hotspot ")) / 2, 420);
  tft.println("   WiFi Hotspot ");
  tft.setCursor((tft.width() - tft.textWidth("2. Scan QR to open WiFi")) / 2, 440);
  tft.println("2. Scan QR to open WiFi");
  tft.setCursor((tft.width() - tft.textWidth("   Config Page")) / 2, 460);
  tft.println(" Config Page");
  
}

void displayConnectingScreen(const char* ssid, int attempt) {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(0x2602);
  tft.setTextSize(2);
  tft.setCursor(5, 30);
  tft.println("Connecting to WiFi.......");

  tft.setTextColor(TFT_BLUE);
  tft.setTextSize(2);
  tft.setCursor(10, 80);
  tft.print("SSID: ");
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(10, 110);
  tft.println(ssid);

  tft.setTextColor(TFT_RED);
  tft.setCursor(10, 160);
  tft.print("Attempt: ");
  tft.setTextColor(TFT_BLACK);
  for (int i = 0; i < attempt; i++) {
    tft.print(".");
  }
  tft.println();
}

void configureStaticIP() {
  if (staticIP.length() > 0 && subnetMask.length() > 0 && gateway.length() > 0) {
    IPAddress localIP, localSubnet, localGateway;
    if (localIP.fromString(staticIP) && localSubnet.fromString(subnetMask) && localGateway.fromString(gateway)) {
      WiFi.config(localIP, localGateway, localSubnet);
    } else {
      Serial.println("Failed to configure static IP settings");
    }
  }
}

void clearDisplay() {
  tft.fillScreen(TFT_BLACK);
}

void handleResetWiFi() {
  preferences.begin("wifi", false);
  preferences.remove("ssid");
  preferences.remove("pass");
  preferences.end();

  clearDisplay();
  setupAP();
}

void attemptWiFiConnection() {
  WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
  configureStaticIP();
  Serial.print("Connecting to ");
  Serial.println(esid);
  WiFi.begin(esid.c_str(), epass.c_str());

  int wifi_attempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_attempts < 20) {
    displayConnectingScreen(esid.c_str(), wifi_attempts);
    delay(500);
    Serial.print(".");
    wifi_attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        wifiAudioCompleted = false;
     }else {
        Serial.println("Failed to connect to WiFi");
    }
}

void listFiles() {
  File root = LittleFS.open("/my-python-device-01/");
  File file = root.openNextFile();
  while (file) {
    Serial.print("FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }
}

void reconnect() {
  int maxRetries = 2;
  int retryCount = 0;
  unsigned long delayMs = 1000;
  while (!client.connected() && retryCount < maxRetries) {
    Serial.print("Attempting AWS connection (Attempt ");
    Serial.print(retryCount + 1);
    Serial.print(")...");
    if (client.connect(temp_clientID.c_str())) {
      Serial.println("Connected to AWS IoT Core");
      awsconnected = true;

      client.subscribe(temp_subscribe_topic.c_str());
      displayAWSConnectedImage();
        if (isAudioPlaying) {
            if (mp3 && mp3->isRunning()) {
              mp3->stop();
              delete mp3;
              mp3 = nullptr;
              delete file;
              file = nullptr;
            }
            isAudioPlaying = false;
          }

          String server_connectPath = String(mp3Directory) + "/server_connected.mp3";
          if (playbackEnabled && LittleFS.exists(server_connectPath)) {
            file = new AudioFileSourceLittleFS(server_connectPath.c_str());
            mp3 = new AudioGeneratorMP3();
            mp3->begin(file, out);
            isAudioPlaying = true;
            Serial.println("Started playing server_connected.mp3");
          } else {
            Serial.print("File does not exist in LittleFS or playback disabled: ");
            Serial.println(server_connectPath);
          }
      return;
    } else {
      Serial.print("Failed to connect, rc=");
      Serial.println(client.state());
      retryCount++;
      delay(delayMs);
      delayMs *= 2;
      if (delayMs > 10000) delayMs = 10000;
    }
  }
  if (!client.connected()) {
    Serial.println("Max retries reached, giving up on MQTT connection");
    awsconnected = false;
  }
}

void publishMessage() {
  // Publish message to "devices/health/message"
  uint64_t chipId = ESP.getEfuseMac(); 
  String message = "{\"serial\":\"" + String(chipId) + "\", \"msg\":\"hello Jatin Makwana \"}";
  client.publish(temp_pub_topic.c_str(), message.c_str());
  Serial.print("Published Topic : ");
  Serial.println(temp_pub_topic.c_str());
  Serial.println("Published message: " + message);
}

void AWS_connection_files(){
    listFiles();

  // Set up secure MQTT connection
 // client.setServer(awsEndpoint, awsPort);
  client.setServer(temp_domain.c_str(),temp_port.toInt());
  client.setCallback(callback);
  // Load SSL certificates from LittleFS
  File rootCACertFile = LittleFS.open("/my-python-device-01/AmazonRootCA1.pem", "r");
  File deviceCertFile = LittleFS.open("/my-python-device-01/certificate-1.pem.crt", "r");
  File privateKeyFile = LittleFS.open("/my-python-device-01/private1.pem.key", "r");

  if (!rootCACertFile || !deviceCertFile || !privateKeyFile) {
    Serial.println("Failed to open certificate files from LittleFS");
    return;
  }

  // Read certificate files into Strings
   rootCACert = "";
   deviceCert = "";
   privateKey = "";

  while (rootCACertFile.available()) {
    rootCACert += (char)rootCACertFile.read();
  }
  while (deviceCertFile.available()) {
    deviceCert += (char)deviceCertFile.read();
  }
  while (privateKeyFile.available()) {
    privateKey += (char)privateKeyFile.read();
  }

  // Set certificates for the secure connection
  espClient.setCACert(rootCACert.c_str());
  espClient.setCertificate(deviceCert.c_str());
  espClient.setPrivateKey(privateKey.c_str());

  rootCACertFile.close();
  deviceCertFile.close();
  privateKeyFile.close();
}

void downloadFile(const String& url, const char* destPath) {
  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  if (code == 200) {
    fs::File f = LittleFS.open(destPath, FILE_WRITE);
    if (!f) {
      Serial.printf("❌ Failed to open %s for writing\n", destPath);
    } else {
      WiFiClient* stream = http.getStreamPtr();
      while (http.connected() && stream->available()) {
        f.write(stream->read());
      }
      f.close();
      Serial.printf("✅ Downloaded %s\n", destPath);
    }
  } else {
    Serial.printf("❌ HTTP %d fetching %s\n", code, url.c_str());
  }
  http.end();
}

bool fetchAndStoreAWSConfig() {

  // 1) build URL using your chipId:
  const char* apiUrl = "https://api.dqr-display-tms.bonrix.in/api/Share/AWSIOTCore/serialId/Json/119363428283888";
  
  Serial.println("Fetching AWS config from: ");
  Serial.println(apiUrl);

  WiFiClientSecure secure;
  secure.setInsecure();

      HTTPClient https;
    if (!https.begin(secure, apiUrl)) {
    Serial.println("❌ HTTPS begin failed");
    return false;
  }
  int code = https.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("❌ Failed to fetch AWS config, HTTP code %d\n", code);
    https.end();
    return false;
  }
  
  String payload = https.getString();
  https.end();
  // 2) parse JSON

  DynamicJsonDocument doc(2048);
  auto err = deserializeJson(doc, payload);
  Serial.println(">> RAW AWS JSON:");
  Serial.println(payload);
  if (err) {
    Serial.println("❌ JSON parse failed");
    return false;
  }
  bool ok;
  String stat;
  if (doc.containsKey("isresponse") && doc.containsKey("responsestatus")) {
    ok   = doc["isresponse"].as<bool>();
    stat = doc["responsestatus"].as<String>();
  } else {
    // no wrapper in this API, assume success
    ok   = true;
    stat = "SUCCESS";
  }

  if (!ok || stat != "SUCCESS") {
    Serial.println(F("API are fail please. used TMS website"));
    return false;
  }

  // 3) extract fields
  String deviceId       = doc["deviceid"].as<String>();
  String serialId       = doc["serialid"].as<String>();
  String domain         = doc["domain"].as<String>();
  String port           = doc["port"].as<String>();
  String subscribeTopic = doc["topic"].as<String>();
  String publishTopic   = doc["global_pub"].as<String>();
  String topic          = doc["global_sub"].as<String>();
  String clientId       = doc["client_id"].as<String>();
  String urlCACert      = doc["CACert"].as<String>();
  String urlCert        = doc["Certificate"].as<String>();
  String urlPrivKey     = doc["Private"].as<String>();

  // 4) store to Preferences (overwrite the "AWS" namespace)
  preferences.begin("AWS", false);
    preferences.putString("domain",          domain);
    preferences.putString("port",            port);
    preferences.putString("user",           deviceId);      
    preferences.putString("password",       serialId);      
    preferences.putString("topic",           subscribeTopic);
    preferences.putString("publish_topic",   publishTopic.c_str());
    preferences.putString("global_sub",     topic);
    preferences.putString("clientid",       clientId);
  preferences.end();

    preferences.begin("AWS", true);  // Read-only mode
    temp_domain = preferences.getString("domain", "");
    temp_port = preferences.getString("port", "");
    temp_subscribe_topic = preferences.getString("topic", "");
    temp_pub_topic = preferences.getString("publish_topic", "");
    //temp_user = preferences.getString("user", "");
    //temp_password = preferences.getString("password", "");
    temp_clientID= preferences.getString("clientid", "");
    preferences.end();

  // 5) print out what we’ve saved
  Serial.println("┌──────────────── AWS Config ────────────────");
  Serial.printf("│ DeviceID       : %s\n", deviceId.c_str());
  Serial.printf("│ SerialID       : %s\n", serialId.c_str());
  Serial.printf("│ Domain         : %s\n", temp_domain.c_str());
  Serial.printf("│ Port           : %s\n", temp_port);
  Serial.printf("│ Topic          : %s\n", topic.c_str());
  Serial.printf("│ Pub Topic      : %s\n", temp_pub_topic.c_str());
  Serial.printf("│ Sub Topic        : %s\n", temp_subscribe_topic.c_str());
  Serial.printf("│ ClientID        : %s\n", temp_clientID.c_str());
  Serial.println("└────────────────────────────────────────────");

  // 6) delete previous cert files in /my-python-device-01/
  {
    File dir = LittleFS.open("/my-python-device-01");
    while (File f = dir.openNextFile()) {
      LittleFS.remove(f.name());
      Serial.printf("🗑️  Deleted %s\n", f.name());
    }
  }

  downloadFile(urlCACert,  "/my-python-device-01/AmazonRootCA1.pem");
  downloadFile(urlCert,    "/my-python-device-01/certificate-1.pem.crt");
  downloadFile(urlPrivKey, "/my-python-device-01/private1.pem.key");

  return true;
}

void handleAWSFileUploadAmazon() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/my-python-device-01/AmazonRootCA1.pem";
    
    // Check if the file already exists and delete it
    if (LittleFS.exists(filename)) {
      LittleFS.remove(filename);
      Serial.println("Deleted existing AmazonRootCA1.pem file.");
    }

    // Open the new file for writing
    fs::File file = LittleFS.open(filename, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file for writing: " + filename);
      server.send(500, "text/plain", "Failed to open file for writing.");
      return;
    }
    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    String filename = "/my-python-device-01/AmazonRootCA1.pem";
    fs::File file = LittleFS.open(filename, FILE_APPEND);
    if (file) {
      file.write(upload.buf, upload.currentSize);
      file.close();
      Serial.println("File written successfully: " + filename);
    } else {
      Serial.println("Failed to write to file: " + filename);
      server.send(500, "text/plain", "Failed to write file.");
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    server.send(200, "text/plain", "AmazonRootCA1.pem uploaded successfully.");
    Serial.println("AmazonRootCA1.pem upload complete.");
  }
}

void handleAWSFileUploadPrivet() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/my-python-device-01/private1.pem.key";
    
    // Check if the file already exists and delete it
    if (LittleFS.exists(filename)) {
      LittleFS.remove(filename);
      Serial.println("Deleted existing private1.pem.key file.");
    }

    // Open the new file for writing
    fs::File file = LittleFS.open(filename, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file for writing: " + filename);
      server.send(500, "text/plain", "Failed to open file for writing.");
      return;
    }
    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    String filename = "/my-python-device-01/private1.pem.key";
    fs::File file = LittleFS.open(filename, FILE_APPEND);
    if (file) {
      file.write(upload.buf, upload.currentSize);
      file.close();
      Serial.println("File written successfully: " + filename);
    } else {
      Serial.println("Failed to write to file: " + filename);
      server.send(500, "text/plain", "Failed to write file.");
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    server.send(200, "text/plain", "private1.pem.key uploaded successfully.");
    Serial.println("private1.pem.key upload complete.");
  }
}

void handleAWSFileUploadCirtificate() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/my-python-device-01/certificate-1.pem.crt";
    
    // Check if the file already exists and delete it
    if (LittleFS.exists(filename)) {
      LittleFS.remove(filename);
      Serial.println("Deleted existing certificate-1.pem.crt file.");
    }

    // Open the new file for writing
    fs::File file = LittleFS.open(filename, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file for writing: " + filename);
      server.send(500, "text/plain", "Failed to open file for writing.");
      return;
    }
    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    String filename = "/my-python-device-01/certificate-1.pem.crt";
    fs::File file = LittleFS.open(filename, FILE_APPEND);
    if (file) {
      file.write(upload.buf, upload.currentSize);
      file.close();
      Serial.println("File written successfully: " + filename);
    } else {
      Serial.println("Failed to write to file: " + filename);
      server.send(500, "text/plain", "Failed to write file.");
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    server.send(200, "text/plain", "certificate-1.pem.crt uploaded successfully.");
    Serial.println("certificate-1.pem.crt upload complete.");
  }
}

void displayAWSConnectedImage() {
  String imagePath = "/default_image/serverconnected.jpg";  // Path to the image in LittleFS
  if (LittleFS.exists(imagePath)) {
    Serial.println("Draw server Image in TFT display.");
    resizeAndDisplayImage(imagePath.c_str());
  } else {
    Serial.println("serverconnected not found in LittleFS");
    tft.fillScreen(TFT_WHITE);  // Just clear screen if image not found
  }
}

void printFileNamesAndSizes(const char* directoryPath) {
  // Open the directory
  File dir = LittleFS.open(directoryPath);
  
  // Check if the directory exists and is valid
  if (!dir || !dir.isDirectory()) {
    Serial.println(F("⚠️ Directory not found or invalid"));
    return;
  }

  // Loop through each file in the directory
  File file = dir.openNextFile();
  if (!file) {
    Serial.println(F("⚠️ No files found in the directory"));
    return;
  }

  while (file) {
    // Store the file name and size
    String fileName = String(file.name());
    size_t fileSize = file.size();

    // Check if the file is "AmazonRootCA1.pem" and save its name and size to global variables
    if (fileName.endsWith("AmazonRootCA1.pem")) {
      rootfilename = fileName;  // Save file name to global variable
      rootfilesize = fileSize;  // Save file size to global variable

      // Print the file name and size
      Serial.print(F("File found: "));
      Serial.print(rootfilename);
      Serial.print(F(" - Size: "));
      Serial.print(rootfilesize);
      Serial.println(F(" bytes"));
    }
    else if(fileName.endsWith("private1.pem.key")){
      privetfilename = fileName;  // Save file name to global variable
      privetfilesize = fileSize;  // Save file size to global variable

      Serial.print(F("File found: "));
      Serial.print(privetfilename);
      Serial.print(F(" - Size: "));
      Serial.print(privetfilesize);
      Serial.println(F(" bytes"));
    }
    else if(fileName.endsWith("certificate-1.pem.crt")){
      cirtificatefilename = fileName;  // Save file name to global variable
      cirtificatefilesize = fileSize;  // Save file size to global variable

      Serial.print(F("File found: "));
      Serial.print(cirtificatefilename);
      Serial.print(F(" - Size: "));
      Serial.print(cirtificatefilesize);
      Serial.println(F(" bytes"));
    }
    else{
      Serial.println("Files are does not Exit in my-python-device-01 this directory.");
    }
    file = dir.openNextFile();
  }
}

void prepareAmountAudioQueue(const String &amount) {
    int i = 0;
    amountAudioQueue[i++] = "/mp3/received.mp3";
    amountAudioQueue[i++] = "/mp3/Rs.mp3";

    int amt = amount.toInt();

    // Lakhs
    if (amt >= 100000) {
        int lakhs = amt / 100000;
        if (lakhs >= 20) {
            int tens = (lakhs / 10) * 10;
            int ones = lakhs % 10;
            if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/" + String(tens) + ".mp3";
            if (ones > 0 && i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/" + String(ones) + ".mp3";
        } else {
            if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/" + String(lakhs) + ".mp3";
        }
        if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/lakh.mp3";
        amt %= 100000;
    }

    // Thousands
    if (amt >= 1000) {
        int thousands = amt / 1000;
        if (thousands >= 20) {
            int tens = (thousands / 10) * 10;
            int ones = thousands % 10;
            if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/" + String(tens) + ".mp3";
            if (ones > 0 && i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/" + String(ones) + ".mp3";
        } else {
            if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/" + String(thousands) + ".mp3";
        }
        if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/thousand.mp3";
        amt %= 1000;
    }

    // Hundreds
    if (amt >= 100) {
        int h = amt / 100;
        if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/" + String(h) + ".mp3";
        if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/hundred.mp3";
        amt %= 100;
    }

    // Tens and units
    if (amt > 0) {
        if (amt >= 20) {
            int tens = (amt / 10) * 10;
            int ones = amt % 10;
            if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/" + String(tens) + ".mp3";
            if (ones > 0 && i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/" + String(ones) + ".mp3";
        } else {
            if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/" + String(amt) + ".mp3";
        }
    }

    // Terminate queue
    if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i] = "";
}

void playNextAmountFile() {
    // Clean up previous resources
    if (fileSource) {
        delete fileSource;
        fileSource = nullptr;
    }
    if (mp3) {
        delete mp3;
        mp3 = nullptr;
    }

    String next = amountAudioQueue[amountFileIndex++];
    if (next == "") return;

    Serial.print("Playing amount audio: ");
    Serial.println(next);

    // Check if file exists
    if (!LittleFS.exists(next)) {
        Serial.println("File not found: " + next);
        amountShouldPlayNext = true; // Try next file
        return;
    }

    fileSource = new AudioFileSourceLittleFS(next.c_str());
    out->SetGain(global_saved_volume / 21.0f); // Use saved volume

    mp3 = new AudioGeneratorMP3();
    if (!mp3->begin(fileSource, out)) {
        Serial.print("Failed to play amount file: ");
        Serial.println(next);
        amountShouldPlayNext = true; // Try next file
    } else {
        isAmountAudioPlaying = true;
    }
}

void prepareIPAudioQueue(String ip) {
  ip.trim();
  ip.replace(".", ",.,");  // Replace dots with a comma-dot-comma pattern for splitting
  int i = 0;

  while (ip.length() > 0 && i < MAX_AUDIO_QUEUE) {
    int index = ip.indexOf(',');
    String part = (index == -1) ? ip : ip.substring(0, index);
    ip = (index == -1) ? "" : ip.substring(index + 1);

    part.trim();
    if (part == ".") {
      if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i++] = "/mp3/dot.mp3";
    } else {
      for (char c : part) {
        if (isdigit(c) && i < MAX_AUDIO_QUEUE) { 
          amountAudioQueue[i++] = "/mp3/" + String(c) + ".mp3";
          
        }
      }
    }
  }

  if (i < MAX_AUDIO_QUEUE) amountAudioQueue[i] = "";
}
