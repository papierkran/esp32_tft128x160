/**
 * LVGL v9.5.0 配置文件 - ESP32 + ST7735S 1.8" 128x160
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

/* STDLIB */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN
#define LV_STDINT_INCLUDE       <stdint.h>
#define LV_STDDEF_INCLUDE       <stddef.h>
#define LV_STDBOOL_INCLUDE      <stdbool.h>
#define LV_INTTYPES_INCLUDE     <inttypes.h>
#define LV_LIMITS_INCLUDE       <limits.h>
#define LV_STDARG_INCLUDE       <stdarg.h>

#define LV_MEM_SIZE (32 * 1024U)

/* HAL */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

/* 只保留最小字体 */
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_36 1

/* 中文字体 */
#define LV_FONT_SOURCE_HAN_SANS_SC_16_CJK 1

/* 最大精简 */
#define LV_USE_ANIMATION    0
#define LV_USE_SHADOW       0
#define LV_USE_BLEND_MODES  0
#define LV_USE_GRADIENT     0
#define LV_USE_IME_PIN      0
#define LV_USE_FILE_EXPLORER 0
#define LV_USE_GRIDNAV      0
#define LV_USE_FRAGMENT     0
#define LV_USE_IMGFONT      0
#define LV_USE_SNAPSHOT     0
#define LV_USE_MONKEY       0
#define LV_USE_GRID         0
#define LV_USE_FLEX         0
#define LV_USE_SYSMON       0
#define LV_USE_PROFILER     0
#define LV_USE_OBJ_ID       0
#define LV_USE_OBSOLETE     0
#define LV_USE_LOG          0
#define LV_USE_ASSERT_NULL  0
#define LV_USE_ASSERT_MALLOC 0
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ   0
#define LV_USE_REFR_DEBUG   0
#define LV_USE_PARALLEL_DRAW_DEBUG 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_DRAW_SW      1
#define LV_USE_DRAW_PXP     0
#define LV_USE_DRAW_DAVE2D  0
#define LV_USE_DRAW_OPENGLES 0
#define LV_USE_DRAW_VG_LITE 0
#define LV_USE_OS           0

/* 精简组件 */
#define LV_USE_ARC          0
#define LV_USE_BAR          0
#define LV_USE_BUTTON       0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR     0
#define LV_USE_CANVAS       0
#define LV_USE_CHART        0
#define LV_USE_CHECKBOX     0
#define LV_USE_DROPDOWN     0
#define LV_USE_IMAGE        0
#define LV_USE_IMAGEBUTTON  0
#define LV_USE_KEYBOARD     0
#define LV_USE_LINE         0
#define LV_USE_LIST         0
#define LV_USE_MENU         0
#define LV_USE_MSGBOX       0
#define LV_USE_ROLLER       0
#define LV_USE_SCALE        0
#define LV_USE_SLIDER       0
#define LV_USE_SPAN         0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      0
#define LV_USE_SWITCH       0
#define LV_USE_TABLE        0
#define LV_USE_TABVIEW      0
#define LV_USE_TEXTAREA     0
#define LV_USE_TILEVIEW     0
#define LV_USE_WIN          0

#endif