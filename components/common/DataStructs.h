// DataStructs.h
#pragma once

enum class LightMode {
    still,
    breathing,
    rolling
};

struct LightSettings
{
    int red = 255;
    int green = 100;
    int blue = 0;
    
    LightMode mode = LightMode::still;
    bool active = false;
};

struct SunriseSettings
{
    int red = 255;
    int green = 100;
    int blue = 0;

    int duration_minutes = 5;
    int duration_on_brightest = 30;
    int alarm_hour = 7;
    int alarm_minute = 30;

    bool enabled = false;
};