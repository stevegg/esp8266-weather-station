#ifndef __SETTINGS_H
#define __SETTINGS_H

// Settings file

// ILI9341 LCD Settings
//---------------------

#define TFT_DC  4
#define TFT_CS 5

// WIFI Settings
//--------------

char ssid[] = "zzr231";  //  your network SSID (name)
char pass[] = "W3B0ughtTh1sH0us31n2008!";      

// NTP Settings
//-------------

static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = -8;  // Pacific Standard Time (USA)

#endif
