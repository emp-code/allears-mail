#define AEM_ACCOUNT_RESPONSE_OK 0
#define AEM_ACCOUNT_RESPONSE_VIOLATION 10

#define AEM_API_ACCOUNT_BROWSE 10
#define AEM_API_ACCOUNT_CREATE 11
#define AEM_API_ACCOUNT_DELETE 12
#define AEM_API_ACCOUNT_UPDATE 13

#define AEM_API_ADDRESS_CREATE 20
#define AEM_API_ADDRESS_DELETE 21
#define AEM_API_ADDRESS_UPDATE 22

#define AEM_API_PRIVATE_UPDATE 40
#define AEM_API_SETTING_LIMITS 50

#define AEM_MTA_GETPUBKEY 10
#define AEM_MTA_ADDMESSAGE 20

#define AEM_LEN_ACCESSKEY crypto_box_SECRETKEYBYTES
#define AEM_LEN_KEY_MASTER crypto_secretbox_KEYBYTES

#define AEM_LEN_KEY_ACC crypto_box_SECRETKEYBYTES
#define AEM_LEN_KEY_API crypto_box_SECRETKEYBYTES
#define AEM_LEN_KEY_MNG crypto_secretbox_KEYBYTES
#define AEM_LEN_KEY_STI crypto_secretbox_KEYBYTES
#define AEM_LEN_KEY_STO 32

#define AEM_ADDRESSES_PER_USER 50
#define AEM_LEN_SALT_ADDR crypto_pwhash_SALTBYTES
#define AEM_LEN_PRIVATE (4096 - crypto_box_PUBLICKEYBYTES - 3 - (AEM_ADDRESSES_PER_USER * 14))

#define AEM_MAXLEN_ADDRESS 24 // 15 bytes Addr32 = 24 characters
#define AEM_MAXLEN_DOMAIN 32
#define AEM_MAXLEN_HOST 32

#define AEM_PORT_API 302
#define AEM_PORT_HTTPS 443
#define AEM_PORT_MANAGER 940
#define AEM_PORT_SMTP 25

#define AEM_USERLEVEL_MAX 3
#define AEM_USERLEVEL_MIN 0

#define AEM_FILETYPE_CSS 1
#define AEM_FILETYPE_HTM 2
#define AEM_FILETYPE_JSA 3
#define AEM_FILETYPE_JSM 4

#define AEM_INFOBYTE_ESMTP 128   // Extended protocol version
#define AEM_INFOBYTE_CMD_QUIT 64 // QUIT issued
#define AEM_INFOBYTE_CMD_RARE 32 // Rare command (NOOP/RSET etc)
#define AEM_INFOBYTE_CMD_FAIL 16 // Invalid command
#define AEM_INFOBYTE_PROTOERR 8  // Protocol violation (commands out of order etc)
