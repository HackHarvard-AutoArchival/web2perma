/*
 * Some license I don't really care about
 *
 *	- Shantanu Gupta <shans95g@gmail.com>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>


/*
 * Min, but with a twist.
 * If either of the arguments is zero then return the other one.
 */
#define MIN(a,b) (((a)<(b))? ((a) ? (a) : (b)):((b) ? (b) : (a)))

/*
 * Internal callback function to handle JSON response.
 */
static size_t JSONCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	char **ptr = userp;
	*ptr = realloc(*ptr, (size * nmemb) + 1);

	memcpy(*ptr, contents, size * nmemb);
	(*ptr)[size * nmemb] = '\0';

	return size * nmemb;
}

/*
 * Gateway to libcurl
 */
char *getPermaLink(char *URL)
{
	CURL *curl;
	CURLcode res;
	struct curl_slist *headers = NULL;
	char *JSONReq = malloc(32 + strlen(URL)), *JSONRes = NULL, *PermLnk = NULL;

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (!curl)
		return NULL;

	sprintf(JSONReq, "{\n\t\"url\": \"%s\",\n\t\"title\": \"\"\n}\n", URL);

	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, "https://api.perma.cc/v1/archives/?api_key=4e4b3c5a831c61c77ae7343d91bcb7f548942ca0");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, JSONReq);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(JSONReq));
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, JSONCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &JSONRes);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "permacc++");
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	free(JSONReq);

	char *idx = strstr(JSONRes, "\"guid\": \"");
	if (idx)
	{
		PermLnk = malloc(10);
		idx += 9;
		strncpy(PermLnk, idx, 9);
		PermLnk[9] = '\0';
	}
	free(JSONRes);

	return PermLnk;
}

void web2perma(char *fNameIn, char *fNameOut)
{
	char *pStrIn;
	unsigned fSizeIn;
	FILE *fpIn = fopen(fNameIn, "r"), *fpOut = fopen(fNameOut, "w");

	if (!fpIn || !fpOut)
		return NULL;

	fseek(fpIn, 0, SEEK_END);
	fSizeIn = ftell(fpIn);
	rewind(fpIn);
	pStrIn = malloc(fSizeIn * sizeof(char));
	fread(pStrIn, sizeof(char), fSizeIn, fpIn);
	fclose(fpIn);

	char *cur = strstr(pStrIn, "<html") ? strstr(pStrIn, "<html") : pStrIn, *end = pStrIn, *wrh = pStrIn;

	while (cur = strstr(cur, "http"))
	{
		end = MIN(strstr(cur, ". "), strchr(cur, ' '));

		if ((cur[4] == 's' && cur[5] == ':' && cur[6] == '/' && cur[7] == '/')
		|| ( cur[4] == ':' && cur[5] == '/' && cur[6] == '/'))
		{
			char t = end[0];
			end[0] = '\0';
			fprintf(fpOut, "%s", wrh);
			char *pURL = getPermaLink(cur);
			fprintf(fpOut, " (http://perma.cc/%s)", pURL);
			end[0] = t;
			free(pURL);
		}

		wrh = end;
		cur = end + 1;
	}

	fprintf(fpOut, "%s", wrh);
	fclose(fpOut);
	free(pStrIn);
}

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; i += 2)
		web2perma(argv[i], argv[i+1]);
}

