#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sodium.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/xtea.h"

#include "aem_file.h"
#include "defines.h"

#include "Includes/Base64.h"
#include "Includes/SixBit.h"

#include "https.h"

#define AEM_FILETYPE_CSS 1
#define AEM_FILETYPE_IMG 2
#define AEM_FILETYPE_JS  3

#define AEM_HTTPS_BUFLEN 1000
#define AEM_NETINT_BUFLEN 1000

#define AEM_NONCE_TIMEDIFF_MAX 30

#define AEM_SERVER_KEY_SEED "TestServer0123456789012345678901"

#define AEM_MSG_HEADSIZE 37

static void sendData(mbedtls_ssl_context* ssl, const char* data, const size_t lenData) {
	size_t sent = 0;

	while (sent < lenData) {
		int ret;
		do {ret = mbedtls_ssl_write(ssl, (unsigned char*)(data + sent), (lenData - sent > AEM_NETINT_BUFLEN) ? AEM_NETINT_BUFLEN : lenData - sent);}
		while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

		if (ret < 0) {
			printf("ERROR: Failed transfer: %d\n", ret);
			return;
		}

		sent += ret;
	}
}

static void respond_https_home(mbedtls_ssl_context *ssl) {
	int fd = open("aem-web.html", O_RDONLY);
	if (fd < 0) return;

	const size_t lenHtml = lseek(fd, 0, SEEK_END);
	if (lenHtml < 10 || lenHtml > 99999) {close(fd); return;}

	char headers[1069 + AEM_LEN_DOMAIN * 4];
	sprintf(headers,
		"HTTP/1.1 200 aem\r\n"
		"Tk: N\r\n"
		"Strict-Transport-Security: max-age=94672800; includeSubDomains\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Content-Length: %zd\r\n"

		"Content-Security-Policy:"
			"connect-src"     " https://"AEM_DOMAIN"/web/;"
			"img-src"         " https://"AEM_DOMAIN"/img/;"
			"script-src"      " https://"AEM_DOMAIN"/js/;"
			"style-src"       " https://"AEM_DOMAIN"/css/;"

			"base-uri"        " 'none';"
			"child-src"       " 'none';"
			"default-src"     " 'none';"
			"font-src"        " 'none';"
			"form-action"     " 'none';"
			"frame-ancestors" " 'none';"
			"frame-src"       " 'none';"
			"manifest-src"    " 'none';"
			"media-src"       " 'none';"
			"navigate-to"     " 'none';" // Use * to allow links
			"object-src"      " 'none';"
			"prefetch-src"    " 'none';"
			"worker-src"      " 'none';"

			"block-all-mixed-content;"
			"sandbox allow-scripts;"
		"\r\n"

		"Feature-Policy:"
			"autoplay"             " 'none';"
			"accelerometer"        " 'none';"
			"ambient-light-sensor" " 'none';"
			"camera"               " 'none';"
			"cookie"               " 'none';"
			"display-capture"      " 'none';"
			"document-domain"      " 'none';"
			"docwrite"             " 'none';"
			"encrypted-media"      " 'none';"
			"fullscreen"           " 'none';"
			"geolocation"          " 'none';"
			"gyroscope"            " 'none';"
			"magnetometer"         " 'none';"
			"microphone"           " 'none';"
			"midi"                 " 'none';"
			"payment"              " 'none';"
			"picture-in-picture"   " 'none';"
			"speaker"              " 'none';"
			"sync-xhr"             " 'none';"
			"usb"                  " 'none';"
			"vr"                   " 'none';"
		"\r\n"

		"Referrer-Policy: no-referrer\r\n"
		"X-Content-Type-Options: nosniff\r\n"
		"X-XSS-Protection: 1; mode=block\r\n"
		"\r\n"
	, lenHtml);
	const size_t lenHeaders = strlen(headers);
//	printf("LenHeaders=%zd\n", lenHeaders - AEM_LEN_DOMAIN * 4);

	char data[lenHeaders + lenHtml];
	memcpy(data, headers, lenHeaders);

	const int bytesRead = pread(fd, data + lenHeaders, lenHtml, 0);
	close(fd);

	if (bytesRead != lenHtml) return;

	sendData(ssl, data, lenHeaders + lenHtml);
}

// Javascript, CSS, images etc
static void respond_https_file(mbedtls_ssl_context *ssl, const char *reqName, const int fileType, struct aem_file files[], const int fileCount) {
	int reqNum = -1;

	for (int i = 0; i < fileCount; i++) {
		if (strcmp(files[i].filename, reqName) == 0) reqNum = i;
	}

	if (reqNum < 0) return;

	char *mediatype;
	int mtLen;
	switch (fileType) {
		case AEM_FILETYPE_JS:
			mediatype = "application/javascript; charset=utf-8";
			mtLen = 37;
			break;
		case AEM_FILETYPE_CSS:
			mediatype = "text/css; charset=utf-8";
			mtLen = 23;
			break;
	}

	char headers[205 + mtLen];
	sprintf(headers,
		"HTTP/1.1 200 aem\r\n"
		"Tk: N\r\n"
		"Strict-Transport-Security: max-age=94672800; includeSubDomains\r\n"
		"Connection: close\r\n"
		"%s"
		"Content-Type: %.*s\r\n"
		"Content-Length: %zd\r\n"
		"X-Content-Type-Options: nosniff\r\n"
		"\r\n"
	, (fileType == AEM_FILETYPE_CSS || fileType == AEM_FILETYPE_JS) ? "Content-Encoding: br\r\n" : "", mtLen, mediatype, files[reqNum].lenData);

	const size_t lenHeaders = strlen(headers);

	char data[lenHeaders + files[reqNum].lenData];
	memcpy(data, headers, lenHeaders);
	memcpy(data + lenHeaders, files[reqNum].data, files[reqNum].lenData);

	sendData(ssl, data, lenHeaders + files[reqNum].lenData);
}

// Tracking Status Resource for DNT
static void respond_https_tsr(mbedtls_ssl_context *ssl) {
	const char data[] =
	"HTTP/1.1 200 aem\r\n"
	"Tk: N\r\n"
	"Strict-Transport-Security: max-age=94672800; includeSubDomains\r\n"
	"Connection: close\r\n"
	"Content-Type: application/tracking-status+json\r\n"
	"Content-Length: 16\r\n"
	"\r\n"
	"{\"tracking\": \"N\"}";

	sendData(ssl, data, 195);
}

// robots.txt
static void respond_https_robots(mbedtls_ssl_context *ssl) {
	const char* data =
	"HTTP/1.1 200 aem\r\n"
	"Tk: N\r\n"
	"Strict-Transport-Security: max-age=94672800; includeSubDomains\r\n"
	"Connection: close\r\n"
	"Content-Type: text/plain; charset=utf-8\r\n"
	"Content-Length: 26\r\n"
	"\r\n"
	"User-agent: *\r\n"
	"Disallow: /";

	sendData(ssl, data, 197);
}

static void encryptNonce(unsigned char nonce[24], const unsigned char seed[16]) {
	// Nonce is encrypted to protect against leaking server time etc
	// One-way encryption (hashing) would work, but TEA guarantees no collision risk
	mbedtls_xtea_context tea;
	mbedtls_xtea_init(&tea);
	mbedtls_xtea_setup(&tea, seed);

	unsigned char nonce_encrypted[24];
	mbedtls_xtea_crypt_ecb(&tea, MBEDTLS_XTEA_ENCRYPT, nonce, nonce_encrypted); // Bytes 1-8
	mbedtls_xtea_crypt_ecb(&tea, MBEDTLS_XTEA_ENCRYPT, nonce + 8, nonce_encrypted + 8); // Bytes 9-16
	mbedtls_xtea_crypt_ecb(&tea, MBEDTLS_XTEA_ENCRYPT, nonce + 16, nonce_encrypted + 16); // Bytes 17-24
	memcpy(nonce, nonce_encrypted, 24);
}

static char *userPath(const char *b64_upk, const char *filename) {
	if (filename == NULL) return NULL;

	char *path = malloc(55 + strlen(filename));
	memcpy(path, "UserData/", 9);

	for (int i = 0; i < 44; i++) {
		if (b64_upk[i] == '/')
			path[9 + i] = '-';
		else
			path[9 + i] = b64_upk[i];
	}

	path[53] = '/';
	strcpy(path + 54, filename);

	return path;
}

char *loadUserAddressList(const char *b64_upk, const char *filename, int *count) {
	char *path = userPath(b64_upk, filename);
	const int fd = open(path, O_RDONLY);

	const off_t sz = lseek(fd, 0, SEEK_END);
	if (sz % 16 != 0) {close(fd); return NULL;}

	char *data = malloc(sz);
	const ssize_t bytesDone = pread(fd, data, sz, 0);
	close(fd);
	free(path);

	if (bytesDone != sz) {free(data); return NULL;}

	*count = sz / 16;
	return data;
}

static unsigned char *loadUserMessages(const char *b64_upk, size_t *totalSize) {
	// TODO: Load all messages in the directory
	char *path = userPath(b64_upk, "msg/test.aem");
	const int fd = open(path, O_RDONLY);

	const off_t sz = lseek(fd, 0, SEEK_END);
	const size_t msgLen = sz - AEM_MSG_HEADSIZE - (crypto_box_SEALBYTES * 2); // Length of decrypted Body part

	if ((msgLen - 2) % 1024 != 0) {close(fd); return NULL;}
	int sizeFactor = ((msgLen - 2) / 1024) - 1; // 0 = 1KiB, 255=256KiB
	if (sizeFactor > 255) {close(fd); return NULL;}

	unsigned char *data = malloc(sz + 1);
	const unsigned char sf = sizeFactor;
	data[0] = sf;
	const ssize_t bytesDone = pread(fd, data + 1, sz, 0);
	close(fd);
	free(path);

	if (bytesDone != sz) {free(data); return NULL;}

	*totalSize = sz + 1;
	return data;
}

static int numDigits(double number) {
	int digits = 0;
	while (number > 1) {number /= 10; digits++;}
	return digits;
}

// Web login
static void respond_https_login(mbedtls_ssl_context *ssl, const char *url, const size_t lenUrl, const uint32_t clientIp, const unsigned char seed[16]) {
	const char *b64_upk = url + 10;
	char* end = strchr(b64_upk, '.');
	if (end == NULL) return;
	const size_t b64_upk_len = end - b64_upk;
	if (b64_upk_len != 44) return;

	// Get nonce
	char *path = userPath(b64_upk, "nonce");
	int fd = open(path, O_RDONLY);
	unsigned char nonce[24];
	ssize_t bytesDone = read(fd, nonce, 24);
	close(fd);
	free(path);
	if (bytesDone != 24) return;

	memcpy(nonce, &clientIp, 4); // Box will not open if current IP differs from the one that requested the nonce

	int32_t ts;
	memcpy(&ts, nonce + 20, 4);
	int timeDiff = (int)time(NULL) - ts;
	if (timeDiff < 0 || timeDiff > AEM_NONCE_TIMEDIFF_MAX) return;

	encryptNonce(nonce, seed);

	// Prepare to open Box
	const char *b64_bd = end + 1;
	const size_t b64_bd_len = (url + lenUrl) - b64_bd;

	size_t pkUserLen = 0, boxDataLen = 0;
	unsigned char *pkUser = b64Decode((unsigned char*)b64_upk, b64_upk_len, &pkUserLen);
	unsigned char *boxData = b64Decode((unsigned char*)b64_bd, b64_bd_len, &boxDataLen);

	if (pkUser == NULL || boxData == NULL || pkUserLen != 32 || boxDataLen != 33) {
		if (pkUser != NULL) free(pkUser);
		if (boxData != NULL) free(boxData);
		return;
	}

	unsigned char *pkServer = malloc(32);
	unsigned char *skServer = malloc(32);
	crypto_box_seed_keypair(pkServer, skServer, (unsigned char*)AEM_SERVER_KEY_SEED);
	free(pkServer);

	// Open the Box
	unsigned char decrypted[18];
	const int ret = crypto_box_open_easy(decrypted, boxData, 33, nonce, pkUser, skServer);
	free(skServer);
	free(pkUser);
	free(boxData);

	if (ret != 0 || strncmp((char*)(decrypted), "AllEars:Web.Login", 17) != 0) {puts("Login Fail"); return;}

	// Login successful
	int addrCountNormal, addrCountShield;
	char *addrNormal = loadUserAddressList(b64_upk, "address_normal.aea", &addrCountNormal);
	if (addrNormal == NULL) return;
	char *addrShield = loadUserAddressList(b64_upk, "address_shield.aea", &addrCountShield);
	if (addrShield == NULL) {free(addrNormal); return;}

	size_t lenMbSet;;
	unsigned char *mbSet = loadUserMessages(b64_upk, &lenMbSet);
	if (mbSet == NULL) {free(addrNormal); free(addrShield); return;}

/*
	Login Response Format:
		[1B] Number of Normal Addresses
		[1B] Number of Shield Addresses
		[1B] Number of Message Boxes
		[16B] Normal Address (21B SixBit-Encoded)
		...
		[16B] Shield Address (21B SixBit-Encoded)
		...
		MessageBoxes
*/

	const size_t szBody = 3 + (16 * addrCountNormal) + (16 * addrCountShield) + lenMbSet;
	const size_t szHead = 141 + numDigits(szBody);
	const size_t szResponse = szHead + szBody;

	char data[szResponse + 1];
	sprintf(data,
		"HTTP/1.1 200 aem\r\n"
		"Tk: N\r\n"
		"Strict-Transport-Security: max-age=94672800; includeSubDomains\r\n"
		"Content-Length: %zd\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"\r\n"
	, szBody);

	data[szHead + 0] = (unsigned char)addrCountNormal;
	data[szHead + 1] = (unsigned char)addrCountShield;
	data[szHead + 2] = (unsigned char)1; // MsgCount
	memcpy(data + szHead + 3, addrNormal, addrCountNormal * 16);
	memcpy(data + szHead + 3 + (16 * addrCountNormal), addrShield, addrCountShield * 16);
	memcpy(data + szHead + 3 + (16 * addrCountNormal) + (16 * addrCountShield), mbSet, lenMbSet);

	free(addrNormal);
	free(addrShield);
	free(mbSet);

	sendData(ssl, data, szResponse);
}

// Request for a nonce to be used with a NaCl Box. URL format: name.tld/web/nonce/public-key-in-base64
static void respond_https_nonce(mbedtls_ssl_context *ssl, const char *b64_upk, const uint32_t clientIp, const unsigned char seed[16]) {
	int fd = open("/dev/urandom", O_RDONLY);
	unsigned char nonce_random[16];
	ssize_t bytesDone = read(fd, nonce_random, 16);
	close(fd);
	if (bytesDone != 16) return;

	unsigned char nonce[24];

	const uint32_t ts = (uint32_t)time(NULL);
	memcpy(nonce, &clientIp, 4); // Client IP. Protection against third parties intercepting the Box.
	memcpy(nonce + 4, nonce_random, 16);
	memcpy(nonce + 20, &ts, 4); // Timestamp. Protection against replay attacks.

// Store nonce in user folder
	char *path = userPath(b64_upk, "nonce");
	fd = open(path, O_WRONLY | O_TRUNC);
	bytesDone = write(fd, nonce, 24);
	close(fd);
	free(path);
	if (bytesDone != 24) return;

	encryptNonce(nonce, seed);

	char data[220];
	sprintf(data,
		"HTTP/1.1 200 aem\r\n"
		"Tk: N\r\n"
		"Strict-Transport-Security: max-age=94672800; includeSubDomains\r\n"
		"Connection: close\r\n"
		"Content-Length: 24\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"X-Content-Type-Options: nosniff\r\n"
		"\r\n"
		"%.24s"
	, nonce);

	sendData(ssl, data, 219);
}

static void handleRequest(mbedtls_ssl_context *ssl, const char *clientHeaders, const size_t chLen, const uint32_t clientIp, const unsigned char seed[16], struct aem_fileSet *fileSet) {
	if (chLen < 14 || memcmp(clientHeaders, "GET /", 5) != 0) return;

	const char *url = clientHeaders + 5;

	char* end = strstr(url, "\r\n\r\n");
	if (end == NULL) return;
	*(end + 2) = '\0';

	if (strstr(url, "\r\nHost: "AEM_DOMAIN"\r\n") == NULL) return;

	end = strpbrk(url, "\r\n");
	if (end == NULL) return;

	if (memcmp(end - 9, " HTTP/1.1", 9) != 0) return;
	*(end - 9) = '\0';

	const size_t urlLen = (end - 9) - url;

	// Static
	if (urlLen == 0) return respond_https_home(ssl);
	if (urlLen == 15 && memcmp(url, ".well-known/dnt", 15) == 0) return respond_https_tsr   (ssl);
	if (urlLen == 10 && memcmp(url, "robots.txt",      10) == 0) return respond_https_robots(ssl);

	// Files
	if (urlLen  >  4 && memcmp(url, "css/", 4) == 0) return respond_https_file(ssl, url + 4, AEM_FILETYPE_CSS, fileSet->cssFiles, fileSet->cssCount);
	if (urlLen  >  4 && memcmp(url, "img/", 4) == 0) return respond_https_file(ssl, url + 4, AEM_FILETYPE_IMG, fileSet->imgFiles, fileSet->imgCount);
	if (urlLen  >  3 && memcmp(url, "js/",  3) == 0) return respond_https_file(ssl, url + 3, AEM_FILETYPE_JS,  fileSet->jsFiles,  fileSet->jsCount);

	// Ajax
	if (urlLen  > 10 && memcmp(url, "web/login/", 10) == 0) return respond_https_login (ssl, url, urlLen, clientIp, seed);
	if (urlLen == 54 && memcmp(url, "web/nonce/", 10) == 0) return respond_https_nonce (ssl, url + 10, clientIp, seed);
}

int respond_https(int sock, mbedtls_x509_crt* srvcert, mbedtls_pk_context* pkey, const uint32_t clientIp, const unsigned char seed[16], struct aem_fileSet *fileSet) {
	// Setting up the SSL
	mbedtls_ssl_config conf;
	mbedtls_ssl_config_init(&conf);

	int ret;
	if ((ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		printf( "Failed; mbedtls_ssl_config_defaults returned %d\n\n", ret);
	}

	// Seed the RNG
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ctr_drbg_init(&ctr_drbg);

	mbedtls_entropy_context entropy;
	mbedtls_entropy_init(&entropy);

	if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, seed, 16)) != 0) {
		printf( "ERROR: mbedtls_ctr_drbg_seed returned %d\n", ret);
		return -1;
	}

	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

	mbedtls_ssl_conf_ca_chain(&conf, srvcert->next, NULL);
	if ((ret = mbedtls_ssl_conf_own_cert(&conf, srvcert, pkey)) != 0) {
		printf("ERROR: mbedtls_ssl_conf_own_cert returned %d\n", ret);
		return -1;
	}

	mbedtls_ssl_context ssl;
	mbedtls_ssl_init(&ssl);

	if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
		printf( "ERROR: mbedtls_ssl_setup returned %d\n", ret);
		return -1;
	}

	mbedtls_ssl_set_bio(&ssl, &sock, mbedtls_net_send, mbedtls_net_recv, NULL);

	// Handshake
	while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			char error_buf[100];
			mbedtls_strerror(ret, error_buf, 100);
			printf( "ERROR: mbedtls_ssl_handshake returned %d: %s\n", ret, error_buf);
			mbedtls_ssl_free(&ssl);
			return -1;
		}
	}

	unsigned char req[AEM_HTTPS_BUFLEN + 1];
	bzero(req, AEM_HTTPS_BUFLEN);

	// Read request
	do {ret = mbedtls_ssl_read(&ssl, req, AEM_HTTPS_BUFLEN);}
		while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

	if (ret > 0) {
		handleRequest(&ssl, (char*)req, ret, clientIp, seed, fileSet);
	} else if (ret < 0 && ret != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY && ret != MBEDTLS_ERR_SSL_CONN_EOF && ret != MBEDTLS_ERR_NET_CONN_RESET) {
		// Failed to read request
		char error_buf[100];
		mbedtls_strerror(ret, error_buf, 100);
		printf("ERROR: Incoming connection failed: %d: %s\n", ret, error_buf);
	}

	mbedtls_entropy_free(&entropy);
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_ssl_config_free(&conf);
	mbedtls_ssl_free(&ssl);
	return 0;
}
