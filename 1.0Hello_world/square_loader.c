#include <stdio.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"

#define LOADER_STACK_SIZE (1024 * 4)
#define LOADER_INTERVAL_US 100000

static void SquareLoaderTask(void)
{
    static const char *frames[] = {
        "\033[2J\033[H"
        "+---+\r\n"
        "|*  |\r\n"
        "|   |\r\n"
        "+---+\r\n",

        "\033[2J\033[H"
        "+---+\r\n"
        "| * |\r\n"
        "|   |\r\n"
        "+---+\r\n",

        "\033[2J\033[H"
        "+---+\r\n"
        "|  *|\r\n"
        "|   |\r\n"
        "+---+\r\n",

        "\033[2J\033[H"
        "+---+\r\n"
        "|   |\r\n"
        "|  *|\r\n"
        "+---+\r\n",

        "\033[2J\033[H"
        "+---+\r\n"
        "|   |\r\n"
        "| * |\r\n"
        "+---+\r\n",

        "\033[2J\033[H"
        "+---+\r\n"
        "|   |\r\n"
        "|*  |\r\n"
        "+---+\r\n",
    };
    unsigned int index = 0;
    unsigned int frameCount = sizeof(frames) / sizeof(frames[0]);

    while (1) {
        printf("%s", frames[index]);
        index = (index + 1) % frameCount;
        usleep(LOADER_INTERVAL_US);
    }
}

static void SquareLoaderEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "SquareLoader";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = LOADER_STACK_SIZE;
    attr.priority = 25;

    if (osThreadNew((osThreadFunc_t)SquareLoaderTask, NULL, &attr) == NULL) {
        printf("Failed to create SquareLoader!\r\n");
    }
}

APP_FEATURE_INIT(SquareLoaderEntry);
