#include <stdint.h>
#include <stdio.h>

#include "esp_private/esp_clk.h"
#include "utils.h"
#include "custom_def.h"
#include "core_portme.h"

int coremark_main(void);

void app_main(void)
{
    const uint32_t cpu_hz = esp_clk_cpu_freq();

    while (1) {
        PRINTF("\n--- CoreMark run on ESP32-C3 Classic @ %u Hz ---\n", cpu_hz);
        coremark_main();
        PRINTF("--- CoreMark complete. %u %s ---\n", cpu_hz, COMPILER_NAME);
        for (int i = 0; i < 10; i++) {
            HAL_Delay(1000);
        }
    }
}
