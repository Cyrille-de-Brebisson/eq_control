#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <iostream>
#include <cmath>
#include <ctime>
#include "libsgp4/Observer.h"
#include "libsgp4/SGP4.h"
#include "libsgp4/Util.h"
#include "libsgp4/CoordTopocentric.h"
#include "libsgp4/CoordGeodetic.h"
using namespace libsgp4;
#include <windows.h>

double FindMaxElevation(const libsgp4::CoordGeodetic& user_geo, libsgp4::SGP4& sgp4, const libsgp4::DateTime& aos, const libsgp4::DateTime& los)
{
    libsgp4::Observer obs(user_geo);
    double time_step = (los - aos).TotalSeconds() / 9.0;
    libsgp4::DateTime current_time(aos); //! current time
    libsgp4::DateTime time1(aos); //! start time of search period
    libsgp4::DateTime time2(los); //! end time of search period
    double max_elevation; //! max elevation

    do
    {
        bool running = true;
        max_elevation = -99999999999999.0;
        while (current_time < time2) // As long as going up, continue....
        {   // find position
            libsgp4::Eci eci = sgp4.FindPosition(current_time);
            libsgp4::CoordTopocentric topo = obs.GetLookAngle(eci);
            if (topo.elevation > max_elevation) // going up
            {
                max_elevation = topo.elevation;
                current_time = current_time.AddSeconds(time_step);
                if (current_time > time2) current_time = time2;
            }
            else break;
        }
        // Reduce time steps to get more and more accurate.. Until we have less than 1s...
        time1 = current_time.AddSeconds(-2.0 * time_step); // go back a little bit
        time2 = current_time;
        current_time = time1;
        time_step = (time2 - time1).TotalSeconds() / 9.0;
    }
    while (time_step > 1.0);

    return max_elevation;
}

libsgp4::DateTime FindCrossingPoint(const libsgp4::CoordGeodetic& user_geo, libsgp4::SGP4& sgp4, const libsgp4::DateTime& initial_time1, const libsgp4::DateTime& initial_time2, bool finding_aos)
{
    libsgp4::Observer obs(user_geo);
    libsgp4::DateTime time1(initial_time1);
    libsgp4::DateTime time2(initial_time2);
    libsgp4::DateTime middle_time;

    int cnt = 0;
    while (cnt++ < 16) // looks like a 16 pass divide by 2 the search space system
    {
        middle_time = time1.AddSeconds((time2 - time1).TotalSeconds() / 2.0);
        libsgp4::Eci eci = sgp4.FindPosition(middle_time);
        libsgp4::CoordTopocentric topo = obs.GetLookAngle(eci);

        if (topo.elevation > 0.0) // Above horizon
        {
            if (finding_aos) time2 = middle_time;
            else time1 = middle_time;
        } else { // Bellow horizon
            if (finding_aos) time1 = middle_time;
            else time2 = middle_time;
        }

        if ((time2 - time1).TotalSeconds() < 1.0) // less than 1s appart, stop...
        {
            int us = middle_time.Microsecond(); // make accurate to 1s
            middle_time = middle_time.AddMicroseconds(-us);
            middle_time = middle_time.AddSeconds(finding_aos ? 1 : -1); // go back 1s if/as needed to bracket our object
            break;
        }
    }

    // go back/forward in second steps until below the horizon
    cnt = 0;
    while (cnt++ < 6)
    {
        libsgp4::Eci eci = sgp4.FindPosition(middle_time);
        libsgp4::CoordTopocentric topo = obs.GetLookAngle(eci);
        if (topo.elevation > 0) middle_time = middle_time.AddSeconds(finding_aos ? -1 : 1);
        else break;
    }

    return middle_time;
}

struct PassDetails { double timeTo_H, duration_mn, max_elevation_deg; };  // timeTo_H is how long form startTime to pass in H

int GeneratePassList(const libsgp4::CoordGeodetic& user_geo, libsgp4::SGP4& sgp4, const libsgp4::DateTime& start_time, const libsgp4::DateTime& end_time, const int time_step, PassDetails *pass_list, int nbpassesMax)
{
    int nbpasses= 0;
    libsgp4::Observer obs(user_geo);
    libsgp4::DateTime aos_time;
    libsgp4::DateTime los_time;
    libsgp4::DateTime previous_time(start_time);
    libsgp4::DateTime current_time(start_time);
    bool found_aos = false;

    while (current_time < end_time)
    {
        bool end_of_pass = false;
        libsgp4::Eci eci = sgp4.FindPosition(current_time);
        libsgp4::CoordTopocentric topo = obs.GetLookAngle(eci);

        if (!found_aos && topo.elevation > 0.0) // we have not found the cross up, but we are above... so we "missed it"
        {
            if (start_time == current_time) aos_time = start_time; // satellite was already above the horizon at the start, so use the start time
            else // find the point at which the satellite crossed the horizon
                aos_time = FindCrossingPoint(user_geo, sgp4, previous_time, current_time, true);
            found_aos = true;
        }
        else if (found_aos && topo.elevation < 0.0)
        {
            found_aos = false;
            end_of_pass = true; // end of pass, so move along more than time_step
            // already have the aos, but now the satellite is below the horizon, so find the los
            los_time = FindCrossingPoint(user_geo, sgp4, previous_time, current_time, false);

            PassDetails pd= { (aos_time-start_time).TotalHours(), (los_time-aos_time).TotalHours(), libsgp4::Util::RadiansToDegrees(FindMaxElevation(user_geo, sgp4, aos_time, los_time)) };
            pass_list[nbpasses++]= pd; if (nbpasses==nbpassesMax) return nbpasses;
        }

        previous_time = current_time;

        if (end_of_pass) current_time = current_time + libsgp4::TimeSpan(0, 30, 0); // at the end of the pass move the time along by 30mins
        else current_time = current_time + libsgp4::TimeSpan(0, 0, time_step); // move the time along by the time step value
    };

    return nbpassesMax;
}

void currentPos(const libsgp4::CoordGeodetic& user_geo, libsgp4::SGP4& sgp4, const libsgp4::DateTime& time, libsgp4::CoordTopocentric &topo)
{
    libsgp4::Observer obs(user_geo);
    libsgp4::Eci eci = sgp4.FindPosition(time);
    topo = obs.GetLookAngle(eci);
}

extern "C" __declspec(dllexport) int GeneratePassList(double siteLat, double siteLong, double siteAltitude, char const *tle1, char const *tle2, PassDetails *pass_list, int nbpassesMax)
{
    libsgp4::CoordGeodetic user_geo(siteLat, siteLong, siteAltitude);
    libsgp4::Tle tle("ISS (NAUKA)             ", tle1, tle2);
    libsgp4::SGP4 sgp4(tle);

    libsgp4::DateTime start_date = libsgp4::DateTime::Now(true);
    libsgp4::DateTime end_date(start_date.AddDays(7.0));
    return GeneratePassList(user_geo, sgp4, start_date, end_date, 180, pass_list, nbpassesMax);
}

extern "C" __declspec(dllexport) void currentPos(double siteLat, double siteLong, double siteAltitude, char const *tle1, char const *tle2, double &az, double &alt, double &dst, double inSecondsFromNow)
{
    libsgp4::CoordGeodetic user_geo(siteLat, siteLong, siteAltitude);
    libsgp4::Tle tle("ISS (NAUKA)             ", tle1, tle2);
    libsgp4::SGP4 sgp4(tle);
    libsgp4::DateTime time = libsgp4::DateTime::Now(true).AddSeconds(inSecondsFromNow);
    libsgp4::CoordTopocentric topo;
    currentPos(user_geo, sgp4, time, topo);
    az= Util::RadiansToDegrees(topo.azimuth);
    alt= Util::RadiansToDegrees(topo.elevation);
    dst= topo.range;
}

struct Tpos { double az, alt, dst; };
// get a list of coordinate for a pass. when is in h (get from pass list). typically 600 slots in pos is enough (10mn)
extern "C" __declspec(dllexport) int FuturePos(double siteLat, double siteLong, double siteAltitude, char const *tle1, char const *tle2, double when, Tpos *pos, int nbpos)
{
    libsgp4::CoordGeodetic user_geo(siteLat, siteLong, siteAltitude);
    libsgp4::Tle tle("ISS (NAUKA)             ", tle1, tle2);
    libsgp4::SGP4 sgp4(tle);
    libsgp4::DateTime time = libsgp4::DateTime::Now(true).AddHours(when);
    libsgp4::CoordTopocentric topo;
    bool hasAbove= false; int i=0;
    while (i<nbpos)
    {
        currentPos(user_geo, sgp4, time, topo);
         
        pos[i].az= Util::RadiansToDegrees(topo.azimuth);
        pos[i].alt= Util::RadiansToDegrees(topo.elevation);
        pos[i++].dst= topo.range;
        if (pos[i].alt>4.0) hasAbove= true;
        time= time.AddSeconds(1);
        if (hasAbove && pos[i].alt<0) break;
    }
    return i;
}

//  int main()
//  {
//      libsgp4::CoordGeodetic user_geo(45.7667, 4.335, 0.0);
//      libsgp4::Tle tle("ISS (NAUKA)             ",
//          "1 49044U 21066A   25188.76216729  .00007482  00000+0  13691-3 0  9993",
//          "2 49044  51.6337 205.0932 0002559 345.2175  14.8739 15.50423801224141"
//      );
//      libsgp4::SGP4 sgp4(tle);
//  
//      std::cout << tle << std::endl;
//  
//      // generate 7 day schedule
//      libsgp4::DateTime start_date = libsgp4::DateTime::Now(true);
// 
//      libsgp4::DateTime end_date(start_date.AddDays(7.0));
//  
//      PassDetails pass_list[5];
//  
//      std::cout << "Start time: " << start_date << std::endl;
//      std::cout << "End time  : " << end_date << std::endl << std::endl;
//  
//      int nbpass_list = GeneratePassList(user_geo, sgp4, start_date, end_date, 180, pass_list, 5);
//  
//      if (nbpass_list==0) std::cout << "No passes found" << std::endl;
//      else
//      {
//          std::stringstream ss;
//          ss << std::right << std::setprecision(1) << std::fixed;
//          for (int i=0; i<nbpass_list; i++)
//          {
//              ss  << "AOS: " << pass_list[i].aos << ", LOS: " << pass_list[i].los
//                  << ", Max El: " << std::setw(4) << libsgp4::Util::RadiansToDegrees(pass_list[i].max_elevation) << ", Duration: " << (pass_list[i].los - pass_list[i].aos)
//                  << std::endl;
//          }
//          std::cout << ss.str();
//      }
// 
//      while (true)
//      {
//          libsgp4::DateTime time = libsgp4::DateTime::Now(true);
//          libsgp4::CoordTopocentric topo;
//          currentPos(user_geo, sgp4, time, topo);
//          printf("\r%s", topo.ToString().c_str());
//          Sleep(1000);
//      }
//     return 0;
// }
