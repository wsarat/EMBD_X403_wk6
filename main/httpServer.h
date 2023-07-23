#ifndef HEADER_HTTPSERVER_H
#define HEADER_HTTPSERVER_H

#include <esp_tls.h>
#include <esp_http_server.h>
#include <sys/param.h>
#include "esp_vfs.h" // ESP_VFS_PATH_MAX

#include "wifi.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_LITTLEFS_OBJ_NAME_LEN)
#define SCRATCH_BUFSIZE ( 2304 / 2 )

typedef struct {
    /* Base path of file storage */
    char base_path[ ESP_VFS_PATH_MAX + 1 ];
    /* Scratch buffer for temporary storage during file transfer */
    char scratch[ SCRATCH_BUFSIZE ];
} file_server_data_t;

static httpd_handle_t server = NULL;
static file_server_data_t *server_data = NULL;

void httpServer_start();
void httpServer_stop();

#endif