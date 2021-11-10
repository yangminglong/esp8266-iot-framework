#include "updater.h"

#include "FS.h"

void FSUpdater::requestStart(String filenameIn)
{
    status = 254;
    filename = filenameIn;
    requestFlag = true;
}

void FSUpdater::begin(FS* fs)
{
    _fs = fs;
}

void FSUpdater::loop()
{
    if (requestFlag==true)
    {
        requestFlag = false;
        flash(filename);
    }
}

uint8_t FSUpdater::getStatus()
{
    return status;
}

void FSUpdater::flash(String filename)
{   
    if (_fs == nullptr)
        return;

    bool answer = 0;
    File file = _fs->open(filename, "r");

    if (!file)
    {
        Serial.println(PSTR("Failed to open file for reading"));
        answer = 0;
    }
    else
    {
        Serial.println(PSTR("Starting update.."));

        size_t fileSize = file.size();

        if (!Update.begin(fileSize))
        {
            Serial.println(PSTR("Not enough space for update"));
        }
        else
        {
            Update.writeStream(file);

            if (Update.end())
            {
                Serial.println(PSTR("Successful update"));
                answer = 1;
            }
            else
            {

                Serial.println(PSTR("Error Occurred: ") + String(Update.getError()));
            }
        }

        file.close();
    }
    
    status = answer;
}

FSUpdater updater;
