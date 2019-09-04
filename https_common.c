#include <mbedtls/ssl.h>

void sendData(mbedtls_ssl_context * const ssl, const char * const data, const size_t lenData) {
	size_t sent = 0;

	while (sent < lenData) {
		int ret;
		do {ret = mbedtls_ssl_write(ssl, (unsigned char*)(data + sent), lenData - sent);} while (ret == MBEDTLS_ERR_SSL_WANT_WRITE);
		if (ret < 0) {printf("[HTTPS] Failed to send data: %d\n", ret); return;}
		sent += ret;
	}
}
