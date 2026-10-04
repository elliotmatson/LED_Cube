#include "weather.h"

#include <math.h>

weather::Sky weather::sky(int wmo)
{
    switch (wmo)
    {
    case 0: return Sky::CLEAR;
    case 1:
    case 2: return Sky::PARTLY_CLOUDY;
    case 3: return Sky::CLOUDY;
    case 45:
    case 48: return Sky::FOG;
    case 51:
    case 53:
    case 55:
    case 56:
    case 57: return Sky::DRIZZLE;
    case 61:
    case 63:
    case 65:
    case 66:
    case 67:
    case 80:
    case 81:
    case 82: return Sky::RAIN;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86: return Sky::SNOW;
    case 95:
    case 96:
    case 99: return Sky::STORM;
    default: return Sky::CLOUDY;
    }
}

const char *weather::describe(int wmo)
{
    switch (wmo)
    {
    case 0: return "Clear";
    case 1: return "Fair";
    case 2: return "Pt cloudy";
    case 3: return "Overcast";
    case 45: return "Fog";
    case 48: return "Icy fog";
    case 51: return "Lt drizzle";
    case 53:
    case 55: return "Drizzle";
    case 56:
    case 57:
    case 66:
    case 67: return "Sleet";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 71: return "Light snow";
    case 73:
    case 77: return "Snow";
    case 75: return "Heavy snow";
    case 80:
    case 81: return "Showers";
    case 82: return "Downpour";
    case 85:
    case 86: return "Flurries";
    case 95: return "T-storm";
    case 96:
    case 99: return "Hail storm";
    default: return "Cloudy";
    }
}

float weather::intensity(int wmo)
{
    switch (wmo)
    {
    case 51:
    case 56:
    case 61:
    case 66:
    case 71:
    case 77:
    case 80:
    case 85: return 0.3f;
    case 53:
    case 63:
    case 73:
    case 81:
    case 95: return 0.6f;
    case 55:
    case 57:
    case 65:
    case 67:
    case 75:
    case 82:
    case 86:
    case 96:
    case 99: return 1.0f;
    default: return 0;
    }
}

int weather::temperature(float celsius, bool metric)
{
    return int(lroundf(metric ? celsius : celsius * 9 / 5 + 32));
}

int weather::windSpeed(float kmh, bool metric)
{
    return int(lroundf(metric ? kmh : kmh / 1.609344f));
}

int weather::urlEncode(const char *in, char *out, int size)
{
    static const char HEX[] = "0123456789ABCDEF";
    int n = 0;
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(in); *c; c++)
    {
        const bool plain = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') ||
                           *c == '-' || *c == '_' || *c == '.' || *c == '~';
        if (plain)
        {
            if (n + 1 >= size)
            {
                return -1;
            }
            out[n++] = char(*c);
        }
        else
        {
            if (n + 3 >= size)
            {
                return -1;
            }
            out[n++] = '%';
            out[n++] = HEX[*c >> 4];
            out[n++] = HEX[*c & 15];
        }
    }
    if (n >= size)
    {
        return -1;
    }
    out[n] = '\0';
    return n;
}
