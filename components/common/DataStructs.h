// DataStructs.h
#pragma once

struct SunriseSettings
{
    int red = 255;
    int green = 100;
    int blue = 0;

    bool light_preview = false;

    int duration_minutes = 5;
    int duration_on_brightest = 30;
    int alarm_hour = 7;
    int alarm_minute = 30;

    bool alarm_enabled = false;
    bool disable_hardware_switches = false;
};