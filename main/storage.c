#include "storage.h"

#define FS_TAG "FS"

#pragma GCC diagnostic ignored "-Wformat-truncation"
#pragma GCC diagnostic ignored "-Wformat-overflow"

// FILE SYSTEM SFIFFS, LittleFS and FATFS
// must contain / at the end for directory
void
list_files( const char* basedir )  { 
    struct dirent *de;  // Pointer for directory entry 
    struct stat entry_stat;

    ESP_LOGI( FS_TAG, "opening %s", basedir );
    // opendir() returns a pointer of DIR type.  
    DIR *dr = opendir( basedir ); 
  
    if (dr == NULL){  // opendir returns NULL if couldn't open directory 
        ESP_LOGE(FS_TAG, "Could not open current directory" ); 
    } 
  
    // Refer http://pubs.opengroup.org/onlinepubs/7990989775/xsh/readdir.html 
    // for readdir() 
    while((de = readdir(dr)) != NULL) {
        char entrypath[ 2*CONFIG_LITTLEFS_OBJ_NAME_LEN ];

        if( de->d_name[0] == '/' ){
            sprintf( entrypath, "%s%s", basedir, &de->d_name[1] ); // remove beginning '/'
        } else {
            sprintf( entrypath, "%s%s", basedir, de->d_name );
        }
        // create a full path from the entry
        snprintf( entrypath, sizeof(entrypath), "%s/%s", basedir, de->d_name );
        //ESP_LOGI( FS_TAG, "%s", entrypath ); 
        if( stat( entrypath, &entry_stat ) != 0 ) {
            ESP_LOGE( FS_TAG, "Failed to stat %s, errno %d", entrypath, errno );
            // somehow not able to open, ignore
            continue;
        }
        if( S_ISDIR( entry_stat.st_mode )) {
            if (strcmp(".", de->d_name) == 0 || strcmp("..", de->d_name) == 0) {
                continue;
            }
            // recursive
            list_files( entrypath );
        } else {        
            ESP_LOGI( FS_TAG, "Found %s (%ld bytes)", de->d_name, entry_stat.st_size );
        }

    }
    closedir(dr);     
} 

esp_err_t  filesystem_init() {
    esp_err_t ret;

    // 1: Main file system
    ESP_LOGI(FS_TAG, "Initializing LittelFS");
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "",
        .partition_label = "image",
        .format_if_mount_failed = false,
        .dont_mount = false,
    };

    // Use settings defined above to initialize and mount LittleFS filesystem.
    // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
    if(( ret = esp_vfs_littlefs_register( &conf ))!= ESP_OK) {
        if (ret == ESP_FAIL) {
                ESP_LOGE(FS_TAG, "Failed to mount or format LittleFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
                ESP_LOGE(FS_TAG, "Failed to find LittleFS partition");
        } else {
                ESP_LOGE(FS_TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
            ESP_LOGE(FS_TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    } else {
            ESP_LOGI(FS_TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // debug list all files
    // careful, this is a recursive function, requires BIG stack space in CONFIG_ESP_MAIN_TASK_STACK_SIZE
    ESP_LOGI(FS_TAG, "files:"); list_files( "/" );        

    // 2: mount SDCARD file system
    // if( sdcard_init() != ESP_OK ){
    //     ESP_LOGE( FS_TAG, "no SD card available");
    // } 
    return ESP_OK;
}

#define STORAGE_NAMESPACE "WIFI_STORAGE"

esp_err_t nvs_set(const char *key, char *value) {
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) { 
        ESP_LOGE(FS_TAG, "nvs_open %s error",STORAGE_NAMESPACE ); 
        return err;
    }

    err= nvs_set_str(my_handle, key, value);
    if (err != ESP_OK) { 
        ESP_LOGE(FS_TAG, "nvs_get_str for key:%s error", key ); 
        return err;
    }
    
    return err;    
}

esp_err_t nvs_get(const char *key, char *out_value) {
    nvs_handle_t my_handle;
    esp_err_t err;
    size_t required_size = 32;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) { 
        ESP_LOGE(FS_TAG, "nvs_open %s error",STORAGE_NAMESPACE ); 
        return err;
    }

    err= nvs_get_str(my_handle, key, out_value, &required_size);
    //printf("get %s: %s\n", key, out_value);
    if (err != ESP_OK) { 
        ESP_LOGE(FS_TAG, "nvs_get_str for key:%s error", key ); 
        return err;
    }

    return err;    
}

void nvs_init() {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}