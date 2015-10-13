/**
 * @file leash_display.h
 *
 * Definition of the leash display topic.
 */

#ifndef LEASH_DISPLAY_H_
#define LEASH_DISPLAY_H_

#include "../uORB.h"
#include <stdint.h>

/**
 * @addtogroup topics
 * @{
 */

enum
{
    LEASHDISPLAY_NONE,
    LEASHDISPLAY_LOGO,
    LEASHDISPLAY_MAIN,
    LEASHDISPLAY_MENU,
    LEASHDISPLAY_INFO,
    LEASHDISPLAY_LIST,
    LEASHDISPLAY_TEST,
};

#define LEASHDISPLAY_LINE_COUNT 5
#define LEASHDISPLAY_LINE_LENGH 25
#define LEASHDISPLAY_TEXT_SIZE 30
typedef char LeashDisplay_Lines[LEASHDISPLAY_LINE_COUNT][LEASHDISPLAY_LINE_LENGH];

enum
{
    FOLLOW_ABS,
    FOLLOW_PATH,
    FOLLOW_LINE,
    FOLLOW_ADAPTIVE,
    FOLLOW_CIRCLE,
    FOLLOW_MAX,
};

enum
{
    LAND_HOME = 1,
    LAND_SPOT,
    LAND_MAX,
};

enum
{
    MAINSCREEN_INFO,
    MAINSCREEN_INFO_SUB,
    MAINSCREEN_LANDING,
    MAINSCREEN_TAKING_OFF,
    MAINSCREEN_READY_TO_TAKEOFF,
    MAINSCREEN_CONFIRM_TAKEOFF,
    MAINSCREEN_GOING_HOME,
    MAINSCREEN_MAX
};

enum
{
    AIRDOGMODE_NONE,
    AIRDOGMODE_PLAY,
    AIRDOGMODE_PAUSE,
};

enum
{
    MENUTYPE_SETTINGS,
    MENUTYPE_ACTIVITIES,
    MENUTYPE_SELECTED_ACTIVITY,
    MENUTYPE_PAIRING,
    MENUTYPE_CALIBRATION,
    MENUTYPE_CALIBRATION_AIRDOG,
    MENUTYPE_SENSOR_CHECK,
    MENUTYPE_COMPASS,
    MENUTYPE_ACCELS,
    MENUTYPE_GYRO,
    MENUTYPE_RESET,
    MENUTYPE_SELECT,
    MENUTYPE_CUSTOMIZE,
    MENUTYPE_ALTITUDE,
    MENUTYPE_FOLLOW,
    MENUTYPE_LANDING,
    MENUTYPE_CUSTOM_VALUE,
    MENUTYPE_SAVE,
    MENUTYPE_CANCEL,
    MENUTYPE_MAX
};

enum
{
    INFO_CONNECTING_TO_AIRDOG,
    INFO_ESTABLISHING_CONNECTION,
    INFO_GETTING_ACTIVITIES,
    INFO_CONNECTION_LOST,
    INFO_TAKEOFF_FAILED,
    INFO_CALIBRATING_SENSORS,
    INFO_CALIBRATING_AIRDOG,
    INFO_PAIRING,
    INFO_PAIRING_OK,
    INFO_NOT_PAIRED,
    INFO_ACQUIRING_GPS_LEASH,
    INFO_ACQUIRING_GPS_AIRDOG,
    INFO_CALIBRATING_HOLD_STILL,
    INFO_SUCCESS,
    INFO_FAILED,
    INFO_CALIBRATING_DANCE,
    INFO_NEXT_SIDE_UP,
    INFO_ARE_YOU_SURE,
    INFO_REBOOT,
    INFO_ERROR,
    INFO_SENSOR_VALIDATION_REQUIRED,
    INFO_SENSOR_VALIDATION_SHOW_STATUS,
    INFO_SENSOR_VALIDATION_OK_TO_START,
    INFO_SENSOR_VALIDATION_COUNTDOWN,
    INFO_MAX,
};

enum
{
    MENUBUTTON_LEFT = 1,
    MENUBUTTON_RIGHT = 2
};

/**
 * leash display status
 */
struct leash_display_s {
    int screenId;
    char presetName[20];
    int mainMode;
    int followMode;
    int landMode;
    int airdogMode;
    int menuType;
    int menuValue;
    int menuButtons;
    int infoId;
    int infoError;
    int leashGPS;
    int airdogGPS;
    int activity;
    char customText[LEASHDISPLAY_TEXT_SIZE];

    // show text for debug puprose
    LeashDisplay_Lines lines;
    int lineCount;
};

/**
 * @}
 */

/* register this as object request broker structure */
ORB_DECLARE(leash_display);

#endif
