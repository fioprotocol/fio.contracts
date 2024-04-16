/** fiotime definitions file
 *  Description: Handles fio time logic ( high performance )
 *  @author Casey Gardiner
 *  @file fiotime.hpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE ) Dapix
 *
 *  Changes:
 */

#include <time.h>
#include <limits.h>

/* 2000-03-01 (mod 400 year, immediately after feb29 */
#define LEAPOCH (946684800LL + 86400*(31+29))
#define DAYS_PER_400Y (365*400 + 97)
#define DAYS_PER_100Y (365*100 + 24)
#define DAYS_PER_4Y   (365*4   + 1)

#pragma once

namespace fioio {

    std::string tmstringformat(struct tm timeinfo) {
        std::string timebuffer = to_string(timeinfo.tm_year);
        timebuffer.append("-");
        if (timeinfo.tm_mon < 10) {
            timebuffer.append("0");
        }
        timebuffer.append(to_string(timeinfo.tm_mon));
        timebuffer.append("-");
        if (timeinfo.tm_mday < 10) {
            timebuffer.append("0");
        }
        timebuffer.append(to_string(timeinfo.tm_mday));
        timebuffer.append("T");
        if (timeinfo.tm_hour < 10) {
            timebuffer.append("0");
        }
        timebuffer.append(to_string(timeinfo.tm_hour));
        timebuffer.append(":");
        if (timeinfo.tm_min < 10) {
            timebuffer.append("0");
        }
        timebuffer.append(to_string(timeinfo.tm_min));
        timebuffer.append(":");
        if (timeinfo.tm_sec < 10) {
            timebuffer.append("0");
        }
        timebuffer.append(to_string(timeinfo.tm_sec));

        return timebuffer;
    }


 //temporary code for new computations of epoch times
    bool isLeapYear(int year) {
        return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
    }



    void dateTimeStructFromEpoch(long epochTime, struct tm& resultTime) {
        const long secondsPerDay = 86400;
        const long secondsPerYear = 31536000;
        const long secondsPerLeapYear = 31622400;
        const long epochYear = 1970;
        // Calculate year
        long totalDays = epochTime / secondsPerDay;
        int year = epochYear + totalDays / 365;
        int leapYears = (year - epochYear) / 4 - (year - epochYear) / 100 + (year - epochYear) / 400;
        // Adjust for leap years
        totalDays -= leapYears;
        while (totalDays < 0 || (isLeapYear(year + 1) && totalDays >= 366) || (!isLeapYear(year + 1) && totalDays >= 365)) {
            totalDays -= isLeapYear(year + 1) ? 366 : 365;
            year++;
        }
        // Calculate month and day
        int daysInMonth[] = {31, 28 + isLeapYear(year), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int month = 1;
        while (totalDays >= daysInMonth[month - 1]) {
            totalDays -= daysInMonth[month - 1];
            month++;
        }
        // Calculate day
        int day = totalDays + 1;
        // Calculate hour, minute, and second
        int remainingSeconds = epochTime % secondsPerDay;
        int hour = remainingSeconds / 3600;
        int minute = (remainingSeconds % 3600) / 60;
        int second = remainingSeconds % 60;
        // Populate the tm struct
        resultTime.tm_sec = second;
        resultTime.tm_min = minute;
        resultTime.tm_hour = hour;
        resultTime.tm_mday = day;
        resultTime.tm_mon = month - 1;
        resultTime.tm_year = year - 1900;
    }






        int convertfiotime(long long t, struct tm *tm) {
        long long days, secs;
        int remdays, remsecs, remyears;
        int qc_cycles, c_cycles, q_cycles;
        int years, months;
        int wday, yday, leap;
        static const char days_in_month[] = {31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 29};

        /* Reject time_t values whose year would overflow int */
        if (t < INT_MIN * 31622400LL || t > INT_MAX * 31622400LL)
            return -1;

        secs = t - LEAPOCH;
        days = secs / 86400;
        remsecs = secs % 86400;
        if (remsecs < 0) {
            remsecs += 86400;
            days--;
        }

        wday = (3 + days) % 7;
        if (wday < 0) wday += 7;

        qc_cycles = days / DAYS_PER_400Y;
        remdays = days % DAYS_PER_400Y;
        if (remdays < 0) {
            remdays += DAYS_PER_400Y;
            qc_cycles--;
        }

        c_cycles = remdays / DAYS_PER_100Y;
        if (c_cycles == 4) c_cycles--;
        remdays -= c_cycles * DAYS_PER_100Y;

        q_cycles = remdays / DAYS_PER_4Y;
        if (q_cycles == 25) q_cycles--;
        remdays -= q_cycles * DAYS_PER_4Y;

        remyears = remdays / 365;
        if (remyears == 4) remyears--;
        remdays -= remyears * 365;

        leap = !remyears && (q_cycles || !c_cycles);
        yday = remdays + 31 + 28 + leap;
        if (yday >= 365 + leap) yday -= 365 + leap;

        years = remyears + 4 * q_cycles + 100 * c_cycles + 400 * qc_cycles;

        for (months = 0; days_in_month[months] <= remdays; months++)
            remdays -= days_in_month[months];

        if (years + 2000 > INT_MAX || years + 2000 < INT_MIN)
            return -1;

        tm->tm_year = years + 2000;
        tm->tm_mon = months + 3;
        if (tm->tm_mon >= 12) {
            tm->tm_mon -= 12;
            //tm->tm_year++;

            if(tm->tm_mon == 00){ // some unknown but that makes the 12th month 00.
                tm->tm_mon = 12;
            }
        }
        tm->tm_mday = remdays + 1;
        tm->tm_wday = wday;
        tm->tm_yday = yday;

        tm->tm_hour = remsecs / 3600;
        tm->tm_min = remsecs / 60 % 60;
        tm->tm_sec = remsecs % 60;

        return 0;
    }
}
