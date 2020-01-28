#define AEM_ACCOUNT_RESPONSE_OK 0
#define AEM_ACCOUNT_RESPONSE_VIOLATION 10

#define AEM_API_ACCOUNT_BROWSE 10
#define AEM_API_ACCOUNT_CREATE 11
#define AEM_API_PRIVATE_UPDATE 50

#define AEM_LEN_ACCESSKEY crypto_box_SECRETKEYBYTES
#define AEM_LEN_KEY_ACC crypto_box_SECRETKEYBYTES
#define AEM_LEN_KEY_ADR crypto_pwhash_SALTBYTES
#define AEM_LEN_KEY_API crypto_box_SECRETKEYBYTES
#define AEM_LEN_KEY_MASTER crypto_secretbox_KEYBYTES
#define AEM_LEN_KEY_MNG crypto_secretbox_KEYBYTES
#define AEM_LEN_KEY_STO 32
#define AEM_LEN_PRIVATE (4096 - crypto_box_PUBLICKEYBYTES - 5)
#define AEM_MAXLEN_DOMAIN 32
#define AEM_MAXLEN_HOST 32
#define AEM_PORT_API 302
#define AEM_PORT_HTTPS 443
#define AEM_PORT_MANAGER 940
#define AEM_USERLEVEL_MAX 3
#define AEM_USERLEVEL_MIN 0

#define AEM_FILETYPE_CSS 1
#define AEM_FILETYPE_HTM 2
#define AEM_FILETYPE_JSA 3
#define AEM_FILETYPE_JSM 4
