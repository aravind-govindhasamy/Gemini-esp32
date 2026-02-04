#ifndef APP_HTTP_CLIENT_H
#define APP_HTTP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the HTTP client task to push sensor data to the Hub.
 */
void app_http_client_start(void);

#ifdef __cplusplus
}
#endif

#endif // APP_HTTP_CLIENT_H
