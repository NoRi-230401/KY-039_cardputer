#include "N_util.h"
#include <M5StackUpdater.h>
#include <WiFi.h> // Added for WiFi.mode(WIFI_OFF)

M5Canvas canvas(&M5Cardputer.Display);
static SPIClass SPI2;
bool SD_ENABLE;

// ----- Cardputer Specific disp paramaters -----------
// default japanese font
// lgfxJapanGothic_16 and lgfxJapanMincho_16
const int32_t N_COLS = 30; // columns
const int32_t N_ROWS = 8;  // rows

//---- caluculated in m5stackc_begin() ----------------
int32_t W_CHR, H_CHR;      // Character dimensions
int32_t X_WIDTH, Y_HEIGHT; // Screen dimensions
int32_t SC_LINES[N_ROWS];  // Array to store Y coordinates of each line

// #define PLATFORMIO_IDE_DEBUG
void m5stack_begin()
{
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200; // Serial communication speed
  cfg.internal_imu = false;     // Do not use IMU (accelerometer/gyroscope)
  cfg.internal_mic = false;     // Do not use microphone
  // cfg.output_power = false;  // Disable Grove port power output
  cfg.led_brightness = 0;
  M5Cardputer.begin(cfg, true);

#ifdef PLATFORMIO_IDE_DEBUG
  // vsCode terminal cannot get serial data
  //  of cardputer before 5 sec ...!
  delay(5000);
#endif
  dbPrtln("\n\n*** m5stack begin ***");

  M5Cardputer.Speaker.setVolume(0);
  // Disable Wi-Fi and Bluetooth to save power as they are not used.
  WiFi.mode(WIFI_OFF);
  btStop();
  dbPrtln("Wi-Fi and Bluetooth disabled.");

  // Reduce power consumption by lowering CPU frequency (e.g., 80MHz)
  if (setCpuFrequencyMhz(80))
  {
    dbPrtln("CPU Freq set to: " + String(getCpuFrequencyMhz()) + " MHz");
  }


  // Calculate Cardputer specific display scale parameters
  X_WIDTH = M5Cardputer.Display.width();
  Y_HEIGHT = M5Cardputer.Display.height();
  W_CHR = X_WIDTH / N_COLS;  // width of 1 character
  H_CHR = Y_HEIGHT / N_ROWS; // height of 1 character
  for (int i = 0; i < N_ROWS; ++i)
  {
    SC_LINES[i] = i * H_CHR;
  }

  // --- display setup at startup ---------------
  // M5Cardputer.Display.setBrightness(0);
  // ** brightness setup at settingsInit() **

  M5Cardputer.Display.setColorDepth(8);
  M5Cardputer.Display.setRotation(1);
  
  canvas.setColorDepth(8);
  canvas.createSprite(X_WIDTH, Y_HEIGHT);
  canvas.fillScreen(TFT_BLACK);
  canvas.setFont(&fonts::lgfxJapanGothic_16); // default japanese font
  canvas.setTextSize(1);
  canvas.setTextDatum(top_left); // character base position
  canvas.setTextWrap(false);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(0, 0);
  // canvas.pushSprite(0, 0);

  // SPI setup for using SD card
  SPI2.begin(
      M5.getPin(m5::pin_name_t::sd_spi_sclk),
      M5.getPin(m5::pin_name_t::sd_spi_miso),
      M5.getPin(m5::pin_name_t::sd_spi_mosi),
      M5.getPin(m5::pin_name_t::sd_spi_ss));

  SD_ENABLE = SD_begin();
}

// ------------------------------------------------------------------------
// SDU_lobby :  lobby for M5Stack-SD-Updater
// ------------------------------------------------------------------------
// load '/menu.bin' on SD, if key'a' pressed at booting.
// 'menu.bin' for Cardputer is involved in BINS folder at this github site
// ------------------------------------------------------------------------
// ** M5Stack-SD-Updater is launcher software for m5stak by tobozo **
// https://github.com/tobozo/M5Stack-SD-Updater/
// ------------------------------------------------------------------------
void SDU_lobby()
{
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isKeyPressed('a'))
  {
    updateFromFS(SD, "/menu.bin");
    ESP.restart();
    // *** NEVER RETURN ***
  }

  dbPrtln("SDU_lobby exit");
}

bool SD_begin()
{
  const int SD_INIT_RETRIES = 10;
  const int SD_INIT_DELAY_MS = 500;

  for (int i = 0; i < SD_INIT_RETRIES; ++i)
  {
    if (SD.begin(M5.getPin(m5::pin_name_t::sd_spi_ss), SPI2))
    {
      return true; // Success
    }
    delay(SD_INIT_DELAY_MS);
  }

  dbPrtln("ERR: SD begin fail...");
  SD.end();
  return false; // Failed after retries
}

void dbPrtln(String msg)
{
#ifdef DEBUG
  Serial.println(msg);
#endif
}

void dbPrt(String msg)
{
#ifdef DEBUG
  Serial.print(msg);
#endif
}

void dispLx(uint8_t Lx, const char *msg)
{
  //*********** Lx is (0 to N_ROWS - 1) *************************
  if (Lx >= N_ROWS)
    return;

  canvas.fillRect(0, SC_LINES[Lx], X_WIDTH, H_CHR, TFT_BLACK);
  canvas.setCursor(0, SC_LINES[Lx]);
  canvas.print(msg);
}

void POWER_OFF()
{
  dbPrtln(" *** POWER OFF ***");
  delay(5 * 1000L);
  
  // M5.Power.powerOff();
  M5Cardputer.Power.powerOff();

  // *** never return ***
}

const char *NVS_SETTING = "setting";
bool wrtNVS(const char *title, uint8_t data)
{
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_SETTING, NVS_READWRITE, &nvs_handle);
  if (err == ESP_OK)
  {
    nvs_set_u8(nvs_handle, title, data);
    nvs_close(nvs_handle);
    return true;
  }
  else
  {
    dbPrtln("ERR: NVS open for write failed: " + String(esp_err_to_name(err)));
  }
  return false;
}

bool rdNVS(const char *title, uint8_t &data)
{
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_SETTING, NVS_READONLY, &nvs_handle);
  if (err == ESP_OK)
  {
    nvs_get_u8(nvs_handle, title, &data);
    nvs_close(nvs_handle);
    return true;
  }
  else
  {
    dbPrtln("ERR: NVS open for read failed: " + String(esp_err_to_name(err)));
  }
  return false;
}

void loadSetting(const char *nvs_key, uint8_t &setting_variable, uint8_t default_value, uint8_t min_val, uint8_t max_val)
{
  uint8_t nvmData;
  bool needs_write = false;

  if (!rdNVS(nvs_key, nvmData))
  {
    nvmData = default_value;
    needs_write = true;
  }
  else if (nvmData < min_val || nvmData > max_val)
  {
    nvmData = default_value;
    needs_write = true;
  }

  setting_variable = nvmData;
  if (needs_write)
  {
    wrtNVS(nvs_key, setting_variable);
  }
}
