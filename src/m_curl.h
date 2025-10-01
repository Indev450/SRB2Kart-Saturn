#ifndef __M_CURL__
#define __M_CURL__

#include <curl/curl.h>

// Applies CLI args to curl handle, such as -curl-proxy
void M_SetCURLArgs(CURL *http_handle, char *curl_errbuf);

#endif
