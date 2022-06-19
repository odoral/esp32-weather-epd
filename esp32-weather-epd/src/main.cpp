// built-in Arduino/C libraries
#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>

// additional libraries
#include <Adafruit_BusIO_Register.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
// fonts (these are modified font files that have the degree symbol mapped '`')
#include "fonts/FreeSans6pt7b.h"
#include "fonts/FreeSans7pt7b.h"
#include "fonts/FreeSans8pt7b.h"
#include "fonts/FreeSans9pt7b.h"
#include "fonts/FreeSans10pt7b.h"
#include "fonts/FreeSans11pt7b.h"
#include "fonts/FreeSans12pt7b.h"
#include "fonts/FreeSans14pt7b.h"
#include "fonts/FreeSans16pt7b.h"
#include "fonts/FreeSans18pt7b.h"
#include "fonts/FreeSans20pt7b.h"
#include "fonts/FreeSans22pt7b.h"
#include "fonts/FreeSans24pt7b.h"
#include "fonts/FreeSans26pt7b.h"
// only has character set used for displaying temperature (0123456789.-`)
#include "fonts/FreeSans48pt_temperature.h"

// header files
#include "config.h"
#include "lang.h"
// icon header files 
#include "icons/icons_16x16.h"
#include "icons/icons_32x32.h"
#include "icons/icons_48x48.h"
#include "icons/icons_64x64.h"
#include "icons/icons_96x96.h"
#include "icons/icons_128x128.h"
#include "icons/icons_160x160.h"
#include "icons/icons_196x196.h"

// GLOBAL VARIABLES
tm timeinfo;
int     wifiSignal;
long    startTime = 0;
String  timeStr, dateStr;

#define DISP_WIDTH  800
#define DISP_HEIGHT 480

enum alignment {LEFT, RIGHT, CENTER};

// Connections for Waveshare e-paper Driver Board
static const uint8_t EPD_BUSY = 26;
static const uint8_t EPD_CS   = 5;
static const uint8_t EPD_RST  = 27; 
static const uint8_t EPD_DC   = 13; 
static const uint8_t EPD_SCK  = 25;
static const uint8_t EPD_MISO = 12; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 2;

GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY)); // B/W display
// GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT> display(GxEPD2_750(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY));     // 3-colour displays

void printLocalTime()
{
  if(!getLocalTime(&timeinfo, 10000)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

uint8_t startWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.printf("Connecting to '%s'", ssid);
  WiFi.begin(ssid, password);
  
  unsigned long timeout = millis() + 10000; // timeout if wifi does not connect in 10s from now
  uint8_t connection_status = WiFi.status();
  
  while ( (connection_status != WL_CONNECTED) && (millis() < timeout) ) {
    Serial.print(".");
    delay(50);
    connection_status = WiFi.status();
  }
  Serial.println();
  
  if (connection_status == WL_CONNECTED) {
    wifiSignal = WiFi.RSSI(); // get Wifi signal strength now, because the WiFi will be turned off to save power!
    Serial.println("IP: " + WiFi.localIP().toString());
  } else {
    Serial.printf("Could not connect to '%s'\n", ssid);
  }
  return connection_status;
}

void killWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

bool setupTime() {
  configTime(0, 0, ntp_server1, ntp_server2); // We will pass 0 for gmtOffset_sec and daylightOffset_sec and use setenv() for timezone offsets
  setenv("TZ", timezone, 1);
  tzset();
  if (!getLocalTime(&timeinfo, 10000)) {
    Serial.println("Failed to obtain time");
    return false;
  }
  Serial.println(&timeinfo, "Time setup complete at: %A, %B %d %Y %H:%M:%S");
  return true;
}

// TODO
bool updateTimeDateStrings() {
  char time_output[30], day_output[30], update_time[30];
  // see http://www.cplusplus.com/reference/ctime/strftime/
  if (units[0] == 'm') {
    if ((!strcmp(lang,"cz")) || !(strcmp(lang,"de")) || !(strcmp(lang,"pl")) || !(strcmp(lang,"nl"))){
      sprintf(day_output, "%s, %02u. %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900); // day_output >> So., 23. Juni 2019 <<
    }
    else
    {
      sprintf(day_output, "%s %02u-%s-%04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    }
    strftime(update_time, sizeof(update_time), "%H:%M:%S", &timeinfo);  // Creates: '14:05:49'
    sprintf(time_output, "%s %s", TXT_UPDATED, update_time);
  }
  else
  {
    strftime(day_output, sizeof(day_output), "%a %b-%d-%Y", &timeinfo); // Creates  'Sat May-31-2019'
    strftime(update_time, sizeof(update_time), "%r", &timeinfo);        // Creates: '02:05:49pm'
    sprintf(time_output, "%s %s", TXT_UPDATED, update_time);
  }
  dateStr = day_output;
  timeStr = time_output;
  return true;
}

void beginSleep() {
  display.powerOff();
  if (!getLocalTime(&timeinfo, 10000)) {
    Serial.println("Failed to obtain time before deep-sleep, using old time.");
  }
  long sleep_timer = (sleep_dur * 60 - ((timeinfo.tm_min % sleep_dur) * 60 + timeinfo.tm_sec));
  esp_sleep_enable_timer_wakeup((sleep_timer + 1) * 1000000LL); // Add 1s extra sleep to allow for fast ESP32 RTCs
  Serial.println("Awake for: " + String((millis() - startTime) / 1000.0, 3) + "s");
  Serial.println("Entering deep-sleep for " + String(sleep_timer) + "(+1)s");
  esp_deep_sleep_start();
}

void drawString(int x, int y, String text, alignment align) {
  int16_t  x1, y1;
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  display.setCursor(x, y);
  display.print(text);
}

void initDisplay() {
  display.init(115200, true, 2, false); // init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode)
  //display.init(); for older Waveshare HAT's
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);

  display.setRotation(0);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}

void updateDisplayBuffer() {
  // location, date
  display.setFont(&FreeSans16pt7b);
  drawString(DISP_WIDTH - 1, 23, "Tucson, Arizona", RIGHT);
  display.setFont(&FreeSans12pt7b);
  drawString(DISP_WIDTH - 1, 30 + 4 + 17, "Saturday, May 29", RIGHT);

  // 1 Alert
  // (fits on 1 line)
  /*
  display.drawInvertedBitmap(196, 8, wi_tornado_48x48, 48, 48, GxEPD_BLACK);
  display.setFont(&FreeSans16pt7b);
  drawString(196 + 48 + 4, 24 + 8 - 12 + 23, "Tornado Watch", LEFT);
  */
  // (fits on 1 line with smaller font)
  /*
  display.drawInvertedBitmap(196, 8, warning_icon_48x48, 48, 48, GxEPD_BLACK);
  display.setFont(&FreeSans12pt7b);
  drawString(196 + 48 + 4, 24 + 8 - 12 + 17, "Severe Thunderstorm Warning", LEFT);
  */
  // (needs 2 lines with smaller font)
  /*
  display.drawInvertedBitmap(196, 8, wi_tornado_48x48, 48, 48, GxEPD_BLACK);
  display.setFont(&FreeSans12pt7b);
  drawString(196 + 48 + 4, 24 + 8 - 22 - 4 - 2 + 17, "Crazy Severe Thunderstorm", LEFT);
  drawString(196 + 48 + 4, 24 + 8 - 2 + 17, " Warning", LEFT);
  */
  // 2 Alerts
  /*
  display.drawInvertedBitmap(196, 0, wi_tornado_32x32, 32, 32, GxEPD_BLACK);
  display.drawInvertedBitmap(196, 32, warning_icon_32x32, 32, 32, GxEPD_BLACK);
  display.setFont(&FreeSans12pt7b);
  drawString(196 + 32 + 3, 5 + 17, "Severe Thunderstorm Warning", LEFT);
  drawString(196 + 32 + 3, 32 + 5 + 17, "Hurricane Force Wind Warning", LEFT);
  */

  // current weather icon
  display.drawInvertedBitmap(0, 0, wi_day_rain_wind_196x196, 196, 196, GxEPD_BLACK);
  // current temp
  display.setFont(&FreeSans48pt_temperature);
  drawString(196 + 164 / 2 - 20, 196 / 2 + 69 / 2, "88", CENTER);
  display.setFont(&FreeSans14pt7b);
  drawString(display.getCursorX(), 196 / 2  - 69 / 2 + 20, "`F", LEFT);
  // current feels like
  display.setFont(&FreeSans12pt7b);
  drawString(196 + 164 / 2, 98 + 69 / 2 + 12 + 17, "Feels Like 86`", CENTER);

  // OLD IDEA TO DISPLAY HIGH AND LOW WITH CURRENT TEMP
  // decided against this to because it is redundent with temperature graph
  // and removing this from above the current temperture helps reduce clutter
  // current weather (with alert)
  // current weather icon
  /*
  display.drawInvertedBitmap(0, 0, wi_day_rain_wind_196x196, 196, 196, GxEPD_BLACK);
  // current temp
  display.setFont(&FreeSans48pt_temperature);
  drawString(196 + 162 / 2 - 20, 98 - 69 / 2 + 30, "199", CENTER);
  display.setFont(&FreeSans14pt7b);
  drawString(display.getCursorX(), 98 - 69 / 2 + 30, "`F", LEFT);
  // today's high | low
  display.setFont(&FreeSans14pt7b);
  drawString(196 + 162 / 2     , 98 - 69 / 2 - 6, "|", CENTER);
  drawString(196 + 162 / 2 - 10, 98 - 69 / 2 + 4, "199`", RIGHT);
  drawString(196 + 162 / 2 + 12, 98 - 69 / 2 + 4, "76`", LEFT);
  // current feels like
  display.setFont(&FreeSans12pt7b);
  drawString(196 + 162 / 2 + 4, 98 + 69 / 2 + 40, "Like 86`", CENTER);
  */


  // 5 day, forecast icons
  display.drawInvertedBitmap(398, 98 + 69 / 2 - 32 - 6, wi_day_fog_64x64, 64, 64, GxEPD_BLACK);
  display.drawInvertedBitmap(480, 98 + 69 / 2 - 32 - 6, wi_day_rain_wind_64x64, 64, 64, GxEPD_BLACK);
  display.drawInvertedBitmap(562, 98 + 69 / 2 - 32 - 6, wi_snow_64x64, 64, 64, GxEPD_BLACK);
  display.drawInvertedBitmap(644, 98 + 69 / 2 - 32 - 6, wi_thunderstorm_64x64, 64, 64, GxEPD_BLACK);
  display.drawInvertedBitmap(726, 98 + 69 / 2 - 32 - 6, wi_windy_64x64, 64, 64, GxEPD_BLACK);
  // 5 day, day of week label
  display.setFont(&FreeSans11pt7b);
  drawString(398 + 32, 98 + 69 / 2 - 32 - 26 - 6 + 16, "Sun", CENTER);
  drawString(480 + 32, 98 + 69 / 2 - 32 - 26 - 6 + 16, "Mon", CENTER);
  drawString(562 + 32, 98 + 69 / 2 - 32 - 26 - 6 + 16, "Tue", CENTER);
  drawString(644 + 32, 98 + 69 / 2 - 32 - 26 - 6 + 16, "Wed", CENTER);
  drawString(726 + 32, 98 + 69 / 2 - 32 - 26 - 6 + 16, "Thur", CENTER);
  // 5 day, high | low
  display.setFont(&FreeSans8pt7b);
  drawString(398 + 32    , 98 + 69 / 2 + 38 - 4 + 12 - 2, "|", CENTER);
  drawString(398 + 32 - 4, 98 + 69 / 2 + 38 - 6 + 12, "199`", RIGHT);
  drawString(398 + 32 + 5, 98 + 69 / 2 + 38 - 6 + 12, "198`", LEFT);
  drawString(480 + 32    , 98 + 69 / 2 + 38 - 4 + 12 - 2, "|", CENTER);
  drawString(480 + 32 - 4, 98 + 69 / 2 + 38 - 6 + 12, "199`", RIGHT);
  drawString(480 + 32 + 5, 98 + 69 / 2 + 38 - 6 + 12, "-22`", LEFT);
  drawString(562 + 32    , 98 + 69 / 2 + 38 - 4 + 12 - 2, "|", CENTER);
  drawString(562 + 32 - 4, 98 + 69 / 2 + 38 - 6 + 12, "99`", RIGHT);
  drawString(562 + 32 + 5, 98 + 69 / 2 + 38 - 6 + 12, "67`", LEFT);
  drawString(644 + 32    , 98 + 69 / 2 + 38 - 4 + 12 - 2, "|", CENTER);
  drawString(644 + 32 - 4, 98 + 69 / 2 + 38 - 6 + 12, "0`", RIGHT);
  drawString(644 + 32 + 5, 98 + 69 / 2 + 38 - 6 + 12, "0`", LEFT);
  drawString(726 + 32    , 98 + 69 / 2 + 38 - 4 + 12 - 2, "|", CENTER);
  drawString(726 + 32 - 4, 98 + 69 / 2 + 38 - 6 + 12, "79`", RIGHT);
  drawString(726 + 32 + 5, 98 + 69 / 2 + 38 - 6 + 12, "199`", LEFT);

  // line dividing top and bottom display areas
  display.drawLine(0, 196, DISP_WIDTH - 1, 196, GxEPD_BLACK);
  
  // current weather data
  int16_t sp = 8;
  display.drawInvertedBitmap(0, 204 + (48 + sp) * 0, wi_sunrise_48x48, 48, 48, GxEPD_BLACK);
  display.drawInvertedBitmap(0, 204 + (48 + sp) * 1, wi_strong_wind_48x48, 48, 48, GxEPD_BLACK);
  display.drawInvertedBitmap(0, 204 + (48 + sp) * 2, wi_day_sunny_48x48, 48, 48, GxEPD_BLACK);
  display.drawInvertedBitmap(0, 204 + (48 + sp) * 3, air_filter_48x48, 48, 48, GxEPD_BLACK);
  display.drawInvertedBitmap(0, 204 + (48 + sp) * 4, house_thermometer_48x48, 48, 48, GxEPD_BLACK);

  display.drawInvertedBitmap(170, 204 + (48 + sp) * 0, wi_sunset_48x48, 48, 48, GxEPD_BLACK);
  display.drawInvertedBitmap(170, 204 + (48 + sp) * 1, wi_humidity_48x48, 48, 48, GxEPD_BLACK);
  display.drawInvertedBitmap(170, 204 + (48 + sp) * 2, wi_barometer_48x48, 48, 48, GxEPD_BLACK);
  display.drawInvertedBitmap(170, 204 + (48 + sp) * 3, visibility_icon_48x48, 48, 48, GxEPD_BLACK);
  display.drawInvertedBitmap(170, 204 + (48 + sp) * 4, house_humidity_48x48, 48, 48, GxEPD_BLACK);

  display.setFont(&FreeSans7pt7b);
  drawString(48, 204 + 10 + (48 + sp) * 0, "Sunrise", LEFT);
  drawString(48, 204 + 10 + (48 + sp) * 1, "Wind", LEFT);
  drawString(48, 204 + 10 + (48 + sp) * 2, "UV Index", LEFT);
  drawString(48, 204 + 10 + (48 + sp) * 3, "Air Quality", LEFT);
  drawString(48, 204 + 10 + (48 + sp) * 4, "Temperature", LEFT);

  drawString(170 + 48, 204 + 10 + (48 + sp) * 0, "Sunset", LEFT);
  drawString(170 + 48, 204 + 10 + (48 + sp) * 1, "Humidity", LEFT);
  drawString(170 + 48, 204 + 10 + (48 + sp) * 2, "Pressure", LEFT);
  drawString(170 + 48, 204 + 10 + (48 + sp) * 3, "Visibiliy", LEFT);
  drawString(170 + 48, 204 + 10 + (48 + sp) * 4, "Humidity", LEFT);

  display.setFont(&FreeSans12pt7b);
  drawString(48, 204 + 17 + (48 + sp) * 0 - 17 / 2 + 48 / 2, "6:00", LEFT);
  drawString(48, 204 + 17 + (48 + sp) * 1 - 17 / 2 + 48 / 2, "18mph", LEFT);
  drawString(48, 204 + 17 + (48 + sp) * 2 - 17 / 2 + 48 / 2, "10 - High", LEFT);
  drawString(48, 204 + 17 + (48 + sp) * 3 - 17 / 2 + 48 / 2, "Good", LEFT);
  drawString(48, 204 + 17 + (48 + sp) * 4 - 17 / 2 + 48 / 2, "78`", LEFT);

  drawString(170 + 48, 204 + 17 + (48 + sp) * 0 - 17 / 2 + 48 / 2, "18:00", LEFT);
  drawString(170 + 48, 204 + 17 + (48 + sp) * 1 - 17 / 2 + 48 / 2, "12%", LEFT);
  drawString(170 + 48, 204 + 17 + (48 + sp) * 2 - 17 / 2 + 48 / 2, "29.65in", LEFT);
  drawString(170 + 48, 204 + 17 + (48 + sp) * 3 - 17 / 2 + 48 / 2, "4000ft", LEFT);
  drawString(170 + 48, 204 + 17 + (48 + sp) * 4 - 17 / 2 + 48 / 2, "20%", LEFT);

  

  
  // debug
  int16_t  x1, y1;
  uint16_t w, h;
  char str[20];

  // Total font height
  display.setFont(&FreeSans6pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "6 h: %d", h);
  drawString(500,215, str, LEFT);

  display.setFont(&FreeSans7pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "7 h: %d", h);
  drawString(500,230, str, LEFT);

  display.setFont(&FreeSans8pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "8 h: %d", h);
  drawString(500,245, str, LEFT);

  display.setFont(&FreeSans9pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "9 h: %d", h);
  drawString(500,260, str, LEFT);

  display.setFont(&FreeSans10pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "10 h: %d", h);
  drawString(500,275, str, LEFT);

  display.setFont(&FreeSans11pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "11 h: %d", h);
  drawString(500,290, str, LEFT);

  display.setFont(&FreeSans12pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "12 h: %d", h);
  drawString(500,305, str, LEFT);

  display.setFont(&FreeSans14pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "14 h: %d", h);
  drawString(500,320, str, LEFT);

  display.setFont(&FreeSans16pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "16 h: %d", h);
  drawString(500,335, str, LEFT);

  display.setFont(&FreeSans18pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "18 h: %d", h);
  drawString(500,350, str, LEFT);

  display.setFont(&FreeSans20pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "20 h: %d", h);
  drawString(500,365, str, LEFT);

  display.setFont(&FreeSans22pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "22 h: %d", h);
  drawString(500,380, str, LEFT);

  display.setFont(&FreeSans24pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "24 h: %d", h);
  drawString(500,395, str, LEFT);

  display.setFont(&FreeSans26pt7b);
  display.getTextBounds("TpXygQq", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "26 h: %d", h);
  drawString(500,410, str, LEFT);

  display.setFont(&FreeSans48pt_temperature);
  display.getTextBounds("1234567890.-`", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "48temp h: %d", h);
  drawString(500,425, str, LEFT);

  // Upper font height
    display.setFont(&FreeSans6pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "6 h: %d", h);
  drawString(580,215, str, LEFT);

  display.setFont(&FreeSans7pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "7 h: %d", h);
  drawString(580,230, str, LEFT);

  display.setFont(&FreeSans8pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "8 h: %d", h);
  drawString(580,245, str, LEFT);

  display.setFont(&FreeSans9pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "9 h: %d", h);
  drawString(580,260, str, LEFT);

  display.setFont(&FreeSans10pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "10 h: %d", h);
  drawString(580,275, str, LEFT);

  display.setFont(&FreeSans11pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "11 h: %d", h);
  drawString(580,290, str, LEFT);

  display.setFont(&FreeSans12pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "12 h: %d", h);
  drawString(580,305, str, LEFT);

  display.setFont(&FreeSans14pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "14 h: %d", h);
  drawString(580,320, str, LEFT);

  display.setFont(&FreeSans16pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "16 h: %d", h);
  drawString(580,335, str, LEFT);

  display.setFont(&FreeSans18pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "18 h: %d", h);
  drawString(580,350, str, LEFT);

  display.setFont(&FreeSans20pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "20 h: %d", h);
  drawString(580,365, str, LEFT);

  display.setFont(&FreeSans22pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "22 h: %d", h);
  drawString(580,380, str, LEFT);

  display.setFont(&FreeSans24pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "24 h: %d", h);
  drawString(580,395, str, LEFT);

  display.setFont(&FreeSans26pt7b);
  display.getTextBounds("TMNH", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "26 h: %d", h);
  drawString(580,410, str, LEFT);

  display.setFont(&FreeSans48pt_temperature);
  display.getTextBounds("1234567890.-`", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "48temp h: %d", h);
  drawString(580,425, str, LEFT);


  display.setFont(&FreeSans14pt7b);
  display.getTextBounds("`F`", 0, 0, &x1, &y1, &w, &h);
  display.setFont(&FreeSans8pt7b);
  sprintf(str, "w: %d h: %d", w, h);
  drawString(500,440, str, LEFT);
  // end debug
  
  

  
}

/*
// debug performance testing
bool isPrime(int N)
{
    for(int i = 2 ; i * i <= N ; i++)
        if(N % i == 0)
            return false;
    return true;
}
int countPrimes(int N)
{
    if(N < 3)
        return 0;
    int cnt = 1;
    for(int i = 3 ; i < N ; i += 2)
        if(isPrime(i))
            cnt++;
    return cnt;
}
*/

void setup() {
  startTime = millis();
  Serial.begin(115200);
  // Serial.println("Primes: " + String(countPrimes(1000000)));
  // esp_sleep_enable_timer_wakeup((5) * 1000000LL);
  // Serial.println("Awake for: " + String((millis() - startTime) / 1000.0, 3) + "s");
  // esp_deep_sleep_start();

  // TODO will firebeetle led stay off when on battery? otheriwse desolder...
//#ifdef LED_BUILTIN
//  pinMode(LED_BUILTIN, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
//  digitalWrite(LED_BUILTIN, HIGH);
//#endif

  if ( (startWiFi() == WL_CONNECTED) && setupTime() ) {
    initDisplay();

    updateTimeDateStrings();
    killWiFi();
    /*
    if ((CurrentHour >= WakeupTime && CurrentHour <= SleepTime) || DebugDisplayUpdate) {
      //InitialiseDisplay(); // Give screen time to initialise by getting weather data!
      uint8_t Attempts = 1;
      bool RxWeather = false, RxForecast = false;
      WiFiClient client;   // wifi client object
      while ((RxWeather == false || RxForecast == false) && Attempts <= 2) { // Try up-to 2 time for Weather and Forecast data
        if (RxWeather  == false) RxWeather  = obtain_wx_data(client, "weather");
        if (RxForecast == false) RxForecast = obtain_wx_data(client, "forecast");
        Attempts++;
      }
      if (RxWeather && RxForecast) { // Only if received both Weather or Forecast proceed
        StopWiFi(); // Reduces power consumption
        DisplayWeather();
        display.display(false); // Full screen update mode
      }
    }
    */
    updateDisplayBuffer();
    display.display(false); // Full display refresh
  }
  beginSleep();
}

void loop() { // this will never run
}




