#ifndef WEATHER_H
#define WEATHER_H

// Turning Open-Meteo's answers into what the Weather pattern shows.
// Hardware independent (test/test_weather).

#include <stdint.h>

namespace weather
{
    /// What the sky is doing, for the animation.
    enum class Sky : uint8_t
    {
        CLEAR,
        PARTLY_CLOUDY,
        CLOUDY,
        FOG,
        DRIZZLE,
        RAIN,
        SNOW,
        STORM,
    };

    /// The sky for a WMO weather code (Open-Meteo's weather_code).
    Sky sky(int wmo);

    /// A short description, at most 10 characters (one line of the top
    /// face): "Clear", "Pt cloudy", "Heavy rain".
    const char *describe(int wmo);

    /// How heavy the rain, snow or drizzle is: 0.3 light, 0.6 moderate,
    /// 1 heavy; 0 for none.
    float intensity(int wmo);

    /// A temperature in whole degrees C or F.
    int temperature(float celsius, bool metric);

    /// A wind speed in whole km/h or mph.
    int windSpeed(float kmh, bool metric);

    /// Percent-encodes `in` for a URL query. Returns the length written, or
    /// -1 if it did not fit.
    int urlEncode(const char *in, char *out, int size);
}

#endif
