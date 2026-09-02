#include <stdio.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"

#define PROGRESS_STACK_SIZE (1024 * 4)
#define PROGRESS_INTERVAL_US 100000
#define PROGRESS_BAR_WIDTH 20

static void PrintProgressBar(unsigned int percent, char spinner)
{
    unsigned int filled = percent * PROGRESS_BAR_WIDTH / 100;
    unsigned int i;

    printf("\rLoading [");
    for (i = 0; i < PROGRESS_BAR_WIDTH; i++) {
        if (i < filled) {
            printf("#");
        } else {
            printf(" ");
        }
    }
    printf("] %3u%% %c", percent, spinner);
}

static void ProgressBarTask(void)
{
    static const char spinner[] = {'|', '/', '-', '\\'};
    unsigned int percent = 0;
    unsigned int spinnerIndex = 0;

    while (1) {
        PrintProgressBar(percent, spinner[spinnerIndex]);

        percent++;
        if (percent > 100) {
            percent = 0;
            printf("\r\n");
        }

        spinnerIndex = (spinnerIndex + 1) % (sizeof(spinner) / sizeof(spinner[0]));
        usleep(PROGRESS_INTERVAL_US);
    }
}

static void ProgressBarEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "ProgressBar";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = PROGRESS_STACK_SIZE;
    attr.priority = 25;

    if (osThreadNew((osThreadFunc_t)ProgressBarTask, NULL, &attr) == NULL) {
        printf("Failed to create ProgressBar!\r\n");
    }
}

APP_FEATURE_INIT(ProgressBarEntry);
