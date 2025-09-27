#pragma once
#include <time.h>
#include "../../common/DataStructs.h"

namespace Alarm {
    void init();
    bool obtain_time(int timeout_sec = 30);
    bool is_alarm_time(const SunriseSettings &settings, double &sunrise_percentage);
}
