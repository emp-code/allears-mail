#include <strings.h>

#include <sodium.h>

#include "AEM_KDF.h"

// Use the 368-bit Server Master Key (SMK) with an 8-bit nonce to generate up to 16 KiB
void aem_kdf_smk(unsigned char * const out, const size_t lenOut, const uint8_t n, const unsigned char smk[AEM_KDF_SMK_KEYLEN]) {
	bzero(out, lenOut);
	crypto_stream_chacha20_ietf_xor_ic(out, out, lenOut,
	/* Nonce */ smk + 32,
	/* Counter */ (smk[44] << 24) | (smk[45] << 16) | (n << 8),
	smk);
}

// Use the 320-bit subkey with a 56-bit nonce to generate up to 16 KiB
void aem_kdf_sub(unsigned char * const out, const size_t lenOut, const uint64_t n, const unsigned char key[AEM_KDF_SUB_KEYLEN]) {
	bzero(out, lenOut);
	crypto_stream_chacha20_ietf_xor_ic(out, out, lenOut,
	/* Nonce */ (const uint8_t[]){key[32], key[33], key[34], key[35], key[36], key[37], key[38], key[39], ((const uint8_t*)&n)[0], ((const uint8_t*)&n)[1], ((const uint8_t*)&n)[2], ((const uint8_t*)&n)[3]},
	/* Counter */ (((const uint8_t*)&n)[4] << 8) | (((const uint8_t*)&n)[5] << 16) | ((unsigned int)(((const uint8_t*)&n)[6]) << 24),
	key);
}

// Get UserID from UAK
uint16_t aem_getUserId(const unsigned char uak[AEM_KDF_SUB_KEYLEN]) {
	uint16_t uid;
	aem_kdf_sub((unsigned char*)&uid, sizeof(uint16_t), AEM_KDF_KEYID_UAK_UID, uak);
	return uid & 4095;
}

#ifdef AEM_KDF_UMK
// Use the 360-bit User Master Key (UMK) with a 16-bit nonce to generate up to 16 KiB
void aem_kdf_umk(unsigned char * const out, const size_t lenOut, const uint16_t n, const unsigned char umk[AEM_KDF_UMK_KEYLEN]) {
	bzero(out, lenOut);
	crypto_stream_chacha20_ietf_xor_ic(out, out, lenOut,
	/* Nonce */ umk + 32,
	/* Counter */ (umk[44] << 24) | (n << 8),
	umk);
}
#endif
