#include <ESP8266WiFi.h>
#include <SPI.h>
#include <TFT_ILI93XX.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <WundergroundClient.h>
#include "images/partly_cloudy.c"
#include "ntp.h"

// Download helper
#include "WebResource.h"

// ILI9341 LCD Settings
//---------------------

#define TFT_DC  4
#define TFT_CS 5

// WIFI Settings
//--------------

char ssid[] = "zzr231";  //  your network SSID (name)
char pass[] = "W3B0ughtTh1sH0us31n2008!";    

// WUNDERGROUND Settings
//----------------------

const String WUNDERGRROUND_API_KEY = "065314d504077deb";
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_COUNTRY = "Canada";
const String WUNDERGROUND_CITY = "Qualicum_Beach";

// LCD Display Configuration
TFT_ILI93XX tft = TFT_ILI93XX(TFT_CS, TFT_DC  );

WiFiUDP Udp;
WundergroundClient wunderground = WundergroundClient(true);
WebResource webResource;

// Prototypes
void displayTime();
void displayDate();
void displayWeather();
void displayForecast();
void displayAstronomy();
String getWeatherIcon(String iconText);
char *buildTimeString(boolean includeSeconds);
void downloadResources();
void drawBmp(String filename, uint16_t x, uint16_t y);

#define null NULL
#define INTERVAL 60 * 15

char logLine[255];
char logTime[10];
void log( char *message, char *level ) {
  char *currentTime = buildTimeString(true);

  snprintf(logLine, 255, "%s - %s : %s", currentTime, level, message );
  Serial.println(logLine);
  free(currentTime);
}

void logDebug( char *message) {
  log(message, "DEBUG");  
}

void logWarning( char *message ) {
  log(message, "WARN");
}

void logError( char *message ) {
  log(message, "ERROR");
}


void setup() {

  tft.begin(false);
  delay(30);
  tft.clearScreen();
  tft.setCursor(0, 0);
  tft.println("Steve's Weather Station V1.1");
  tft.println("Setting up wifi connection...");
  
  Serial.begin(38400);
  Serial.println(F("Steve's Weather Station V1.1"));
  Serial.println(F("Setting up wifi connection"));
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  tft.print("IP number assigned by DHCP is ");
  tft.println(WiFi.localIP());
  
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(8888);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  tft.clearScreen();
  

  SPIFFS.begin();
  downloadResources();
}

time_t lastUpdate = 0L;

void loop() {

  logDebug("Loop start.");

  displayDate();
  displayTime();

  time_t currentTime = now();

  long difference = currentTime - lastUpdate;
  char debugMessage[128];
  snprintf(debugMessage, 128, "Elapsed since last update: %d.  Interval : %d", difference, INTERVAL);
  logDebug(debugMessage);
  if ( lastUpdate == 0L || ( currentTime - lastUpdate ) > INTERVAL ) {
    logDebug("Updating weather conditions");
    lastUpdate = currentTime;
    wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
    wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
    wunderground.updateAstronomy(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
    displayWeather();
    displayForecast();
    displayAstronomy();
    logDebug("Weather conditions updated.");
  }


  delay(30000);
}

char dateString[30] = "";
void displayDate() {

  char months[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  char days[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

  if ( dateString ) {
    tft.setTextColor(BLACK);
    tft.setCursor(CENTER, 0, SCREEN);
    tft.setTextScale(1);
    tft.println(dateString);
  }

  sprintf(dateString, "%s %s %d, %d", days[weekday()-1], months[month()-1], day(), year());
  
  Serial.print("Date : " );
  Serial.println(dateString);
  tft.setCursor(CENTER, 0, SCREEN);
  tft.setTextColor(WHITE);
  tft.setTextScale(1);

  tft.println(dateString);

}

char timeString[10];
#define TIME_VERTICAL_POS 25
void displayTime() {

  if ( timeString ) {
    tft.setTextColor(BLACK);
    tft.setCursor(CENTER, TIME_VERTICAL_POS, SCREEN);
    tft.setTextScale(2);
    tft.println(timeString);
  }

  char *currentTime = buildTimeString(false);
  Serial.print("Value from buildTimeString: ");
  Serial.println(currentTime);
  strcpy(timeString, currentTime);
  free(currentTime);


  tft.setTextColor(WHITE);
  tft.setCursor(CENTER, TIME_VERTICAL_POS, SCREEN);
  tft.setTextScale(2);
  tft.println(timeString);
}

char *buildTimeString(boolean includeSeconds) {
  char hourString[3];
  char minuteString[3];
  char secondString[3];
  char *builtTimeString = (char *)malloc(10);
    
  int currentHour = hour();
  int currentMinute = minute();
  int currentSecond = second();

  sprintf(hourString, (currentHour<10)?"0%d":"%d", currentHour);
  sprintf(minuteString, (currentMinute<10)?"0%d":"%d", currentMinute);
  sprintf(secondString, (currentSecond<10)?"0%d":"%d", currentSecond);
  if ( includeSeconds ) {
    snprintf(builtTimeString, 10, "%s:%s:%s", hourString, minuteString, secondString);
  } else {
    snprintf(builtTimeString, 10, "%s:%s", hourString, minuteString);
  }

  return builtTimeString;
}

#define WEATHER_VERTICAL_POS 70
void displayWeather() {

  tft.fillRect(0, WEATHER_VERTICAL_POS, tft.width(), WEATHER_VERTICAL_POS+64, BLACK);
  int center = 74 + (int)((float)(tft.width()-74) / 2.0f);

  String weatherIcon = getWeatherIcon(wunderground.getTodayIcon());
  drawBmp(weatherIcon + ".bmp", 0, 55);
  /*
  const tPicture *weatherIcon = getWeatherIcon(wunderground.getTodayIcon());

  // Draw current weather image

  if ( weatherIcon != null ) {
    tft.drawImage(10, WEATHER_VERTICAL_POS, weatherIcon);
  }
  */
  // Draw current temperature
  tft.setCursor(center, WEATHER_VERTICAL_POS+5, REL_X);
  tft.setTextScale(1);
  tft.println(wunderground.getWeatherText());
  tft.setCursor(center, WEATHER_VERTICAL_POS+25, REL_X);
  tft.setTextScale(2);
  tft.setTextColor(CYAN);
  tft.print(wunderground.getCurrentTemp());
  tft.println("c");
}

// if you want separators, uncomment the tft-line
void drawSeparator(uint16_t y) {
   //tft.drawFastHLine(10, y, 240 - 2 * 10, 0x4228);
}

// helper for the forecast columns
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {

  tft.setTextColor(CYAN, BLACK);
  tft.setTextScale(1);
  //tft.setFont(&ArialRoundedMTBold_14);
  //ui.setTextAlignment(CENTER);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  tft.setCursor(x + 25, y, REL_X);
  tft.println(day);

  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(x + 25, y + 20, REL_X);
  tft.println(wunderground.getForecastLowTemp(dayIndex) + "|" + wunderground.getForecastHighTemp(dayIndex));
  
  String weatherIcon = getWeatherIcon(wunderground.getForecastIcon(dayIndex));
  drawBmp("/mini/" + weatherIcon + ".bmp", x, y + 35);
    
}

#define FORECAST_VERTICAL_POSITION 160
// draws the three forecast columns
void displayForecast() {
  tft.fillRect(0, FORECAST_VERTICAL_POSITION, tft.width(), FORECAST_VERTICAL_POSITION + 65 + 10, BLACK);
  drawForecastDetail(10, FORECAST_VERTICAL_POSITION, 2);
  drawForecastDetail(95, FORECAST_VERTICAL_POSITION, 4);
  drawForecastDetail(180, FORECAST_VERTICAL_POSITION, 6);
}


#define ASTRONOMY_VERTICAL_POSITION 250

// draw moonphase and sunrise/set and moonrise/set
void displayAstronomy() {
  tft.fillRect(0, ASTRONOMY_VERTICAL_POSITION, tft.width(), tft.height(), BLACK);
  int moonAgeImage = 24 * wunderground.getMoonAge().toInt() / 30.0;
  drawBmp("/moon" + String(moonAgeImage) + ".bmp", 120 - 30, ASTRONOMY_VERTICAL_POSITION);
  tft.setTextColor(WHITE, BLACK);
  //tft.setFont(&ArialRoundedMTBold_14);  
  //ui.setTextAlignment(LEFT);
  tft.setTextScale(1);
  tft.setTextColor(CYAN, BLACK);
  tft.setCursor(20, ASTRONOMY_VERTICAL_POSITION);
  tft.println("Sun");
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(20, ASTRONOMY_VERTICAL_POSITION + 22);
  tft.println(wunderground.getSunriseTime());
  tft.setCursor(20, ASTRONOMY_VERTICAL_POSITION + 45);
  tft.println(wunderground.getSunsetTime());
  
  tft.setTextColor(CYAN, BLACK);
  tft.setCursor(190, ASTRONOMY_VERTICAL_POSITION);
  tft.println("Moon");
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(190, ASTRONOMY_VERTICAL_POSITION + 22);
  tft.println(wunderground.getMoonriseTime());
  tft.setCursor(190, ASTRONOMY_VERTICAL_POSITION + 45);
  tft.println(wunderground.getMoonsetTime());
  
}
// Helper function, should be part of the weather station library and should disappear soon
String getWeatherIcon(String iconText) {
  if (iconText == "F") return "chanceflurries";
  if (iconText == "Q") return "chancerain";
  if (iconText == "W") return "chancesleet";
  if (iconText == "V") return "chancesnow";
  if (iconText == "S") return "chancetstorms";
  if (iconText == "B") return "clear";
  if (iconText == "Y") return "cloudy";
  if (iconText == "F") return "flurries";
  if (iconText == "M") return "fog";
  if (iconText == "E") return "hazy";
  if (iconText == "Y") return "mostlycloudy";
  if (iconText == "H") return "mostlysunny";
  if (iconText == "H") return "partlycloudy";
  if (iconText == "J") return "partlysunny";
  if (iconText == "W") return "sleet";
  if (iconText == "R") return "rain";
  if (iconText == "W") return "snow";
  if (iconText == "B") return "sunny";
  if (iconText == "0") return "tstorms";
  

  return "unknown";
}

void drawString(int x, int y, char *text) {
  int16_t x1, y1;
  uint16_t w, h;
  x1 = x - w / 2;
  tft.fillRect(x1, y - h -1, w + 1, h + 1, BLACK);
  tft.setTextColor(CYAN, BLACK);
  tft.setCursor(x1, y, SCREEN);
  tft.print(text);
}

void drawString(int x, int y, String text) {
  char buf[text.length()+2];
  text.toCharArray(buf, text.length() + 1);
  drawString(x, y, buf);
}

void drawProgressBar(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint8_t percentage, uint16_t frameColor, uint16_t barColor) {
  if (percentage == 0) {
    tft.fillRect(x0, y0, w, h, BLACK);
  }
  uint8_t margin = 2;
  uint16_t barHeight = h - 2 * margin;
  uint16_t barWidth = w - 2 * margin;
  tft.drawRect(x0, y0, w, h, CYAN);
  tft.fillRect(x0 + margin, y0 + margin, barWidth * percentage / 100.0, barHeight, barColor);
}

// callback called during download of files. Updates progress bar
void downloadCallback(String filename, int16_t bytesDownloaded, int16_t bytesTotal) {
  Serial.println(String(bytesDownloaded) + " / " + String(bytesTotal));

  int percentage = 100 * bytesDownloaded / bytesTotal;
  if (percentage == 0) {
    drawString(120, 160, filename);
  }
  if (percentage % 5 == 0) {
    //ui.drawString(120, 160, String(percentage) + "%");
    drawProgressBar(10, 165, 240 - 20, 15, percentage, WHITE, BLUE);
  }

}

String wundergroundIcons [] = {"chanceflurries","chancerain","chancesleet","chancesnow","clear","cloudy","flurries","fog","hazy","mostlycloudy","mostlysunny","partlycloudy","partlysunny","rain","sleet","snow","sunny","tstorms","unknown"};

ProgressCallback _downloadCallback = downloadCallback;

void downloadResources() {
  tft.fillScreen(BLACK);
  //tft.setFont(&ArialRoundedMTBold_14);
  char id[5];
  for (int i = 0; i < 21; i++) {
    sprintf(id, "%02d", i);
    tft.fillRect(0, 120, 240, 40, BLACK);
    webResource.downloadFile("http://www.squix.org/blog/wunderground/" + wundergroundIcons[i] + ".bmp", wundergroundIcons[i] + ".bmp", _downloadCallback);
  }
  for (int i = 0; i < 21; i++) {
    sprintf(id, "%02d", i);
    tft.fillRect(0, 120, 240, 40, BLACK);
    webResource.downloadFile("http://www.squix.org/blog/wunderground/mini/" + wundergroundIcons[i] + ".bmp", "/mini/" + wundergroundIcons[i] + ".bmp", _downloadCallback);
  }
  for (int i = 0; i < 23; i++) {
    tft.fillRect(0, 120, 240, 40, BLACK);
    webResource.downloadFile("http://www.squix.org/blog/moonphase_L" + String(i) + ".bmp", "/moon" + String(i) + ".bmp", _downloadCallback);
  }
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

#define BUFFPIXEL 20

void drawBmp(String filename, uint16_t x, uint16_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col, x2, y2, bx1, by1;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SPIFFS.open(filename, "r")) == NULL) {
    Serial.print(F("File not found"));
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        x2 = x + bmpWidth  - 1; // Lower-right corner
        y2 = y + bmpHeight - 1;
        if((x2 >= 0) && (y2 >= 0)) { // On screen?
          w = bmpWidth; // Width/height of section to load/display
          h = bmpHeight;
          bx1 = by1 = 0; // UL coordinate in BMP file
          if(x < 0) { // Clip left
            bx1 = -x;
            x   = 0;
            w   = x2 + 1;
          }
          if(y < 0) { // Clip top
            by1 = -y;
            y   = 0;
            h   = y2 + 1;
          }
          if(x2 >= tft.width())  w = tft.width()  - x; // Clip right
          if(y2 >= tft.height()) h = tft.height() - y; // Clip bottom

          // Set TFT address window to clipped image bounds
          //_tft->startWrite(); // Requires start/end transaction now
          //_tft->setAddrWindow(x, y, w, h);

          for (row=0; row<h; row++) { // For each scanline...
  
            // Seek to start of scan line.  It might seem labor-
            // intensive to be doing this on every line, but this
            // method covers a lot of gritty details like cropping
            // and scanline padding.  Also, the seek only takes
            // place if the file position actually needs to change
            // (avoids a lot of cluster math in SD library).
            if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
              pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
            else     // Bitmap is stored top-to-bottom
              pos = bmpImageoffset + row * rowSize;
            if(bmpFile.position() != pos) { // Need seek?
              //_tft->endWrite(); // End TFT transaction
              bmpFile.seek(pos, SeekSet);
              buffidx = sizeof(sdbuffer); // Force buffer reload
              //_tft->startWrite(); // Start new TFT transaction
            }
  
            for (col=0; col<w; col++) { // For each pixel...
              // Time to read more pixel data?
              if (buffidx >= sizeof(sdbuffer)) { // Indeed
                //_tft->endWrite(); // End TFT transaction
                bmpFile.read(sdbuffer, sizeof(sdbuffer));
                buffidx = 0; // Set index to beginning
                //_tft->startWrite(); // Start new TFT transaction
              }
  
              // Convert pixel from BMP to TFT format, push to display
              b = sdbuffer[buffidx++];
              g = sdbuffer[buffidx++];
              r = sdbuffer[buffidx++];
              tft.drawPixel(x + col, y + row, tft.Color565(r,g,b));
             yield();
            } // end pixel
          } // end scanline
          Serial.print(F("Loaded in "));
          Serial.print(millis() - startTime);
          Serial.println(" ms");
        } // end onscreen
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
}
