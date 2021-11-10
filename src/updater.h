#ifndef UPDATER_H
#define UPDATER_H

#include <Arduino.h>
#include <FS.h>

class FSUpdater
{

private:
    String filename;
    bool requestFlag = false;
    uint8_t status = 255;
    void flash(String filename);
    FS* _fs = nullptr;

public:
    void requestStart(String filename);
    void begin(FS* fs);
    void loop();
    uint8_t getStatus();
};

extern FSUpdater updater;

#endif