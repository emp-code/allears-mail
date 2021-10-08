#include <string.h>
#include <ctype.h> // for isupper/tolower
#include <sys/types.h> // for ssize_t

#include "../Global.h"
#include "memeq.h"
#include "HtmlRefs.h"
#include "Trim.h"

#include "HtmlToText.h"

static void convertChar(char * const text, const size_t lenText, const char from, const char to) {
	while(1) {
		char * const c = memchr(text, from, lenText);
		if (c == NULL) return;
		*c = to;
	}
}

static void lfToSpace(char * const text, const size_t len) {
	char *c = memchr(text, '\n', len);

	while (c != NULL) {
		*c = ' ';

		const size_t skip = c - text;
		c = memchr(text + skip, '\n', len - skip);
	}
}

static void bracketsInQuotes_single(const char * const br1, char ** const br2) {
	const char *qt1 = strchr(br1, '\'');
	while (qt1 != NULL && qt1 < *br2) {
		const char * const qt2 = strchr(qt1 + 1, '\'');
		if (qt2 == NULL) break;

		while (*br2 < qt2) {
			*br2 = strchr(qt2 + 1, '>');
			if (*br2 == NULL) return;
		}

		while(1) {
			char * const c = memchr(qt1 + 1, '<', qt2 - (qt1 + 1));
			if (c == NULL) break;
			*c = AEM_HTML_PLACEHOLDER_LT;
		}

		while(1) {
			char * const c = memchr(qt1 + 1, '>', qt2 - (qt1 + 1));
			if (c == NULL) break;
			*c = AEM_HTML_PLACEHOLDER_GT;
		}

		// br2 is now beyond the quote character, look for next quote
		qt1 = strchr(qt2 + 1, '\'');
		if (qt1 == NULL || qt1 > *br2) break;
	}
}

static void bracketsInQuotes_double(const char * const br1, char ** const br2) {
	const char *qt1 = strchr(br1, '"');
	while (qt1 != NULL && qt1 < *br2) {
		const char * const qt2 = strchr(qt1 + 1, '"');
		if (qt2 == NULL) break;

		while (*br2 < qt2) {
			*br2 = strchr(qt2 + 1, '>');
			if (*br2 == NULL) return;
		}

		while(1) {
			char * const c = memchr(qt1 + 1, '\'', qt2 - (qt1 + 1));
			if (c == NULL) break;
			*c = AEM_HTML_PLACEHOLDER_SINGLEQUOTE;
		}

		while(1) {
			char * const c = memchr(qt1 + 1, '<', qt2 - (qt1 + 1));
			if (c == NULL) break;
			*c = AEM_HTML_PLACEHOLDER_LT;
		}

		while(1) {
			char * const c = memchr(qt1 + 1, '>', qt2 - (qt1 + 1));
			if (c == NULL) break;
			*c = AEM_HTML_PLACEHOLDER_GT;
		}

		// br2 is now beyond the quote character, look for next quote
		qt1 = strchr(qt2 + 1, '"');
		if (qt1 == NULL || qt1 > *br2) break;
	}
}

// 1. Look for double quotes
// 2. Locate single quotes within double quotes, change them into a placeholder character
// 3. Locate angle brackets, change them into placeholders
// 4. Look for single quotes
// 5. Repeat step 3
// 6. Convert single quotes to double quotes (src='abc' -> src="abc")
static void bracketsInQuotes(char *text) {
	char *br1 = strchr(text, '<');

	while (br1 != NULL) {
		char *br2 = strchr(br1 + 1, '>');
		if (br2 == NULL) break;

		bracketsInQuotes_double(br1, &br2);
		bracketsInQuotes_single(br1, &br2);
		bracketsInQuotes_double(br1, &br2);

		if (br2 == NULL) break;
		convertChar(br1, br2 - br1, '\'', '"');

		br1 = strchr(br2 + 1, '<');
	}
}

static const unsigned char *pmin(const unsigned char * const a, const unsigned char * const b) {
	return (a == NULL) ? b : ((b == NULL) ? a : ((a <= b) ? a : b));
}

static int extractLink(unsigned char * const br1, const unsigned char * const br2, const char * const param, const size_t lenParam, const unsigned char linkCharBase) {
	const unsigned char *url = memcasemem(br1 + 1, br2 - (br1 + 1), param, lenParam);
	if (url == NULL) return 0; // param not found
	url += lenParam;

	while (*url == ' ') url++;
	const bool isQuot = (*url == '"');
	if (isQuot) url++;
	if (*url == '\0') return -1;

	const unsigned char * const term = isQuot? memchr(url, '"', br2 - url) : pmin(memchr(url, ' ', br2 - url), br2);
	if (term == NULL || term > br2) return -1;
	size_t lenUrl = term - url;

	unsigned char linkChar = linkCharBase + 1; // Secure by default
	if (lenUrl >= 2 && memeq(url, "//", 2)) {
		url += 2;
		lenUrl -= 2;
	} else if (lenUrl >= 8 && memeq_anycase(url, "https://", 8)) {
		url += 8;
		lenUrl -= 8;
	} else if (lenUrl >= 7 && memeq_anycase(url, "http://", 7)) {
		linkChar--;
		url += 7;
		lenUrl -= 7;
	} else if (lenUrl >= 6 && memeq_anycase(url, "ftp://", 6)) {
		url += 6;
		lenUrl -= 6;
	} else if (lenUrl >= 7 && memeq_anycase(url, "mailto:", 7)) {
		url += 7;
		lenUrl -= 7;
		linkChar = AEM_CET_CHAR_MLT;
	} else if ((lenUrl >= 4 && memeq_anycase(url, "cid:", 4)) || (lenUrl >= 11 && memeq_anycase(url, "javascript:", 11)) || !isalnum(*url)) return 0; // otherwise, bare link with no protocol specified, assume HTTPS

	if (lenUrl < 3) return 0;

	// Replace the content
	*br1 = linkChar;
	memmove(br1 + 1, url, lenUrl); // TODO: Lowercase domain (until slash)
	br1[1 + lenUrl] = linkChar;

	return 2 + lenUrl;
}

// Needs bracketsInQuotes() and lfToSpace()
static void html2cet(unsigned char * const text, size_t * const lenText) {
	text[*lenText] = '\0';
	size_t begin = 0;

	while (begin < *lenText) {
		unsigned char *br1 = memchr(text + begin, '<', *lenText - begin);
		if (br1 == NULL) return;

		const unsigned char *br2 = memchr(br1 + 1, '>', (text + *lenText) - (br1 + 1));
		if (br2 == NULL) return;
		br2++;

		int keep = 0;
		switch (br1[1]) {
			case '/':
				if (
				   memeq_anycase(br1 + 2, "li", 2)
				|| memeq_anycase(br1 + 2, "table", 5)
				|| memeq_anycase(br1 + 2, "td", 2)
				|| memeq_anycase(br1 + 2, "tr", 2)
				) {
					br1[0] = AEM_HTML_PLACEHOLDER_LINEBREAK;
					keep = 1;
				}

				if (
				   memeq_anycase(br1 + 2, "p>", 2)
				|| memeq_anycase(br1 + 2, "p ", 2)
				|| memeq_anycase(br1 + 2, "div>", 4)
				|| memeq_anycase(br1 + 2, "div ", 4)
				) {
					br1[0] = AEM_HTML_PLACEHOLDER_LINEBREAK;
					br1[1] = AEM_HTML_PLACEHOLDER_LINEBREAK;
					keep = 2;
				}
			break;

			case 'A':
			case 'a':
				if (br1[2] == ' ') {keep = extractLink(br1, br2, "href=", 5, AEM_CET_CHAR_LNK);}
				else if (memeq_anycase(br1 + 1, "udio ", 5)) {keep = extractLink(br1, br2, "src=", 4, AEM_CET_CHAR_FIL);}
			break;

			case 'B':
			case 'b':
				if ((br1[2] == 'R' || br1[2] == 'r') && (br1[3] == ' ' || br1[3] == '>')) {
					*br1 = AEM_HTML_PLACEHOLDER_LINEBREAK;
					keep = 1;
				}
			break;

			case 'D':
			case 'd':
				if ((br1[2] == 'I' || br1[2] == 'i') && (br1[3] == 'V' || br1[3] == 'v') && (br1[4] == ' ' || br1[4] == '>')) {
					br1[0] = AEM_HTML_PLACEHOLDER_LINEBREAK;
					br1[1] = AEM_HTML_PLACEHOLDER_LINEBREAK;
					keep = 2;
				}
			break;

			case 'E':
			case 'e':
				if (memeq_anycase(br1 + 2, "mbed ", 5)) {keep = extractLink(br1, br2, "src=", 4, AEM_CET_CHAR_LNK);}
			break;

			case 'F':
			case 'f':
				if (memeq_anycase(br1 + 2, "rame ", 5)) {keep = extractLink(br1, br2, "src=", 4, AEM_CET_CHAR_LNK);}
			break;

			case 'H':
			case 'h':
				if ((br1[2] == 'R' || br1[2] == 'r') && (br1[3] == ' ' || br1[3] == '>')) {
					*br1 = AEM_CET_CHAR_HRB;
					keep = 1;
				}
			break;

			case 'I':
			case 'i':
				if (memeq_anycase(br1 + 2, "frame ", 6)) {keep = extractLink(br1, br2, "src=", 4, AEM_CET_CHAR_LNK);}
				else if (memeq_anycase(br1 + 2, "mg ", 3)) {keep = extractLink(br1, br2, "src=", 4, AEM_CET_CHAR_FIL);}
			break;

			case 'O':
			case 'o':
				if (memeq_anycase(br1 + 2, "bject ", 6)) {keep = extractLink(br1, br2, "data=", 5, AEM_CET_CHAR_LNK);}
			break;

			case 'P':
			case 'p':
				if (br1[2] == '>' || br1[2] == ' ') {
					br1[0] = AEM_HTML_PLACEHOLDER_LINEBREAK;
					br1[1] = AEM_HTML_PLACEHOLDER_LINEBREAK;
					keep = 2;
				}
			break;

			case 'S':
			case 's':
				if (memeq_anycase(br1 + 2, "ource ", 6)) {keep = extractLink(br1, br2, "src=", 4, AEM_CET_CHAR_LNK);}
				else if (memeq_anycase(br1 + 2, "tyle", 4)) {
					const unsigned char * const br3 = memcasemem(br2 + 1, (text + *lenText) - (br2 + 1), "</style>", 8);
					if (br3 != NULL) br2 = br3 + 8;
				}
			break;

			case 'T':
			case 't':
				if (memeq_anycase(br1 + 2, "rack ", 4)) {keep = extractLink(br1, br2, "src=", 4, AEM_CET_CHAR_LNK);}
				else if (
				   memeq_anycase(br1 + 2, "able", 4)
				|| memeq_anycase(br1 + 2, "d", 1)
				|| memeq_anycase(br1 + 2, "r", 1)
				) {*br1 = AEM_HTML_PLACEHOLDER_LINEBREAK; keep = 1;}
			break;

			case 'V':
			case 'v':
				if (memeq_anycase(br1 + 2, "ideo ", 5)) {keep = extractLink(br1, br2, "src=", 4, AEM_CET_CHAR_LNK);}
			break;
		}

		if (keep < 0) return;

		br1 += keep;
		begin = br1 - text;
		const size_t lenRem = br2 - br1;

		memmove(br1, br2, (text + *lenText) - br2);
		*lenText -= lenRem;
	}
}

static void text2space(char * const hay, const size_t lenHay, const char * const needle, const size_t lenNeedle) {
	char *c;
	while ((c = memmem(hay, lenHay, needle, lenNeedle)) != NULL) {
		memset(c, ' ', lenNeedle);
	}
}

void htmlToText(char * const text, size_t * const len) {
	removeControlChars((unsigned char*)text, len);
	lfToSpace(text, *len);
	bracketsInQuotes(text);

	text2space(text, *len, "<!--", 4);
	text2space(text, *len, "-->", 3);

	html2cet((unsigned char*)text, len);
	decodeHtmlRefs((unsigned char*)text, len);

	convertChar(text, *len, AEM_HTML_PLACEHOLDER_LINEBREAK, '\n');
	convertChar(text, *len, AEM_HTML_PLACEHOLDER_SINGLEQUOTE, '\'');
	convertChar(text, *len, AEM_HTML_PLACEHOLDER_DOUBLEQUOTE, '"');
	convertChar(text, *len, AEM_HTML_PLACEHOLDER_GT, '>');
	convertChar(text, *len, AEM_HTML_PLACEHOLDER_LT, '<');

	cleanText((unsigned char*)text, len, false);
}
