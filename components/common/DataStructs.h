#pragma once

struct SunriseSettings
{
    int brightness = 100;
    int red = 255;
    int green = 100;
    int blue = 0;
    int duration_minutes = 30;
    int alarm_hour = 7;    // 0–23
    int alarm_minute = 30; // 0–59

    bool enabled = true;
};