#ifndef HEADER_STORAGE_H
#define HEADER_STORAGE_H

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_littlefs.h"
#include "nvs.h"

#define KEY_SSID    "ssid"
#define KEY_PASSWD  "passwd"

esp_err_t filesystem_init();
void nvs_init();
extern esp_err_t nvs_set(const char *key, char *value);
extern esp_err_t nvs_get(const char *key, char *out_value);

#endif