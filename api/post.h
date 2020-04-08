#ifndef AEM_HTTPS_POST_H
#define AEM_HTTPS_POST_H

#include <sodium.h>

#define AEM_HTTPS_POST_SIZE 8192 // 8 KiB
#define AEM_HTTPS_POST_BOXED_SIZE (crypto_box_NONCEBYTES + crypto_box_PUBLICKEYBYTES + AEM_HTTPS_POST_SIZE + 2 + crypto_box_MACBYTES)

void setApiKey(const unsigned char * const newKey);
void setAccessKey_account(const unsigned char * const newKey);
void setAccessKey_storage(const unsigned char * const newKey);
void setAccountPid(const pid_t pid);
void setStoragePid(const pid_t pid);

int aem_api_init(void);
void aem_api_free(void);

int aem_api_prepare(const unsigned char * const pubkey, const bool ka);
int aem_api_process(mbedtls_ssl_context * const ssl, const char * const url, const unsigned char * const post);

#endif
