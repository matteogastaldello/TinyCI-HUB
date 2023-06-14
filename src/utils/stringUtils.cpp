#include <Arduino.h>

//convert a byte array to string. Note: 'strPayload' must be length+1 to include \0
void byteToString(byte *payload, unsigned int length, char *strPayload)
{
    for (int i = 0; i < length; i++)
    {
        strPayload[i] = (char)payload[i];
    }
    strPayload[length] = '\0';
}