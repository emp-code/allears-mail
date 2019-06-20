#ifndef AEM_DATABASE_H
#define AEM_DATABASE_H

int getPublicKeyFromAddress(const unsigned char addr[18], unsigned char pk[32], const unsigned char hashKey[16], int *memberLevel);

int addUserMessage(const unsigned char ownerPk[32], const unsigned char *msgData, const size_t msgLen);

unsigned char *getUserInfo(const unsigned char pk[32], uint8_t *level, uint16_t *addrDataSize);
unsigned char *getUserMessages(const unsigned char pk[32], int *msgCount, const size_t maxSize);

#endif
