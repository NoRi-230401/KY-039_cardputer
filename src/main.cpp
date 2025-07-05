// --------------------------------------------------------
//  *** KY-039_cardputer ***     by NoRi
//  KY=039 Heart beat Senseor software for Cardputer
//    2025-07-03  v101
// https://github.com/NoRi-230401/KY-039_cardputer
//  MIT License
// --------------------------------------------------------
#include "N_util.h"
enum KeyNum
{
  KN_NONE,
  KN_UP,
  KN_DOWN,
  KN_LEFT,
  KN_RIGHT
};

enum SettingMode
{
  SM_ESC,
  SM_BRIGHT_LEVEL,
  SM_LOWBAT_THRESHOLD,
  SM_LANG
};
static SettingMode settingMode = SM_ESC;

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

  // Sensor and timing settings
  // namespace Sensor
  // {
  //   constexpr unsigned long KY039_CHECK_INTERVAL_MS = 20UL; // 20mSEC interval
  // }

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
    // constexpr int MEAS_UNIT_POS = 23;
    constexpr int MEAS_UNIT_POS = 21;
    constexpr int MEAS_ITEM_POS = 2;
    constexpr int MEAS_ITEM_FONT_SIZE = 24;
  }
}

// --- Key mapping constants ---
const char KEY_SETTING_ESCAPE = '`';
const char KEY_SETTING_BRIGHTNESS = '1';
const char KEY_SETTING_LOWBAT = '2';
const char KEY_SETTING_LANG = '3';
const char KEY_UP = ';';
const char KEY_DOWN = '.';
const char KEY_LEFT = ',';
const char KEY_RIGHT = '/';

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
void ky039Sensor();
void ky039Init();
void calcBeat(float newData);
void prtBPM(float temp_val);
void dispInit();
bool keyCheck();
void settings();
void changeSettings(SettingMode mode, KeyNum keyNo);
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
  dispInit();
  canvas.pushSprite(0, 0);
  dbPrtln("end of setup");
}

void loop()
{
  ky039Sensor();
  batteryState();

  if (keyCheck())
    settings();

  // vTaskDelay(1);
}

constexpr uint8_t sensorPin = 1;

void ky039Init()
{
  pinMode(sensorPin, ANALOG);
  // for (int i = 0; i < samp_siz; i++)
  //   SAMPS[i] = 0;
}

constexpr unsigned long MEAS_PERIOD_MS = 20; // measuremnt interval msec
static unsigned long PREV_MEAS_TM = 0;
static uint32_t READ_VAL = 0;
static uint32_t N_MEAS = 0;

void ky039Sensor()
{
  // calculate an average of the  sensor
  // during a 20 ms period (this will eliminate
  // the 50  Hz noise caused by electric light
  unsigned long current_meas_tm = millis();
  READ_VAL += analogRead(sensorPin); // read and add values...
  N_MEAS++;

  if (current_meas_tm - PREV_MEAS_TM < MEAS_PERIOD_MS)
    return;

  float avData = (float)READ_VAL / N_MEAS; // and take an average of the values

  PREV_MEAS_TM = current_meas_tm;
  READ_VAL = 0;
  N_MEAS = 0;

  calcBeat(avData);
}

// moving averaging sampling
// constexpr int samp_siz = 4;
// constexpr int samp_siz = 10;
constexpr int samp_siz = 15;
// constexpr int samp_siz = 20;

static int samp_pos = 0;            // current position in the array
static float SAMPS[samp_siz] = {0};
// ---------------------------------------------

static float PREV_CURVE = 4096.0; // impossible value
static bool isRISING = true;
static int RISE_CNT = 0;
static unsigned long PREV_BEAT01 = 0, PREV_BEAT02 = 0;
static unsigned long PREV_BEAT_TM = 0;
static float PREV_BPMVAL01 = 0;
static float PREV_BPMVAL02 = 0;

void calcBeat(float newData)
{
  constexpr uint8_t rise_threshold = 4;
  // constexpr uint8_t rise_threshold = 6;

  // Add the  newest measurement to an array
  // and subtract the oldest measurement from  the array
  // to maintain a sum of last measurements
  SAMPS[samp_pos++] = newData;
  samp_pos %= samp_siz;

  // new curve : average of the values in the array
  float sum_val = 0;
  for (int i = 0; i < samp_siz; i++)
    sum_val += SAMPS[i];
  float current_curve = sum_val / samp_siz;
  Serial.printf(">current_curve:%f\n", current_curve);

  // check  for a rising curve (= a heart beat)
  if (current_curve > PREV_CURVE)
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
      float current_bpmVal = 60000. / (0.4 * current_beat + 0.3 * PREV_BEAT01 + 0.3 * PREV_BEAT02);

      // *** SELECT VALID DATA ***
      if (current_curve < 2400.0 || current_curve > 2900.0)  // AD value
      {// Not the desired data
        dbPrtln(" invalid curve value = " + String(current_curve));
        prtBPM(-1.0); // invalid data
      }
      else if (current_beat < 500.0 || current_beat > 2000.0) // msec
      { //  500msec period -> 2Hz   -> 120BPM .... invalid data
        // 2000msec period -> 0.5Hz ->  30BPM .... invalid data
        dbPrtln(" invalid curve_beat = " + String(current_beat));
        // prtBPM(-1.0); // invalid data
      }
      else if (current_bpmVal < 30.0 || current_bpmVal > 120.0) // bpm
      { // invalid heart beat bpm .... reject
        dbPrtln("invalid bpm value = " + String(current_bpmVal));
        // prtBPM(-1.0); // invalid data
      }
      else if (abs(current_bpmVal - PREV_BPMVAL01) > 5 || abs(current_bpmVal - PREV_BPMVAL02) > 5)  // bpm
      { // distributed unevenly value ... not stable
        dbPrtln(" distributed unevenly bpm value = " + String(current_bpmVal));
        // prtBPM(-1.0); // invalid data
      }
      else
      {
        prtBPM(current_bpmVal);
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
  PREV_CURVE = current_curve;
}

constexpr int BPM_FONT_SIZE = 48;
constexpr int BPM_LINE_INDEX = 3;
constexpr int BPM_DISP_WIDTH = 27;
static float PREV_BPM_DISP = 0.0;
void prtBPM(float temp_val)
{
  // Skip redrawing if the value hasn't changed.
  // This handles both number-to-number and NAN-to-NAN comparisons.
  if (PREV_BPM_DISP == temp_val || (isnan(PREV_BPM_DISP) && isnan(temp_val)))
  {
    return;
  }
  PREV_BPM_DISP = temp_val;

  char buf[10];
  if (isnan(temp_val) || temp_val < 0)
  {
    snprintf(buf, sizeof(buf), "---.-");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%3.1f", temp_val);
  }

  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setFont(&fonts::Font7);
  canvas.setTextSize(1);
  canvas.fillRect(0, SC_LINES[BPM_LINE_INDEX], X_WIDTH, BPM_FONT_SIZE, TFT_BLACK);
  canvas.drawCenterString(buf, X_WIDTH / 2, SC_LINES[BPM_LINE_INDEX]);
  canvas.pushSprite(0, 0);
}
// ************************************************************************************

void dispInit()
{
  // ---012345678901234567890123456789----
  // L0:- HC-SR04 Sensor -    bat.---%
  // L1: (settings display line)
  // L2:
  // L3:
  // L4:
  // L5:
  // L6:
  // L7:  Distance                cm
  // ---012345678901234567890123456789----

  canvas.fillScreen(TFT_BLACK); // all clear
  canvas.setFont(&fonts::lgfxJapanGothic_16);

  //--L0 : title--------------
  canvas.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  canvas.drawString(F("- KY-039 Heart Beat -"), 0, SC_LINES[0]);

  // L0 :Battery Level -----
  dispBatItem();
  canvas.drawString(F("---"), W_CHR * AppConfig::Layout::BATLVL_VALUE_POS, SC_LINES[0]);
  canvas.drawString(F("%"), W_CHR * AppConfig::Layout::BATLVL_PERCENT_POS, SC_LINES[0]);

  // L7 : Measuremnt items
  canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  canvas.drawString(F("bpm"), W_CHR * AppConfig::Layout::MEAS_UNIT_POS, SC_LINES[7], &fonts::Font4);
  dispMeasItem();
}

bool keyCheck()
{
  M5Cardputer.update(); // update Cardputer key input

  if (M5Cardputer.Keyboard.isChange())
  {
    if (M5Cardputer.Keyboard.isPressed())
      return true;
  }
  return false;
}

void settings()
{
  // Part 1: Handle setting mode changes.
  // These keys change the current setting mode.
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_SETTING_ESCAPE))
  {
    if (settingMode == SM_ESC)
      return;
    settingMode = SM_ESC;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_SETTING_BRIGHTNESS))
  {
    if (settingMode == SM_BRIGHT_LEVEL)
      return;
    settingMode = SM_BRIGHT_LEVEL;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_SETTING_LOWBAT))
  {
    if (settingMode == SM_LOWBAT_THRESHOLD)
      return;
    settingMode = SM_LOWBAT_THRESHOLD;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_SETTING_LANG))
  {
    if (settingMode == SM_LANG)
      return;
    settingMode = SM_LANG;
  }
  else
  {
    // Part 2: Handle value adjustments for the current mode.
    // These keys adjust the value of the selected setting.
    KeyNum keyNum = KN_NONE;

    if (M5Cardputer.Keyboard.isKeyPressed(KEY_UP))
    {
      keyNum = KN_UP;
    }
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_DOWN))
    {
      keyNum = KN_DOWN;
    }
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_LEFT))
    {
      keyNum = KN_LEFT;
    }
    else if (M5Cardputer.Keyboard.isKeyPressed(KEY_RIGHT))
    {
      keyNum = KN_RIGHT;
    }
    else
    {
      return; // No relevant key pressed for mode change or value adjustment.
    }
    changeSettings(settingMode, keyNum);
    return; // Exit after handling value adjustment.
  }

  // This part is reached only when the mode has been changed (Part 1).
  // It displays the initial state for the new mode.
  changeSettings(settingMode, KN_NONE);
}

void changeSettings(SettingMode mode, KeyNum keyNo)
{
  switch (mode)
  {
  case SM_ESC:
    canvas.fillRect(0, SC_LINES[1], X_WIDTH, H_CHR, TFT_BLACK);
    break;
  case SM_BRIGHT_LEVEL:
    changeBright(keyNo);
    break;
  case SM_LOWBAT_THRESHOLD:
    changeLowBatThr(keyNo);
    break;
  case SM_LANG:
    changeLang(keyNo);
    break;
  default:
    return;
  }
  canvas.pushSprite(0, 0);
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
  case KN_UP:
  case KN_DOWN:
  case KN_RIGHT:
  case KN_LEFT:
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
  canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  canvas.drawString(meas_items[LANG_INDEX], W_CHR * AppConfig::Layout::MEAS_ITEM_POS, SC_LINES[7]);
}

bool updateSettingValue(uint8_t &value, KeyNum keyNo, uint8_t min, uint8_t max, uint8_t step, uint8_t big_step)
{
  int tempValue = value;

  switch (keyNo)
  {
  case KN_UP:
    tempValue += big_step;
    break;
  case KN_DOWN:
    tempValue -= big_step;
    break;
  case KN_RIGHT:
    tempValue += step;
    break;
  case KN_LEFT:
    tempValue -= step;
    break;
  default:
    return false; // Not a value-changing key
  }

  // Clamp the value to the allowed range
  if (tempValue > max)
    tempValue = max;
  if (tempValue < min)
    tempValue = min;

  if (value != (uint8_t)tempValue)
  {
    value = (uint8_t)tempValue;
    return true; // Value changed
  }
  return false; // No change in value
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
