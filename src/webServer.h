#ifndef SERVER_H
#define SERVER_H

#include <ESPAsyncWebServer.h>
#include <FS.h>

class webServer
{
private:    
    static void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    static void serveProgmem(AsyncWebServerRequest *request);
    void bindAll();
    static FS* _fs;

public:
    AsyncWebServer server = AsyncWebServer(80);
    AsyncWebSocket ws = AsyncWebSocket("/ws");
    ArRequestHandlerFunction requestHandler = serveProgmem;
    void begin(FS* fs);
};

extern webServer GUI;

#endif
