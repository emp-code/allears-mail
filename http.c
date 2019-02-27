#include <sys/socket.h>
#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "http.h"

void respond_http(const int sock) {
	char buf[1000];
	int recvLen = recv(sock, buf, 999, 0);

	const char* data =
	"HTTP/1.1 301 aem\r\n"
	"TSV: N\r\n"
	"Location: https://"AEM_DOMAIN"\r\n"
	"Content-Length: 0\r\n"
	"Connection: close\r\n"
	"\r\n";
	
	send(sock, data, 86 + AEM_LEN_DOMAIN, 0);
}
