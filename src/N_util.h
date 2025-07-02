// *******************************************************
//  N_UTIL          by NoRi 2025-04-15
// -------------------------------------------------------
// N_util.h
// *******************************************************
#ifndef _N_UTIL_H
#define _N_UTIL_H
// -------------------------------------------------------
#include <Arduino.h>
#include <SD.h>
#include <nvs.h>
#include <M5Cardputer.h>
#include <M5GFX.h>

extern M5Canvas canvas;
extern bool SD_ENABLE;
extern const int32_t N_COLS, N_ROWS;
extern int32_t W_CHR, H_CHR;        // Character dimensions
extern int32_t X_WIDTH, Y_HEIGHT;   // Screen dimensions
extern int32_t SC_LINES[];          // Array to store Y coordinates of each line

extern void m5stack_begin();
extern void SDU_lobby();
extern bool SD_begin();
extern void dbPrtln(String msg);
extern void dbPrt(String msg);
extern void dispLx(uint8_t Lx, const char *msg);
extern void POWER_OFF();
extern bool wrtNVS(const char *title, uint8_t data);
extern bool rdNVS(const char *title, uint8_t &data);
extern void loadSetting(const char *nvs_key, uint8_t &setting_variable, uint8_t default_value, uint8_t min_val, uint8_t max_val);

// -------------------------------------------------------
#endif // _N_UTIL_H
