#ifndef AEM_HTTPS_COMMON_H
#define AEM_HTTPS_COMMON_H

#include <mbedtls/ssl.h>

void sendData(mbedtls_ssl_context * const ssl, const void * const data, const size_t lenData);

#endif
