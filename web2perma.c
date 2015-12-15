/*
 * 2perma.c
 * Brought to you by the HackHarvard AutoArchival team.
 *
 * Shantanu Gupta <sg3273(AT)drexel.edu>
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

#define E_NOMEM (-1)

#define MTO_API "http://timetravel.mementoweb.org/api/json/0/"

#define PRM_KEY "4e4b3c5a831c61c77ae7343d91bcb7f548942ca0"
#define PRM_API "https://api.perma.cc/v1/archives/?api_key=" PRM_KEY

#define MIN_PTR(a, b)                                                          \
	({                                                                         \
		__typeof__ (a) _a = (a);                                               \
		__typeof__ (b) _b = (b);                                               \
		_b < _a ? _b != NULL ? _b : _a : _a != NULL ? _a : _b;                 \
	})


/*
 * Internal callback function to handle JSON response.
 */
static size_t JSONCallBack(void *contents, size_t size, size_t nmemb, void *userp)
{
	char **ptr = userp;

	*ptr = realloc(*ptr, (size * nmemb) + 1);
	if (*ptr == NULL)
		exit(E_NOMEM);
	memcpy(*ptr, contents, size * nmemb);
	(*ptr)[size * nmemb] = '\0';

	return size * nmemb;
}

/*
 * Gateway to libcurl
 */
char *cUrlPerform(char *pURL, char *JSONReq)
{
	CURL *curl;
	char *JSONRes = NULL;
	struct curl_slist *headers = NULL;

	curl = curl_easy_init();
	if (!curl)
		exit(E_NOMEM);

	curl_easy_setopt(curl, CURLOPT_URL, pURL);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

	if (JSONReq == NULL) {
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
	} else {
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, JSONReq);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(JSONReq));
		headers = curl_slist_append(headers, "Content-Type: application/json");
	}

	headers = curl_slist_append(headers, "Accept: application/json");

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, JSONCallBack);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &JSONRes);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "permacc++");
	curl_easy_perform(curl);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (JSONReq)
		free(JSONReq);

	return JSONRes;
}


char *getMemtoLink(char *sURL)
{
	char *JSONEndp, *JSONRes, *idx, *ret = NULL;

	JSONEndp = malloc(strlen(sURL) + 45);
	if (!JSONEndp)
		exit(E_NOMEM);

	strcpy(JSONEndp, MTO_API);
	strcat(JSONEndp, sURL);

	JSONRes = cUrlPerform(JSONEndp, NULL);
	if (!JSONRes)
		goto cleanupEx;

	idx = strstr(JSONRes, "\"last\":{\"datetime\":\"");
	if (idx == NULL)
		goto cleanup;

	idx = strstr(idx, "\"uri\":[\"");
	ret = &idx[8];
	idx = MIN_PTR(strchr(idx, ','), strstr(idx, "\"]"));
	idx[0] = '\0';
	ret = strdup(ret);

cleanup:
	free(JSONRes);
cleanupEx:
	free(JSONEndp);

	return ret;
}

char *getPermaLink(char *sURL)
{
	char *JSONReq, *JSONRes, *idx, *ret = NULL;

	JSONReq = malloc(strlen(sURL) + 32);
	if (!JSONReq)
		exit(E_NOMEM);

	sprintf(JSONReq, "{\n\t\"url\": \"%s\",\n\t\"title\": \"\"\n}\n", sURL);

	JSONRes = cUrlPerform(PRM_API, JSONReq);
	if (!JSONRes)
		return NULL;

	idx = strstr(JSONRes, "\"guid\": \"");
	if (idx == NULL)
		goto cleanup;
	idx[18] = '\0';

	ret = strdup("http://perma.cc/         ");
	if (!ret)
		exit(E_NOMEM);

	strcpy(&ret[16], &idx[9]);

cleanup:
	free(JSONRes);

	return ret;
}

char *getPermLink(char *sURL)
{
	char *ret;

	ret = getMemtoLink(sURL);
	if (ret == NULL)
		ret = getPermaLink(sURL);

	return ret;
}

void pdf2perma(char *sNameInPdf)
{
	int e;
	FILE *fp;
	char *cmd;
	char *cur;
	char *end;
	char *sIn;
	char *sNameInTxt;
	char *sNameOutCsv;
	long iInLen;

	cmd = malloc(11 + strlen(sNameInPdf));
	if (!cmd)
		exit(E_NOMEM);
	sprintf(cmd, "pdftotext %s", sNameInPdf);
	e = system(cmd);
	free(cmd);
	if (e)
		return; // Process next file

	sNameInTxt = strdup(sNameInPdf);
	if (!sNameInTxt)
		exit(E_NOMEM);
	strcpy(&sNameInTxt[strlen(sNameInPdf) - 3], "txt");
	sNameOutCsv = strdup(sNameInPdf);
	if (!sNameOutCsv)
		exit(E_NOMEM);
	strcpy(&sNameOutCsv[strlen(sNameInPdf) - 3], "csv");

	fp = fopen(sNameInTxt, "r");
	if (!fp)
		return; // Process next file
	fseek(fp, 0, SEEK_END);
	iInLen = ftell(fp);
	sIn = malloc(iInLen + 1);
	if (!sIn)
		exit(E_NOMEM);
	rewind(fp);
	fread(sIn, sizeof(char), (size_t) iInLen, fp);
	fclose(fp);
	free(sNameInTxt);

	fp = fopen(sNameOutCsv, "w+");
	if (!fp)
		return; // Process next file
	cur = sIn;

	while(cur = strstr(cur, "http"))
	{
		if (cur[4] == ':'
		|| (cur[4] == 's' && cur[5] == ':'))
		{
			long  i = 0;
			char *cURL, *tURL;

			end = NULL;
			end = MIN_PTR(end, strchr(cur, ' '));
			end = MIN_PTR(end, strchr(cur, ';'));
			end = MIN_PTR(end, strchr(cur, '['));
			end = MIN_PTR(end, strstr(cur+1, "http"));
			end = MIN_PTR(end, strstr(cur, ".\n"));
			end = MIN_PTR(end, strstr(cur, ". "));
			end = MIN_PTR(end, strstr(cur, "\n\n"));

			if (end[-1] == '.')
				end--;
			end[0] = '\0';

			cURL = strdup(cur);
			if (!cURL)
				exit(E_NOMEM);
			tURL = cURL;
			while (*tURL)
			{
				if (*tURL == '\\' && tURL[1] == 'n')
					 tURL ++; else
				if (*tURL != ' '
				&& (*tURL != '\n'))
					 cURL[i++] = *tURL;
				tURL++;
			}
			cURL[i] = '\0';

			fprintf(fp, "%s,", cURL);
			tURL = getPermLink(cURL);
			if (tURL == NULL)
				tURL = strdup("E: API");
			if (tURL == NULL)
				exit(E_NOMEM);
			fprintf(fp, "%s\n", tURL);
			free(tURL);
			free(cURL);

			cur = end + 1;
		}
	}

	free(sIn);
	fclose(fp);
	free(sNameOutCsv);
}

int main(int argc, char *argv[])
{
	curl_global_init(CURL_GLOBAL_ALL);

	for (int i = 1; i < argc; i++)
	{
		char *pptr = strstr(argv[i], ".pdf");

		while (pptr != NULL && pptr[4] != '\0')
			pptr = strstr(pptr+4, ".pdf");

		if (pptr)
		{
			pdf2perma(argv[i]);
		} else {
			printf("Usage: ./url2perma [*.pdf]+\n");
		}
	}

	curl_global_cleanup();
}

