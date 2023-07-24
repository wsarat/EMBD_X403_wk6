#include <stdio.h>
#include "storage.h"
#include <wifi.h>

void app_main(void)
{
    nvs_init();
    filesystem_init();
    wifi_init();

    while( true ){

        vTaskDelay( 5000 / portTICK_PERIOD_MS );
    }
}
