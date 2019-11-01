#include <string.h>
#include <stdio.h>

#include <mbedtls/ssl.h>
#include <sodium.h>

#include "https_get.h"

#include "Include/https_common.h"
#include "aem_file.h"
#include "global.h"

#define AEM_FILETYPE_CSS 1
#define AEM_FILETYPE_IMG 2
#define AEM_FILETYPE_JS  3

// Javascript, CSS, images etc
static void respondFile(mbedtls_ssl_context * const ssl, const char * const name, const size_t lenName, const int fileType, const struct aem_file * const files, const int fileCount) {
	int reqNum = -1;

	for (int i = 0; i < fileCount; i++) {
		if (strlen(files[i].filename) == lenName && memcmp(files[i].filename, name, lenName) == 0) {
			reqNum = i;
			break;
		}
	}

	if (reqNum < 0) return;

	if (files[reqNum].lenData > 999999) return;

	const char *mediatype;
	int mtLen;
	switch (fileType) {
		case AEM_FILETYPE_IMG:
			mediatype = "image/png";
			mtLen = 9;
			break;
		case AEM_FILETYPE_JS:
			mediatype = "application/javascript; charset=utf-8";
			mtLen = 37;
			break;
		case AEM_FILETYPE_CSS:
			mediatype = "text/css; charset=utf-8";
			mtLen = 23;
			break;
		default:
			return;
	}

	char data[357 + mtLen + files[reqNum].lenData];
	sprintf(data,
		"HTTP/1.1 200 aem\r\n"
		"Tk: N\r\n"
		"Strict-Transport-Security: max-age=99999999; includeSubDomains\r\n"
		"Expect-CT: enforce; max-age=99999999\r\n"
		"Connection: close\r\n"
		"Cache-Control: public, max-age=999, immutable\r\n" // ~15min
		"%s"
		"Content-Type: %.*s\r\n"
		"Content-Length: %zd\r\n"
		"X-Content-Type-Options: nosniff\r\n"
		"X-Robots-Tag: noindex\r\n"
		"Cross-Origin-Resource-Policy: same-origin\r\n"
		"\r\n"
	, (fileType == AEM_FILETYPE_CSS || fileType == AEM_FILETYPE_JS) ? "Content-Encoding: br\r\n" : "", mtLen, mediatype, files[reqNum].lenData);

	const size_t lenHeaders = strlen(data);
	memcpy(data + lenHeaders, files[reqNum].data, files[reqNum].lenData);

	sendData(ssl, data, lenHeaders + files[reqNum].lenData);
}

static void respondHtml(mbedtls_ssl_context * const ssl, const char * const name, const size_t lenName, const struct aem_file * const files, const int fileCount) {
	int reqNum = -1;

	for (int i = 0; i < fileCount; i++) {
		if (strlen(files[i].filename) == lenName && memcmp(files[i].filename, name, lenName) == 0) {
			reqNum = i;
			break;
		}
	}

	if (reqNum < 0) return;

	if (files[reqNum].lenData > 99999) return;

	char data[1386 + (lenDomain * 4) + files[reqNum].lenData];
	sprintf(data,
		"HTTP/1.1 200 aem\r\n"
		"Tk: N\r\n"
		"Strict-Transport-Security: max-age=99999999; includeSubDomains; preload\r\n"
		"Expect-CT: enforce; max-age=99999999\r\n"
		"Connection: close\r\n"
		"Cache-Control: public, max-age=999, immutable\r\n" // ~15min
		"Content-Encoding: br\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Content-Length: %zd\r\n"

		"Content-Security-Policy: "
			"connect-src"     " https://%.*s:7850/api/;"
			"img-src"         " https://%.*s/img/;"
			"script-src"      " https://%.*s/js/ https://cdn.jsdelivr.net/gh/google/brotli@1.0.7/js/decode.min.js https://cdnjs.cloudflare.com/ajax/libs/js-nacl/1.3.2/nacl_factory.min.js;"
			"style-src"       " https://%.*s/css/;"

			"base-uri"        " 'none';"
			"child-src"       " 'none';"
			"default-src"     " 'none';"
			"font-src"        " 'none';"
			"form-action"     " 'none';"
			"frame-ancestors" " 'none';"
			"frame-src"       " 'none';"
			"manifest-src"    " 'none';"
			"media-src"       " 'none';"
			"object-src"      " 'none';"
			"prefetch-src"    " 'none';"
			"worker-src"      " 'none';"

			"block-all-mixed-content;"
			"sandbox allow-scripts allow-same-origin;"
		"\r\n"

		"Feature-Policy: "
			"accelerometer"        " 'none';"
			"ambient-light-sensor" " 'none';"
			"autoplay"             " 'none';"
			"battery"              " 'none';"
			"camera"               " 'none';"
			"display-capture"      " 'none';"
			"document-domain"      " 'none';"
			"document-write"       " 'none';"
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
			"xr-spatial-tracking"  " 'none';"
		"\r\n"

		"Referrer-Policy: no-referrer\r\n"
		"X-Content-Type-Options: nosniff\r\n"
		"X-Frame-Options: deny\r\n"
		"X-XSS-Protection: 1; mode=block\r\n"
		"\r\n"
	, files[reqNum].lenData, (int)lenDomain, domain, (int)lenDomain, domain, (int)lenDomain, domain, (int)lenDomain, domain);

	const size_t lenHeaders = strlen(data);
	memcpy(data + lenHeaders, files[reqNum].data, files[reqNum].lenData);

	sendData(ssl, data, lenHeaders + files[reqNum].lenData);
}

void https_get(mbedtls_ssl_context * const ssl, const char * const url, const size_t lenUrl, const struct aem_fileSet * const fileSet) {
	if (lenUrl == 0) return respondHtml(ssl, "index.html", 10, fileSet->htmlFiles, fileSet->htmlCount);
	if (lenUrl > 5 && memcmp(url + lenUrl - 5, ".html", 5) == 0) return respondHtml(ssl, url, lenUrl, fileSet->htmlFiles, fileSet->htmlCount);

	if (lenUrl > 8 && memcmp(url, "css/", 4) == 0 && memcmp(url + lenUrl - 4, ".css", 4) == 0) return respondFile(ssl, url + 4, lenUrl - 4, AEM_FILETYPE_CSS, fileSet->cssFiles, fileSet->cssCount);
	if (lenUrl > 8 && memcmp(url, "img/", 4) == 0 && memcmp(url + lenUrl - 4, ".png", 4) == 0) return respondFile(ssl, url + 4, lenUrl - 4, AEM_FILETYPE_IMG, fileSet->imgFiles, fileSet->imgCount);
	if (lenUrl > 6 && memcmp(url, "js/",  3) == 0 && memcmp(url + lenUrl - 3, ".js",  3) == 0) return respondFile(ssl, url + 3, lenUrl - 3, AEM_FILETYPE_JS,  fileSet->jsFiles,  fileSet->jsCount);
}

void https_mtasts(mbedtls_ssl_context * const ssl) {
	char data[317 + lenDomain];
	sprintf(data,
		"HTTP/1.1 200 aem\r\n"
		"Tk: N\r\n"
		"Strict-Transport-Security: max-age=99999999; includeSubDomains\r\n"
		"Expect-CT: enforce; max-age=99999999\r\n"
		"Connection: close\r\n"
		"Content-Type: text/plain; charset=utf-8\r\n"
		"Content-Length: %zd\r\n"
		"X-Content-Type-Options: nosniff\r\n"
		"X-Robots-Tag: noindex\r\n"
		"\r\n"
		"version: STSv1\n"
		"mode: enforce\n"
		"mx: %.*s\n"
		"max_age: 31557600"
	, 51 + lenDomain, (int)lenDomain, domain);

	sendData(ssl, data, 316 + lenDomain);
}

void https_robots(mbedtls_ssl_context * const ssl) {
	sendData(ssl,
		"HTTP/1.1 200 aem\r\n"
		"Tk: N\r\n"
		"Strict-Transport-Security: max-age=99999999; includeSubDomains\r\n"
		"Expect-CT: enforce; max-age=99999999\r\n"
		"Connection: close\r\n"
		"Cache-Control: public, max-age=9999999, immutable\r\n"
		"Content-Type: text/plain; charset=utf-8\r\n"
		"Content-Length: 84\r\n"
		"X-Content-Type-Options: nosniff\r\n"
		"X-Robots-Tag: noindex\r\n"
		"\r\n"
		"User-agent: *\n"
		"Disallow: /.well-known/\n"
		"Disallow: /css/\n"
		"Disallow: /js/\n"
		"Disallow: /img/"
	, 400);
}

// Tracking Status Resource for DNT
void https_tsr(mbedtls_ssl_context * const ssl) {
	sendData(ssl,
		"HTTP/1.1 200 aem\r\n"
		"Tk: N\r\n"
		"Strict-Transport-Security: max-age=99999999; includeSubDomains\r\n"
		"Expect-CT: enforce; max-age=99999999\r\n"
		"Connection: close\r\n"
		"Cache-Control: public, max-age=9999999, immutable\r\n"
		"Content-Type: application/tracking-status+json\r\n"
		"Content-Length: 17\r\n"
		"X-Content-Type-Options: nosniff\r\n"
		"\r\n"
		"{\"tracking\": \"N\"}"
	, 317);
}
