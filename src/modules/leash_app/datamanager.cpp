#include "datamanager.h"

#include <poll.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

DataManager* DataManager::_instance = nullptr;

DataManager::DataManager() :
    activityManager()
{
    // get orbs id
    orbId[FD_ActivityParams] = ORB_ID(activity_params);
    orbId[FD_AirdogStatus] = ORB_ID(airdog_status);
    orbId[FD_BLRHandler] = ORB_ID(bt_state);
    orbId[FD_BTLinkQuality] = ORB_ID(bt_link_status);
    orbId[FD_Calibrator] = ORB_ID(calibrator);
    orbId[FD_DroneLocalPos] = ORB_ID(target_global_position);
    orbId[FD_DroneRowGPS] = ORB_ID(target_gps_raw);
    orbId[FD_KbdHandler] = ORB_ID(kbd_handler);
    orbId[FD_LeashGlobalPos] = ORB_ID(vehicle_global_position);
    orbId[FD_LeashRowGPS] = ORB_ID(vehicle_gps_position);
    orbId[FD_LocalPos] = ORB_ID(vehicle_local_position);
    orbId[FD_MavlinkStatus] = ORB_ID(mavlink_receive_stats);
    orbId[FD_SystemPower] = ORB_ID(system_power);
    orbId[FD_VehicleStatus] = ORB_ID(vehicle_status);

    // set orbs interval
    orb_set_interval(fds[FD_AirdogStatus], 5000);
    orb_set_interval(fds[FD_SystemPower], 5000);
    orb_set_interval(fds[FD_MavlinkStatus], 1000);

    // set addresses
    memset(orbData, 0, sizeof(orbData));

    orbData[FD_ActivityParams] = &activity_params;
    orbData[FD_AirdogStatus] = &airdog_status;
    orbData[FD_BLRHandler] = &bt_handler;
    orbData[FD_BTLinkQuality] = &btLinkQuality;
    orbData[FD_Calibrator] = &calibrator;
    orbData[FD_DroneLocalPos] = &droneLocalPos;
    orbData[FD_DroneRowGPS] = &droneRawGPS;
    orbData[FD_KbdHandler] = &kbd_handler;
    orbData[FD_LeashGlobalPos] = &leashGlobalPos;
    orbData[FD_LeashRowGPS] = &leashRawGPS;
    orbData[FD_LocalPos] = &localPos;
    orbData[FD_MavlinkStatus] = &mavlink_received_stats;
    orbData[FD_SystemPower] = &system_power;
    orbData[FD_VehicleStatus] = &vehicle_status;

    // listen orbs
    for (int i = 0; i < FD_Size; i++)
    {
        fds[i] = orb_subscribe(orbId[i]);
        orb_copy(orbId[i], fds[i], orbData[i]);
    }

    // clear
    memset(awaitMask, 0, sizeof(awaitMask));
}

DataManager::~DataManager()
{
    for (int i = 0; i < FD_Size; i++)
    {
        orb_unsubscribe(fds[i]);
    }
}


DataManager* DataManager::instance()
{
    if (_instance == nullptr) 
    {
        _instance = new DataManager();
    }
    return _instance;
}

bool DataManager::wait(int timeout)
{
    int r = 0;
    struct pollfd pollfds[FD_Size];
    bool hasChanges = false;

    memset(awaitResult, 0, sizeof(awaitResult));

    // clean all events
    memset(pollfds, 0, sizeof(pollfds));

    for (int i = 0; i < FD_Size; i++)
    {
        pollfds[i].fd = fds[i];

        if (awaitMask[i])
        {
            pollfds[i].events = POLLIN;
        }
    }

    r = poll(pollfds, FD_Size, timeout);

    if (r == -1)
    {
        DOG_PRINT("[leash datamanager] poll failed. errno %d\n", errno);
    }

    for (int i = 0; i < FD_Size; i++)
    {
        if (pollfds[i].revents & POLLIN)
        {
            hasChanges = true;
            awaitResult[i] = true;
            orb_copy(orbId[i], fds[i], orbData[i]);
        }
    }

    return hasChanges;
}

void DataManager::clearAwait()
{
    memset(awaitMask, 0, sizeof(awaitMask));
}
