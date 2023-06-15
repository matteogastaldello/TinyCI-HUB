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

//Extract subnet from a complete ip address eg. getSubnetString("192.168.1.12") returns "192.168.1"
String getSubnetString(String ip, String out)
{  
  // int i;
  // for (i = ip.length() - 1; i > 0 && ip[i] != '.'; i--)
  //   ;

  out = ip.substring(0, ip.lastIndexOf("."));
  Serial.println(out);
  return out;
}