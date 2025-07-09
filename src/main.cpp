// --------------------------------------------------------
//  *** KY-039_cardputer ***     by NoRi
//  KY=039 Heart beat Senseor software for Cardputer
//    2025-07-08  v104
// https://github.com/NoRi-230401/KY-039_cardputer
//  MIT License
// --------------------------------------------------------
#include "N_util.h"

enum class KeyNum
{
  None,
  Up,
  Down,
  Left,
  Right,
  SettingEscape,
  SettingBrightness,
  SettingLowBat,
  SettingLang,
  SettingDisp
};

enum class SettingMode
{
  Esc,
  Brightness,
  LowBatThreshold,
  Lang
};
static SettingMode settingMode = SettingMode::Esc;

enum class DispMode
{
  Bpm,
  Plot
};
static DispMode dispMode = DispMode::Plot;

namespace AppConfig
{
  // Brightness settings
  constexpr uint8_t BRIGHT_LVL_INIT = 30;
  constexpr uint8_t BRIGHT_LVL_MAX = 255;
  constexpr uint8_t BRIGHT_LVL_MIN = 0;

  // Battery settings
  constexpr uint8_t BATLVL_MAX = 100;
  constexpr uint8_t LOWBAT_THRESHOLD_INIT = 10;
  constexpr uint8_t LOWBAT_THRESHOLD_MAX = 95;
  constexpr uint8_t LOWBAT_THRESHOLD_MIN = 5;

  // Language settings
  constexpr uint8_t LANG_INIT = 0; // 0:English 1:Japanese
  constexpr uint8_t LANG_MAX = 1;

  // Battery status check
  namespace Battery
  {
    constexpr uint8_t BATLVL_FLUCTUATION_TOLERANCE = 5;
    constexpr unsigned long BATTERY_CHECK_INTERVAL_MS = 1993UL; // Interval for battery level check
    constexpr uint8_t LOWBAT_CONSECUTIVE_READINGS = 5;
  }

  // Display layout positions (in character grid)
  namespace Layout
  {
    constexpr int BATLVL_ITEM_POS = 22;
    constexpr int BATLVL_ITEM_LEN = 4;
    constexpr int BATLVL_VALUE_POS = 26;
    constexpr int BATLVL_VALUE_LEN = 3;
    constexpr int BATLVL_PERCENT_POS = 29;
    constexpr int SETTING_DISP_POS = 2;
    constexpr int MEAS_UNIT_POS = 28;
    constexpr int MEAS_ITEM_POS = 2;
    constexpr int MEAS_ITEM_FONT_SIZE = 24;
  }

  // Heart beat calculation settings
  namespace HeartBeat
  {
    constexpr float VALID_WAVE_MIN = 2400.0;
    constexpr float VALID_WAVE_MAX = 2900.0;
    constexpr unsigned long VALID_BEAT_PERIOD_MIN_MS = 500;
    constexpr unsigned long VALID_BEAT_PERIOD_MAX_MS = 2000;
    constexpr float VALID_BPM_MIN = 30.0;
    constexpr float VALID_BPM_MAX = 120.0;
    constexpr float BPM_STABILITY_THRESHOLD = 10.0;
    constexpr float BPM_WEIGHT_CURRENT = 0.4f;
    constexpr float BPM_WEIGHT_PREV01 = 0.3f;
    constexpr float BPM_WEIGHT_PREV02 = 0.3f;
  }

  // Plotting settings
  namespace Plot
  {
    constexpr float AUTO_SCALE_FACTOR = 0.3;
  }
}

// --- Key mapping constants ---

const char *BATLVL_TITLE[] = {"bat.", "電池"};
static uint8_t BRIGHT_LVL;       // 0 - 255 : LCD bright level
static uint8_t LOWBAT_THRESHOLD; // 5 - 95% : LOW BATTERY Threshold level
const char *NVM_BRIGHT = "brt";
const char *NVM_LOWBAT = "lbat";
const char *NVM_LANG = "lang";
const char *LANG[] = {"English", "日本語"};
static uint8_t LANG_INDEX = 0;
const char *meas_items[] = {"Pulse Rate", "脈拍"};

void setup();
void loop();
void ky039Init();
void plot_do();
void ky039Sensor();
void calcBeat(float newData);
void dispPlotModeInit();
void graphFrame();
;
void prtBPM(float temp_val, DispMode currentDispMode);
void dispInit(DispMode mode);
void dispBpmModeInit();
KeyNum keyCheck();
void handleKeyPress(KeyNum key);
void changeLang(KeyNum keyNo);
bool updateLang(KeyNum keyNo);
void dispBatItem();
void dispMeasItem();
bool updateSettingValue(uint8_t &value, KeyNum keyNo, uint8_t min, uint8_t max, uint8_t step, uint8_t big_step);
void prtSetting(const char *msg, uint8_t data);
void prtSetting(const char *msg, const char *data);
void changeBright(KeyNum keyNo);
void changeLowBatThr(KeyNum keyNo);
void settingsInit();
void batteryState();
void prtBatLvl(uint8_t batLvl);
void lowBatteryCheck(uint8_t batLvl);

void setup()
{
  m5stack_begin();

  if (SD_ENABLE)
  { // M5stack-SD-Updater lobby
    SDU_lobby();
    SD.end();
  }

  settingsInit();
  ky039Init();
  dispInit(DispMode::Plot);
  canvas.pushSprite(0, 0);
}

void loop()
{
  ky039Sensor();
  batteryState();

  KeyNum key = keyCheck();
  if (key != KeyNum::None)
  {
    handleKeyPress(key);
  }

  if (dispMode == DispMode::Plot)
    plot_do();

  // vTaskDelay(1);
}

constexpr uint8_t sensorPin = 1;
void ky039Init()
{
  // GPIO 1 : AD 12bit 0 to 4095 value (for 0 to 3.3volts)
  pinMode(sensorPin, ANALOG);
}

constexpr unsigned long AV_PERIOD_MS = 20; // average period
static unsigned long PREV_AVDATA_TM = 0;
static uint64_t SENSOR_READ_SUM = 0;
static uint64_t N_READ = 0;

constexpr int WAVE_SIZ = 240;
static float WAVE_DATA[WAVE_SIZ] = {0};
uint16_t WAVE_POS = 0;
constexpr int plotX_siz = 240;
constexpr int plotY_siz = 100;

static int X_MOVE = 0;
unsigned long PREV_PLOT_TM = 0;
float PREV_PLOT_MIN = 4096.0;
float PREV_PLOT_MAX = -1.0;

void plot_do()
{ // **** plot wave data of heart beating ****
  //   :  IPS LCD 240x135 px            (y/x)
  //  0/0------------------------------ 0/239
  //   |   BPM                    batlvl |
  //  34                               34/239
  //  35 ------------------------------35/239
  //   |           plot area             |
  //  134/0 --------------- --- ------134/239
  //
  unsigned long current_tm = millis();
  constexpr unsigned long PLOT_PERIOD_MS = 33; // 30 fps  1000mSec/30= 33.3mSec

  if (current_tm - PREV_PLOT_TM < PLOT_PERIOD_MS)
    return;

  uint16_t cPlotPos = WAVE_POS;
  PREV_PLOT_TM = current_tm;
  const int y0_pos = Y_HEIGHT - 1;             // origin y-axis disp position   -> 134
  const int ymax_pos = y0_pos - plotY_siz + 1; // max y-axis disp position      ->  35

  canvas.fillRect(0, ymax_pos - 1, plotX_siz, plotY_siz + 1, TFT_BLACK); // clear plot area
  graphFrame();                                                          // draw frame

  // search min/max value
  float max_val = -1.0;
  float min_val = 4096.0;
  for (int i = 0; i < WAVE_SIZ; i++)
  {
    if (WAVE_DATA[i] > max_val)
      max_val = WAVE_DATA[i];
    if (WAVE_DATA[i] < min_val)
      min_val = WAVE_DATA[i];
  }

  // auto scale calculation
  float prev_range = PREV_PLOT_MAX - PREV_PLOT_MIN;
  if (max_val > (PREV_PLOT_MAX + prev_range * AppConfig::Plot::AUTO_SCALE_FACTOR) || max_val < (PREV_PLOT_MAX - prev_range * AppConfig::Plot::AUTO_SCALE_FACTOR))
    PREV_PLOT_MAX = max_val;
  else
    max_val = PREV_PLOT_MAX;

  if (min_val < (PREV_PLOT_MIN - prev_range * AppConfig::Plot::AUTO_SCALE_FACTOR) || min_val > (PREV_PLOT_MIN + prev_range * AppConfig::Plot::AUTO_SCALE_FACTOR))
    PREV_PLOT_MIN = min_val;
  else
    min_val = PREV_PLOT_MIN;

  float range = max_val - min_val;
  float top_pos = max_val + range * 0.2;
  float bottom_pos = min_val - range * 0.2;
  range = top_pos - bottom_pos;
  float multiplier = plotY_siz / range;

  for (int i = 0; i < WAVE_SIZ; i++)
  {
    // *** wave data is converted to real LCD disp area  (y: 134 to 35) ***
    float y_float = (float)y0_pos - (WAVE_DATA[(cPlotPos + i) % WAVE_SIZ] - bottom_pos) * multiplier;
    int32_t y = (int32_t)(y_float); // round down
    if ((y >= ymax_pos) && (y <= y0_pos))
    {
      canvas.drawPixel(i, y, TFT_WHITE);
    }
  }
  canvas.pushSprite(0, 0);
}

void ky039Sensor()
{
  // calculate an average of the  sensor
  // during a 20 ms period (this will eliminate
  // the 50  Hz noise caused by electric light
  unsigned long current_read_tm = millis();
  SENSOR_READ_SUM += analogRead(sensorPin); // read and add values...
  N_READ++;

  if (current_read_tm - PREV_AVDATA_TM < AV_PERIOD_MS)
    return;

  float avData = (float)SENSOR_READ_SUM / N_READ; // and take an average of the values
  PREV_AVDATA_TM = current_read_tm;
  SENSOR_READ_SUM = 0;
  N_READ = 0;
  calcBeat(avData);
}

// -- moving averaging sampling
// constexpr int samp_siz = 4;
constexpr int samp_siz = 5;
static int samp_pos = 0; // current position in the array
static float SAMPS[samp_siz] = {0};
// ---------------------------------------------
static float PREV_WAVE = 4096.0; // impossible value
static bool isRISING = true;
static int RISE_CNT = 0;
static unsigned long PREV_BEAT01 = 0, PREV_BEAT02 = 0;
static unsigned long PREV_BEAT_TM = 0;
static float PREV_BPMVAL01 = 60.0; // defalut 60bpm
static float PREV_BPMVAL02 = 60.0; // default 60bpm

void calcBeat(float newData)
{
  constexpr uint8_t rise_threshold = 4;

  // Add the  newest measurement to an array
  // and subtract the oldest measurement from  the array
  // to maintain a sum of last measurements
  SAMPS[samp_pos++] = newData;
  samp_pos %= samp_siz;

  // current_wave : current average of the values in the array
  float sum_val = 0;
  for (int i = 0; i < samp_siz; i++)
    sum_val += SAMPS[i];
  float current_wave = sum_val / samp_siz;

  // *** plot data at LCD display ***
  // Serial.printf(">current_curve:%f\n", current_curve);
  WAVE_DATA[WAVE_POS++] = current_wave;
  WAVE_POS %= WAVE_SIZ;

  // check  for a rising curve (= a heart beat)
  if (current_wave > PREV_WAVE)
  {
    RISE_CNT++;
    if (!isRISING && RISE_CNT > rise_threshold)
    {
      //  Ok, we have detected a rising curve, which implies a heartbeat.
      //  Record the time since last beat, keep track of the two previous
      //  times (first, second, third) to get a weighed average.
      // The rising  flag prevents us from detecting the same rise more than once.
      unsigned long current_beat = millis() - PREV_BEAT_TM;
      PREV_BEAT_TM = millis();
      isRISING = true;

      // Calculate the weighed average of heartbeat rate
      // according  to the three last beats
      // bpm : beats per minute
      float current_bpmVal = 60000.0f / (AppConfig::HeartBeat::BPM_WEIGHT_CURRENT * current_beat + AppConfig::HeartBeat::BPM_WEIGHT_PREV01 * PREV_BEAT01 + AppConfig::HeartBeat::BPM_WEIGHT_PREV02 * PREV_BEAT02);

      // *** SELECT VALID DATA ***
      if (current_wave < AppConfig::HeartBeat::VALID_WAVE_MIN || current_wave > AppConfig::HeartBeat::VALID_WAVE_MAX) // AD value
      {
        // Not the desired data
        dbPrtln("not desired data = " + String(current_wave));
        prtBPM(-1.0, ::dispMode); // not desired data
        canvas.pushSprite(0, 0);
      }
      else if (current_beat < AppConfig::HeartBeat::VALID_BEAT_PERIOD_MIN_MS || current_beat > AppConfig::HeartBeat::VALID_BEAT_PERIOD_MAX_MS) // msec
      {
        //  500msec period -> 2Hz   -> 120BPM .... invalid data
        // 2000msec period -> 0.5Hz ->  30BPM .... invalid data
        dbPrtln("invalid beat = " + String(current_beat));
      }
      else if (current_bpmVal < AppConfig::HeartBeat::VALID_BPM_MIN || current_bpmVal > AppConfig::HeartBeat::VALID_BPM_MAX) // bpm
      {
        // invalid heart beat bpm .... reject
        dbPrtln("invalid bpm value = " + String(current_bpmVal));
      }
      else if (abs(current_bpmVal - PREV_BPMVAL01) > AppConfig::HeartBeat::BPM_STABILITY_THRESHOLD || abs(current_bpmVal - PREV_BPMVAL02) > AppConfig::HeartBeat::BPM_STABILITY_THRESHOLD) // bpm
      {
        // not stable
        dbPrtln(" not stable bpm value = " + String(current_bpmVal));
      }
      else
      {
        prtBPM(current_bpmVal, ::dispMode);
        canvas.pushSprite(0, 0);
      }
      PREV_BPMVAL02 = PREV_BPMVAL01;
      PREV_BPMVAL01 = current_bpmVal;

      PREV_BEAT02 = PREV_BEAT01;
      PREV_BEAT01 = current_beat;
    }
  }
  else
  {
    //  Ok, the curve is falling
    isRISING = false;
    RISE_CNT = 0;
  }
  PREV_WAVE = current_wave;
}

void dispPlotModeInit()
{
  canvas.fillScreen(TFT_BLACK); // all clear

  // BPM meas value '---.-'
  prtBPM(-1.0, DispMode::Plot);

  // meas unit -----
  canvas.setFont(&fonts::Font4);
  canvas.setTextSize(0.8);
  canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  canvas.drawString(F("bpm"), X_WIDTH / 2 - 15, 0);

  // L0 :Battery Level -----
  canvas.setFont(&fonts::lgfxJapanGothic_16);
  canvas.setTextSize(1);
  dispBatItem();
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawString(F("---"), W_CHR * AppConfig::Layout::BATLVL_VALUE_POS, SC_LINES[0]);
  canvas.drawString(F("%"), W_CHR * AppConfig::Layout::BATLVL_PERCENT_POS, SC_LINES[0]);
}

void graphFrame()
{
  int x_max = X_WIDTH - 1;
  int y_max = Y_HEIGHT - 1;

  // upper limit frame
  canvas.drawLine(0, y_max - plotY_siz - 1, x_max, y_max - plotY_siz - 1, TFT_DARKGREY);

  //   1-2-3-4 sec time frame
  canvas.drawLine(x_max - 50, Y_HEIGHT - plotY_siz, x_max - 50, y_max, TFT_DARKCYAN);
  canvas.drawLine(x_max - 2 * 50, Y_HEIGHT - plotY_siz, x_max - 2 * 50, y_max, TFT_DARKCYAN);
  canvas.drawLine(x_max - 3 * 50, Y_HEIGHT - plotY_siz, x_max - 3 * 50, y_max, TFT_DARKCYAN);
  canvas.drawLine(x_max - 4 * 50, Y_HEIGHT - plotY_siz, x_max - 4 * 50, y_max, TFT_DARKCYAN);
}

constexpr int BPM_FONT_SIZE = 48;
constexpr int BPM_LINE_INDEX = 3;
constexpr int BPM_DISP_WIDTH = 27;
static float PREV_BPM_DISP = 0.0;
void prtBPM(float temp_val, DispMode currentDispMode)
{
  // Skip redrawing if the value hasn't changed.
  // This handles both number-to-number and NAN-to-NAN comparisons.
  // if (PREV_BPM_DISP == temp_val || (isnan(PREV_BPM_DISP) && isnan(temp_val)))
  // {
  //   return;
  // }
  // PREV_BPM_DISP = temp_val;

  char buf[10];
  if (isnan(temp_val) || temp_val < 0)
  {
    snprintf(buf, sizeof(buf), "---.-");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%3.1f", temp_val);
  }

  switch (currentDispMode)
  {
  case DispMode::Bpm:
    canvas.fillRect(0, SC_LINES[BPM_LINE_INDEX], X_WIDTH, BPM_FONT_SIZE, TFT_BLACK); // clear
    canvas.setFont(&fonts::Font7);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.drawRightString(buf, X_WIDTH / 2 + 64, SC_LINES[BPM_LINE_INDEX]);
    break;

  case DispMode::Plot:
    if (settingMode != SettingMode::Esc)
      return;

    canvas.fillRect(0, 0, X_WIDTH / 2 - 24, 34, TFT_BLACK); // clear
    canvas.setFont(&fonts::Font7);
    canvas.setTextSize(0.60);
    canvas.setTextColor(TFT_SKYBLUE, TFT_BLACK);
    canvas.drawRightString(buf, X_WIDTH / 2 - 25, 0);
    break;

  default:
    return;
  }
}

// ************************************************************************************

void dispInit(DispMode mode)
{
  switch (mode)
  {
  case DispMode::Bpm:
    dispBpmModeInit();
    break;
  case DispMode::Plot:
    dispPlotModeInit();
    break;
  default:
    return;
  }
}

void dispBpmModeInit()
{
  // ---012345678901234567890123456789----
  // L0:- HC-SR04 Sensor -    bat.---%
  // L1: (settings display line)
  // L2:
  // L3:
  // L4:
  // L5:
  // L6:
  // L7:  Pusle Rate         bpm
  // ---012345678901234567890123456789----

  canvas.fillScreen(TFT_BLACK); // all clear
  canvas.setFont(&fonts::lgfxJapanGothic_16);
  canvas.setTextSize(1);

  //--L0 : title--------------
  canvas.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  canvas.drawString(F("- KY-039 Heart Beat -"), 0, SC_LINES[0]);

  // L0 :Battery Level -----
  dispBatItem();
  canvas.drawString(F("---"), W_CHR * AppConfig::Layout::BATLVL_VALUE_POS, SC_LINES[0]);
  canvas.drawString(F("%"), W_CHR * AppConfig::Layout::BATLVL_PERCENT_POS, SC_LINES[0]);

  // L7 : Measuremnt items
  canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  canvas.setFont(&fonts::Font4);
  canvas.setTextSize(1);
  canvas.drawRightString(F("bpm"), W_CHR * AppConfig::Layout::MEAS_UNIT_POS, SC_LINES[7]);
  dispMeasItem();

  // meas value
  prtBPM(-1.0, DispMode::Bpm);
}

KeyNum keyCheck()
{
  M5Cardputer.update(); // update Cardputer key input
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())
  {
    // isKeyPressed()は、指定されたキーが現在押されているかをチェックします。
    // この方法は、一度に一つのキーしか押されないことを前提としています。
    if (M5Cardputer.Keyboard.isKeyPressed('`'))
    {
      return KeyNum::SettingEscape;
    }
    else if (M5Cardputer.Keyboard.isKeyPressed('1'))
    {
      return KeyNum::SettingBrightness;
    }
    else if (M5Cardputer.Keyboard.isKeyPressed('2'))
    {
      return KeyNum::SettingLowBat;
    }
    else if (M5Cardputer.Keyboard.isKeyPressed('3'))
    {
      return KeyNum::SettingLang;
    }
    else if (M5Cardputer.Keyboard.isKeyPressed('0'))
    {
      return KeyNum::SettingDisp;
    }
    else if (M5Cardputer.Keyboard.isKeyPressed(';'))
    {
      return KeyNum::Up;
    }
    else if (M5Cardputer.Keyboard.isKeyPressed('.'))
    {
      return KeyNum::Down;
    }
    else if (M5Cardputer.Keyboard.isKeyPressed(','))
    {
      return KeyNum::Left;
    }
    else if (M5Cardputer.Keyboard.isKeyPressed('/'))
    {
      return KeyNum::Right;
    }
  }
  return KeyNum::None;
}

void handleKeyPress(KeyNum key)
{
  bool modeChanged = false;

  switch (key)
  {
  case KeyNum::SettingEscape:
    if (settingMode != SettingMode::Esc)
    {
      settingMode = SettingMode::Esc;
      modeChanged = true;
    }
    break;
  case KeyNum::SettingBrightness:
    if (settingMode != SettingMode::Brightness)
    {
      settingMode = SettingMode::Brightness;
      modeChanged = true;
    }
    break;
  case KeyNum::SettingLowBat:
    if (settingMode != SettingMode::LowBatThreshold)
    {
      settingMode = SettingMode::LowBatThreshold;
      modeChanged = true;
    }
    break;
  case KeyNum::SettingLang:
    if (settingMode != SettingMode::Lang)
    {
      settingMode = SettingMode::Lang;
      modeChanged = true;
    }
    break;
  case KeyNum::SettingDisp:
  {
    // Toggle display mode between Bpm and Plot
    int nextModeIndex = (static_cast<int>(dispMode) + 1) % 2;
    dispMode = static_cast<DispMode>(nextModeIndex);
    dispInit(dispMode);
    canvas.pushSprite(0, 0);
    return;
  }
  case KeyNum::Up:
  case KeyNum::Down:
  case KeyNum::Left:
  case KeyNum::Right:
    switch (settingMode)
    {
    case SettingMode::Brightness:
      changeBright(key);
      break;
    case SettingMode::LowBatThreshold:
      changeLowBatThr(key);
      break;
    case SettingMode::Lang:
      changeLang(key);
      break;
    default:
      break;
    }
    break;

  default:
    return; // Not a relevant key
  }

  if (modeChanged)
  {
    switch (settingMode)
    {
    case SettingMode::Esc:
      canvas.fillRect(0, SC_LINES[1], X_WIDTH, H_CHR, TFT_BLACK);
      break;
    case SettingMode::Brightness:
      changeBright(KeyNum::None); // Display initial setting
      break;
    case SettingMode::LowBatThreshold:
      changeLowBatThr(KeyNum::None); // Display initial setting
      break;
    case SettingMode::Lang:
      changeLang(KeyNum::None); // Display initial setting
      break;
    }
    canvas.pushSprite(0, 0);
  }
}

void changeLang(KeyNum keyNo)
{
  if (updateLang(keyNo))
  {
    wrtNVS(NVM_LANG, LANG_INDEX);
    dispMeasItem();
    dispBatItem();
  }
  prtSetting("lang = ", LANG[LANG_INDEX]);
}

bool updateLang(KeyNum keyNo)
{
  switch (keyNo)
  {
  case KeyNum::Up:
  case KeyNum::Down:
  case KeyNum::Right:
  case KeyNum::Left:
    LANG_INDEX = (LANG_INDEX + 1) % (AppConfig::LANG_MAX + 1);
    return true; // Value changed
  default:
    break;
  }
  return false; // No change
}

void dispBatItem()
{
  canvas.fillRect(W_CHR * AppConfig::Layout::BATLVL_ITEM_POS, SC_LINES[0], W_CHR * AppConfig::Layout::BATLVL_ITEM_LEN, H_CHR, TFT_BLACK);
  canvas.setFont(&fonts::lgfxJapanMincho_16);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.drawString(BATLVL_TITLE[LANG_INDEX], W_CHR * AppConfig::Layout::BATLVL_ITEM_POS, SC_LINES[0]);
}

void dispMeasItem()
{
  canvas.setFont(&fonts::lgfxJapanGothic_24);
  canvas.setTextSize(1);
  int width = max(canvas.textWidth(meas_items[0]), canvas.textWidth(meas_items[1]));

  // clear
  canvas.fillRect(0, SC_LINES[7], W_CHR * AppConfig::Layout::MEAS_ITEM_POS + width, AppConfig::Layout::MEAS_ITEM_FONT_SIZE, TFT_BLACK);

  // measuremt items
  canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  canvas.drawString(meas_items[LANG_INDEX], W_CHR * AppConfig::Layout::MEAS_ITEM_POS, SC_LINES[7]);
}

bool updateSettingValue(uint8_t &value, KeyNum keyNo, uint8_t min, uint8_t max, uint8_t step, uint8_t big_step)
{
  int tempValue = value; // Use int to prevent overflow during calculation
  uint8_t original_value = value;

  switch (keyNo)
  {
  case KeyNum::Up:
    tempValue += big_step;
    break;
  case KeyNum::Down:
    tempValue -= big_step;
    break;
  case KeyNum::Right:
    tempValue += step;
    break;
  case KeyNum::Left:
    tempValue -= step;
    break;
  default:
    return false; // Not a value-changing key
  }

  // Clamp the value to the allowed range
  value = constrain(tempValue, min, max);

  return value != original_value; // Return true if value changed
}

void prtSetting(const char *msg, uint8_t data)
{
  char datBuf[4]; // message buffer
  snprintf(datBuf, sizeof(datBuf), "%3u", data);
  prtSetting(msg, datBuf);
}

void prtSetting(const char *msg, const char *data)
{
  // Line1 : setting display
  char msgBuf[31]; // message buffer
  snprintf(msgBuf, sizeof(msgBuf), "%s%s", msg, data);
  dbPrtln(msgBuf);

  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setFont(&fonts::lgfxJapanGothic_12);
  canvas.setTextSize(1);
  canvas.fillRect(0, SC_LINES[1], X_WIDTH, H_CHR, TFT_BLACK); // clear L1
  canvas.drawString(msgBuf, W_CHR * AppConfig::Layout::SETTING_DISP_POS, SC_LINES[1]);
  canvas.pushSprite(0, 0);
}

void changeBright(KeyNum keyNo)
{
  const uint8_t step_short = 1;
  const uint8_t step_big = 10;
  if (updateSettingValue(BRIGHT_LVL, keyNo, AppConfig::BRIGHT_LVL_MIN, AppConfig::BRIGHT_LVL_MAX, step_short, step_big))
  {
    M5Cardputer.Display.setBrightness(BRIGHT_LVL);
    wrtNVS(NVM_BRIGHT, BRIGHT_LVL);
  }
  prtSetting("bright = ", BRIGHT_LVL);
}

void changeLowBatThr(KeyNum keyNo)
{
  const uint8_t step_short = 1;
  const uint8_t step_big = 10;
  if (updateSettingValue(LOWBAT_THRESHOLD, keyNo, AppConfig::LOWBAT_THRESHOLD_MIN, AppConfig::LOWBAT_THRESHOLD_MAX, step_short, step_big))
  {
    wrtNVS(NVM_LOWBAT, LOWBAT_THRESHOLD);
  }
  prtSetting("lowBattery threshold = ", LOWBAT_THRESHOLD);
}

void settingsInit()
{
  loadSetting(NVM_BRIGHT, BRIGHT_LVL, AppConfig::BRIGHT_LVL_INIT, AppConfig::BRIGHT_LVL_MIN, AppConfig::BRIGHT_LVL_MAX);
  M5Cardputer.Display.setBrightness(BRIGHT_LVL);
  loadSetting(NVM_LOWBAT, LOWBAT_THRESHOLD, AppConfig::LOWBAT_THRESHOLD_INIT, AppConfig::LOWBAT_THRESHOLD_MIN, AppConfig::LOWBAT_THRESHOLD_MAX);
  loadSetting(NVM_LANG, LANG_INDEX, AppConfig::LANG_INIT, 0, AppConfig::LANG_MAX);
}

static unsigned long PREV_BATCHK_TM = 0L;
static uint8_t PREV_BATLVL = 255; // Use an impossible value to force the first update
static bool batCheck_first = true;
void batteryState()
{
  unsigned long currentTime = millis(); // Get current time once

  if (currentTime - PREV_BATCHK_TM < AppConfig::Battery::BATTERY_CHECK_INTERVAL_MS)
    return;

  // This will update consecutiveLowBatteryCount
  PREV_BATCHK_TM = currentTime;
  uint8_t batLvl = (uint8_t)M5Cardputer.Power.getBatteryLevel(); // Get battery level
  // dbPrtln("batLvl: " + String(batLvl));
  if (batLvl > AppConfig::BATLVL_MAX)
    batLvl = AppConfig::BATLVL_MAX;

  lowBatteryCheck(batLvl);

  if (batCheck_first)
  {
    batCheck_first = false;
  }
  else
  { // ** stable battery level is valid **
    if (abs(batLvl - PREV_BATLVL) > AppConfig::Battery::BATLVL_FLUCTUATION_TOLERANCE)
    {
      PREV_BATLVL = batLvl;
      return;
    }
  }

  PREV_BATLVL = batLvl;
  prtBatLvl(batLvl);
}

static uint8_t PREV_BATLVL_DISP = 255; // Use an impossible value to force the first update
void prtBatLvl(uint8_t batLvl)
{
  // Line0 : battery level display
  //---- 012345678901234567890123456789---
  // L0_"                      bat.xxx%"--

  if (batLvl == PREV_BATLVL_DISP)
    return;
  PREV_BATLVL_DISP = batLvl;

  char msg[4] = ""; // message buffer
  snprintf(msg, sizeof(msg), "%3u", batLvl);
  // dbPrtln(msg);

  canvas.fillRect(W_CHR * AppConfig::Layout::BATLVL_VALUE_POS, SC_LINES[0], W_CHR * AppConfig::Layout::BATLVL_VALUE_LEN, H_CHR, TFT_BLACK); // clear
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setFont(&fonts::lgfxJapanMincho_16);
  canvas.setTextSize(1);
  canvas.drawString(msg, W_CHR * AppConfig::Layout::BATLVL_VALUE_POS, SC_LINES[0]);
  canvas.pushSprite(0, 0);
}

static uint8_t consecutiveLowBatteryCount = 0;
void lowBatteryCheck(uint8_t batLvl)
{
  // Update consecutive low battery count
  if (batLvl < LOWBAT_THRESHOLD)
  {
    if (consecutiveLowBatteryCount < AppConfig::Battery::LOWBAT_CONSECUTIVE_READINGS)
    { // Avoid overflow if already at max
      consecutiveLowBatteryCount++;
    }
  }
  else
  {
    consecutiveLowBatteryCount = 0; // Reset if battery level is acceptable
    return;
  }

  if (consecutiveLowBatteryCount >= AppConfig::Battery::LOWBAT_CONSECUTIVE_READINGS)
  {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_RED, TFT_BLACK);
    canvas.drawCenterString(F("Low Battery !!"), X_WIDTH / 2, SC_LINES[3], &fonts::Font4);
    canvas.pushSprite(0, 0);
    POWER_OFF();
    // *** NEVER RETURN ***
  }
}
