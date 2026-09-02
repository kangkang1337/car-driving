#include <stdio.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"

#define CLOCK_STACK_SIZE (1024 * 4)
#define CLOCK_DELAY_US 1000000
#define BEIJING_UTC_OFFSET_HOUR 8
#define SECONDS_PER_DAY 86400
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_MINUTE 60

static void PrintBeijingClock(unsigned int totalSeconds)
{
    unsigned int hour;
    unsigned int minute;
    unsigned int second;

    totalSeconds %= SECONDS_PER_DAY;
    hour = totalSeconds / SECONDS_PER_HOUR;
    minute = (totalSeconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
    second = totalSeconds % SECONDS_PER_MINUTE;

    printf("\rUTC+8 Beijing Time: %02u:%02u:%02u", hour, minute, second);
}

static void BeijingClockTask(void)
{
    unsigned int seconds = BEIJING_UTC_OFFSET_HOUR * SECONDS_PER_HOUR;

    while (1) {
        PrintBeijingClock(seconds);
        seconds = (seconds + 1) % SECONDS_PER_DAY;
        usleep(CLOCK_DELAY_US);
    }
}

static void BeijingClockEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "BeijingClock";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = CLOCK_STACK_SIZE;
    attr.priority = 25;

    if (osThreadNew((osThreadFunc_t)BeijingClockTask, NULL, &attr) == NULL) {
        printf("Failed to create BeijingClock!\r\n");
    }
}

APP_FEATURE_INIT(BeijingClockEntry);
