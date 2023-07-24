#include "httpServer.h"
#include "storage.h"
#include <json_parser.h>
#include "esp_timer.h"

/* A simple example that demonstrates how to create GET and POST
 * handlers and start an HTTPS server.
*/

static const char *HTTPSERVER_TAG = "httpServer";

#pragma GCC diagnostic ignored "-Wformat-truncation"

/* An HTTP GET handler */
/*
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char page1_html[] = "Hello World";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, page1_html, strlen(page1_html));

    return ESP_OK;
}

static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};
*/
void reboot() {
    ESP_LOGE(HTTPSERVER_TAG, "Apply setting, Reboot!");
    esp_restart();
}

void reboot_delay(int sec) {
    // Create a timer to trigger the restart after 3 seconds
    esp_timer_create_args_t timer_args = {
        .callback = reboot,
        .name = "restart_timer"
    };
    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_once(timer, sec * 1000000);
}

static esp_err_t on_api_apply( httpd_req_t *req ) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "text/json");

    reboot_delay(3);

    httpd_resp_send_chunk(req, NULL, 0); // end chunk
    return ESP_OK;    
}


static esp_err_t on_api_setAP( httpd_req_t *req ) {
    char content[100];

    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }
    content[recv_size] = '\0';

    jparse_ctx_t jctx;
    char str_val[64];

	ret = json_parse_start(&jctx, content, strlen(content));
	if (ret == OS_SUCCESS) {
        if (json_obj_get_string(&jctx, "ssid", str_val, sizeof(str_val)) == OS_SUCCESS) {
            printf("ssid %s\n", str_val);
            nvs_set(KEY_SSID, str_val);
        }

        if (json_obj_get_string(&jctx, "passwd", str_val, sizeof(str_val)) == OS_SUCCESS) {
            printf("passwd %s\n", str_val);   
            nvs_set(KEY_PASSWD, str_val);  
        }
    }   

    nvs_get(KEY_SSID, str_val);
    printf("read SSID: %s\n", str_val);

   
    nvs_get(KEY_PASSWD, str_val);
    printf("read password: %s\n", str_val); 

    reboot_delay(5);


    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "text/json");

    httpd_resp_send_chunk(req, NULL, 0); // end chunk
    return ESP_OK;
}

static esp_err_t on_api_scan( httpd_req_t *req ) {
    printf("httd Request: on_api_scan");
    uint16_t ap_count = 8; //will ne changed after wifi_scan called
    wifi_ap_record_t ap_list[ap_count];
    wifi_scan(&ap_list, &ap_count);
    char buf[128];
    esp_err_t err = ESP_OK;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "text/json");

    for (int i=0; i<ap_count; i++) {
        snprintf(buf, 128, 
"%s\
{\
    \"id\" : %d,\
    \"bssid\": \"%02X:%02X:%02X:%02X:%02X:%02X\",\
    \"ssid\": \"%s\"\
}%s\
%s", 
            (i==0)? "{\"ap_list\": [\n":"", // start print { to start json data
            i,
            ap_list[i].bssid[0], ap_list[i].bssid[1], ap_list[i].bssid[2], ap_list[i].bssid[3], ap_list[i].bssid[4], ap_list[i].bssid[5],
            ap_list[i].ssid, (i+1 == ap_count)? "":",", // add comma if not the last item
            (i+1 == ap_count)? "]}":""
            );
        err = httpd_resp_send_chunk(req, buf, strlen(buf));
        ESP_LOGI(HTTPSERVER_TAG, "send chunk %d %s: %s", i, (err==ESP_OK)? "OK":"Failed", buf);
    }

    httpd_resp_send_chunk(req, NULL, 0); // end chunk
    printf("httd Request: on_api_scan : done");
    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
esp_err_t 
set_content_type_from_file(httpd_req_t *req, const char *filename) {
    
    if (IS_FILE_EXT(filename, ".pdf")){
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html") || IS_FILE_EXT(filename, ".html.gz")){
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")){
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")){
        return httpd_resp_set_type(req, "image/x-icon");
    } else if (IS_FILE_EXT(filename, ".js") || IS_FILE_EXT(filename, ".js.gz")){
        return httpd_resp_set_type(req, "application/javascript");
    } else if (IS_FILE_EXT(filename, ".css") || IS_FILE_EXT(filename, ".css.gz")){
        return httpd_resp_set_type(req, "text/css");
	} else if (IS_FILE_EXT(filename, ".png")){
        return httpd_resp_set_type(req, "image/png");
    } else if (IS_FILE_EXT(filename, ".bmp")){
        return httpd_resp_set_type(req, "image/bmp");
    } else if (IS_FILE_EXT(filename, ".ico")){
        return httpd_resp_set_type(req, "image/x-icon");
    } else if (IS_FILE_EXT(filename, ".svg")){
        return httpd_resp_set_type(req, "text/xml");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

// REST URI 
/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
const char*
get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest){
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash){
        pathlen = MIN(pathlen, hash - uri);
    }
    if (base_pathlen + pathlen + 1 > destsize){
        /* Full path string won't fit into destination buffer */
        return NULL;
    }
    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

static esp_err_t on_default_url( httpd_req_t *req ) {
    char filepath[ FILE_PATH_MAX ];
    FILE *fd = NULL;
    struct stat file_stat;
    esp_err_t err = ESP_OK;

	ESP_LOGI(HTTPSERVER_TAG, "URL: %s", req->uri);
	strncpy( filepath, req->uri, sizeof( filepath ) );

	// removes base_path and REST 
	// we must copy the filename because any other call to parse the request invalidates the pointer
    const char *filename = get_path_from_uri( filepath, ((file_server_data_t *)req->user_ctx)->base_path, req->uri, sizeof(filepath));
    if( filename ){
		//ESP_LOGI( TAG, "serving file name %s, basepath \"%s\", filepath %s", filename, 
        //                    ((file_server_data_t *)req->user_ctx)->base_path, 
        //                    filepath );        
    } else {
        ESP_LOGE(HTTPSERVER_TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err( req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long" );
        return ESP_FAIL;
    }  

    // entry point for the webserver: either / or /index.html
    if( filepath[ strlen(filepath) - 1 ] == '/' ) {
		// ends with / (for directories) or is index.html
		strncpy( filepath, "/index.html", sizeof( filepath ));
	}
	if( stat( filepath, &file_stat ) == -1 && strchr(filepath, '.') == NULL ){
		// File doesn't exist and no file extension, assume .html
		strlcat( filepath, ".html", FILE_PATH_MAX );
	}
	if( stat( filepath, &file_stat ) == -1 && 
		( IS_FILE_EXT(filepath, ".html") || IS_FILE_EXT(filepath, ".js") || IS_FILE_EXT(filepath, ".css" ))){
		// File doesn't exist, but is potentially gzip'd
		strlcat( filepath, ".gz", FILE_PATH_MAX);
	}
	if( stat( filepath, &file_stat ) == -1 ) {
		ESP_LOGE(HTTPSERVER_TAG, "Failed to stat file: %s", filepath);
		// Respond with 404 Not Found 
		httpd_resp_send_err( req, HTTPD_404_NOT_FOUND, "File does not exist") ;
		return ESP_FAIL;
	}

	fd = fopen( filepath, "r" );
	if( !fd ){
		ESP_LOGE(HTTPSERVER_TAG, "Failed to read existing file : %s", filepath);
		// Respond with 500 Internal Server Error 
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
		return ESP_FAIL;
	}

    //ESP_LOGI(HTTPSERVER_TAG, "Sending file : %s (%ld bytes)...", filepath, file_stat.st_size);
    set_content_type_from_file( req, filepath );

    if( IS_FILE_EXT( filepath, ".gz" )){
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    // Retrieve the pointer to scratch buffer for temporary storage
    char *chunk = ((file_server_data_t *)req->user_ctx)->scratch;
    // first chunk
    size_t chunksize = fread( chunk, 1, SCRATCH_BUFSIZE, fd );
        
    while( chunksize > 0 ) {
        /* Send the buffer contents as HTTP response chunk */
        if(( err = httpd_resp_send_chunk(req, chunk, chunksize)) != ESP_OK) {

            fclose(fd);
            ESP_LOGE(HTTPSERVER_TAG, "File %s sending failed: %s", filepath, esp_err_to_name( err )); 
            /* Abort sending file */
            httpd_resp_sendstr_chunk( req, NULL );
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            return ESP_FAIL;
            
        } else {
            /* Read file in chunks into the scratch buffer */
            chunksize = fread( chunk, 1, SCRATCH_BUFSIZE, fd );
            //ESP_LOGI(HTTPSERVER_TAG, "chunk %d", chunksize);
        }
        vTaskDelay( 1 / portTICK_PERIOD_MS ); // must yield while transferring file data
        //ESP_LOGI(HTTPSERVER_TAG, "RAM left %d", esp_get_free_heap_size());
        /* Keep looping till the whole file is sent */
    }

    /* Close file after sending complete */
    fclose(fd);
    //ESP_LOGI(HTTPSERVER_TAG, "File sending complete");

    // Respond with an empty chunk to signal HTTP response completion
    httpd_resp_send_chunk(req, NULL, 0);
	// must yield
    vTaskDelay(10 / portTICK_PERIOD_MS); 
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    server_data = calloc( 1, sizeof( file_server_data_t ));
    strlcpy( server_data->base_path, "", sizeof( server_data->base_path ));

    // Start the httpd server
    ESP_LOGI(HTTPSERVER_TAG, "Starting server");

    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
    conf.uri_match_fn = httpd_uri_match_wildcard; // support wildcard

    esp_err_t ret = httpd_start(&server, &conf);
    if (ESP_OK != ret) {
        ESP_LOGI(HTTPSERVER_TAG, "Error starting server!");
        return NULL;
    }

    // Set URI handlers
    ESP_LOGI(HTTPSERVER_TAG, "Registering URI handlers");
    //httpd_register_uri_handler(server, &root);

    httpd_uri_t api_scan = {
        .uri = "/api/scan",
        .method = HTTP_GET,
        .handler = on_api_scan,
        //.user_ctx = server_data
    };
    httpd_register_uri_handler(server, &api_scan);

    httpd_uri_t api_setAP = {
        .uri = "/api/setAP",
        .method = HTTP_POST,
        .handler = on_api_setAP,
        //.user_ctx = server_data
    };
    httpd_register_uri_handler(server, &api_setAP);

    httpd_uri_t api_apply = {
        .uri = "/api/apply",
        .method = HTTP_GET,
        .handler = on_api_apply,
        //.user_ctx = server_data
    };
    httpd_register_uri_handler(server, &api_apply);    

    httpd_uri_t default_url = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = on_default_url,
        .user_ctx = server_data
    };
    httpd_register_uri_handler(server, &default_url);
    return server;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

void httpServer_stop() {
    if (stop_webserver(server) == ESP_OK) {
        server = NULL;
        ESP_LOGI(HTTPSERVER_TAG, "https server stoped");
    } else {
        ESP_LOGI(HTTPSERVER_TAG, "Failed to stop https server");
    }
}

void httpServer_start() {
    server = start_webserver();
    ESP_LOGI(HTTPSERVER_TAG, "https server started");
}
