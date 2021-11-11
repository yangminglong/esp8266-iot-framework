#include "webServer.h"
#include "ArduinoJson.h"

// Include the header file we create with webpack
#include "generated/html.h"

//Access to other classes for GUI functions
#include "WiFiManager.h"
#include "configManager.h"
#include "updater.h"
#include "dashboard.h"
#include "FS.h"

FS* webServer::_fs = nullptr;

void webServer::begin(FS* fs)
{
    _fs = fs;
    //to enable testing and debugging of the interface
    DefaultHeaders::Instance().addHeader(PSTR("Access-Control-Allow-Origin"), PSTR("*"));

    server.addHandler(&ws);
    server.begin();

    server.serveStatic("/api/download", *_fs, "/");

    server.onNotFound(requestHandler);

    //handle uploads
    server.on(PSTR("/api/upload"), HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFileUpload);

    bindAll();
}

void webServer::bindAll()
{
    //Restart the ESP
    server.on(PSTR("/api/restart"), HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, PSTR("text/html"), ""); //respond first because of restart
        ESP.restart();
    });

    //update WiFi details
    server.on(PSTR("/api/wifi/set"), HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, PSTR("text/html"), ""); //respond first because of wifi change
        WiFiManager.setNewWifi(request->arg("ssid"), request->arg("pass"));
    });

    //update WiFi details with static IP
    server.on(PSTR("/api/wifi/setStatic"), HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, PSTR("text/html"), ""); //respond first because of wifi change
        WiFiManager.setNewWifi(request->arg("ssid"), request->arg("pass"), request->arg("ip"), request->arg("sub"), request->arg("gw"), request->arg("dns"));
    });

    //update WiFi details
    server.on(PSTR("/api/wifi/forget"), HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, PSTR("text/html"), ""); //respond first because of wifi change
        WiFiManager.forget();
    });

    //get WiFi details
    server.on(PSTR("/api/wifi/get"), HTTP_GET, [](AsyncWebServerRequest *request) {
        String JSON;
        StaticJsonDocument<200> jsonBuffer;

        jsonBuffer["captivePortal"] = WiFiManager.isCaptivePortal();
        jsonBuffer["ssid"] = WiFiManager.SSID();
        serializeJson(jsonBuffer, JSON);

        request->send(200, PSTR("text/html"), JSON);
    });

    //get file listing
    server.on(PSTR("/api/files/get"), HTTP_GET, [_fs](AsyncWebServerRequest *request) {
        String JSON;
        StaticJsonDocument<1000> jsonBuffer;
        JsonArray files = jsonBuffer.createNestedArray("files");

        //get file listing
        Dir dir = _fs->openDir("");
        while (dir.next())
            files.add(dir.fileName());

        //get used and total data 
        FSInfo fs_info;
        _fs->info(fs_info);
        jsonBuffer["used"] = String(fs_info.usedBytes);
        jsonBuffer["max"] = String(fs_info.totalBytes);

        serializeJson(jsonBuffer, JSON);

        request->send(200, PSTR("text/html"), JSON);
    });

    //remove file
    server.on(PSTR("/api/files/remove"), HTTP_POST, [_fs](AsyncWebServerRequest *request) {
        _fs->remove("/" + request->arg("filename"));
        request->send(200, PSTR("text/html"), "");
    });

    //update from LittleFS
    server.on(PSTR("/api/update"), HTTP_POST, [](AsyncWebServerRequest *request) {        
        updater.requestStart("/" + request->arg("filename"));
        request->send(200, PSTR("text/html"), "");
    });

    //update status
    server.on(PSTR("/api/update-status"), HTTP_GET, [](AsyncWebServerRequest *request) {
        String JSON;
        StaticJsonDocument<200> jsonBuffer;

        jsonBuffer["status"] = updater.getStatus();
        serializeJson(jsonBuffer, JSON);

        request->send(200, PSTR("text/html"), JSON);
    });

    //send binary configuration data
    server.on(PSTR("/api/config/get"), HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream(PSTR("application/octet-stream"));
        response->write(reinterpret_cast<char*>(&configManager.data), sizeof(configManager.data));
        request->send(response);
    });

    //receive binary configuration data from body
    server.on(
        PSTR("/api/config/set"), HTTP_POST,
        [this](AsyncWebServerRequest *request) {},
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {},
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            
            static uint8_t buffer[sizeof(configManager.data)];
            static uint32_t bufferIndex = 0;

            for (size_t i = 0; i < len; i++)
            {
                buffer[bufferIndex] = data[i];
                bufferIndex++;
            }

            if (index + len == total)
            {
                bufferIndex = 0;
                configManager.saveRaw(buffer);
                request->send(200, PSTR("text/html"), "");
            }

        });

    //receive binary configuration data from body
    server.on(
        PSTR("/api/dash/set"), HTTP_POST,
        [this](AsyncWebServerRequest *request) {},
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {},
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            memcpy(reinterpret_cast<uint8_t *>(&(dash.data)) + (request->arg("start")).toInt(), data, (request->arg("length")).toInt());
            request->send(200, PSTR("text/html"), "");
        });
}

// Callback for the html
void webServer::serveProgmem(AsyncWebServerRequest *request)
{    
    Serial.print(PSTR("url not found:"));
    Serial.println(request->url());
    Serial.println(PSTR("enter control panel."));
    // Dump the byte array in PROGMEM with a 200 HTTP code (OK)
    AsyncWebServerResponse *response = request->beginResponse_P(200, PSTR("text/html"), html, html_len);

    // Tell the browswer the content is Gzipped
    response->addHeader(PSTR("Content-Encoding"), PSTR("gzip"));
    
    request->send(response);    
}

void webServer::handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    static File fsUploadFile;
    static unsigned long ts;
    static unsigned long fsize;
    
    if (!index)
    {
        Serial.print(PSTR("Start file upload:"));
        Serial.println(filename);

        if (!filename.startsWith("/"))
            filename = "/" + filename;

        ts = millis();
        fsize = 0;
        // fsUploadFile = _fs->open(filename, "w");
    }

    // fsUploadFile.write(data, len);
    fsize += len;
    if (final)
    {
        Serial.print(PSTR("file uploaded:"));
        Serial.println(filename);

        float usedTime =  (millis()-ts)/1000.0;
        float speed = fsize/1024.0 / usedTime;
        Serial.printf(PSTR("time:%.1fs, speed:%.1fkb/s.\n"), usedTime, speed);  
        
        String JSON;
        StaticJsonDocument<100> jsonBuffer;

        // jsonBuffer["success"] = fsUploadFile.isFile();
        jsonBuffer["success"] = true;
        serializeJson(jsonBuffer, JSON);

        request->send(200, PSTR("text/html"), JSON);
        // fsUploadFile.close();   

    }

}

webServer GUI;