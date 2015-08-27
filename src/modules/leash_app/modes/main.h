#pragma once

#include "error.h"

#include <uORB/topics/leash_display.h>

#include <drivers/drv_hrt.h>

namespace modes
{

class Main : public Error
{
public:
    Main();

    virtual int getTimeout();
    virtual void listenForEvents(bool awaitMask[]);
    virtual Base* doEvent(int orbId);

    virtual bool onError(int errorCode);
private:
    enum MainStates
    {
        GROUNDED = 0, 
        IN_FLINGHT,
        MANUAL_FLIGHT,
    };
    enum SubStates
    {
        NONE = 0,
        // -- GROUNDED subs --
        HELP,
        CONFIRM_TAKEOFF,
        TAKEOFF_CONFIRMED,
        TAKEOFF_FAILED,
        // -- IN_FLIGHT subs --
        PLAY,
        PAUSE,
        // -- Landing and Taking off subs --
        TAKING_OFF,
        LANDING,
        RTL,
    };
    struct Condition
    {
        MainStates main;
        SubStates sub;
    };
    struct DisplayInfo
    {
        int mode;
        int airdog_mode;
        int follow_mode;
        int land_mode;
    };
    typedef enum
    {
        NO_GPS = 0,
        BAD_GPS,
        FAIR_GPS,
        GOOD_GPS,
        EXCELENT_GPS,
    } gps_qality;

    char currentActivity[20];
	const hrt_abstime command_responce_time = 10000000;
	hrt_abstime local_timer = 0;

    bool ignoreKeyEvent;
    gps_qality leashGPS;
    gps_qality airdogGPS;

    struct DisplayInfo displayInfo;
    struct Condition baseCondition;
    Base* makeAction();
    Base* processGround(int orbId);
    Base* processTakeoff(int orbId);
    Base* processLandRTL(int orbId);
    Base* processHelp(int orbId);
    Base* processFlight(int orbId);

    void decide_command(MainStates mainState);
    void checkGPS();
};

}
