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
		exit(-1);
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
		exit(-1);

	headers = curl_slist_append(headers, "Accept: application/json");
	if (JSONReq)
		headers = curl_slist_append(headers, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, pURL);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	if (JSONReq == NULL)
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
	else
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	if (JSONReq)
	{
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, JSONReq);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(JSONReq));
	}
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
		exit(-1);

	strcpy(JSONEndp, MTO_API);
	strcat(JSONEndp, sURL);

	JSONRes = cUrlPerform(JSONEndp, NULL);
	if (!JSONRes)
		goto cleanup;

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

	return ret;
}

char *getPermaLink(char *sURL)
{
	char *JSONReq, *JSONRes, *idx, *ret = NULL;

	JSONReq = malloc(strlen(sURL) + 32);
	if (!JSONReq)
		exit(-1);

	sprintf(JSONReq, "{\n\t\"url\": \"%s\",\n\t\"title\": \"\"\n}\n", sURL);

	JSONRes = cUrlPerform(PRM_API, JSONReq);
	if (!JSONRes)
		goto cleanup;

	idx = strstr(JSONRes, "\"guid\": \"");
	if (idx == NULL)
		goto cleanup;
	idx[18] = '\0';

	ret = malloc(32);
	if (!ret)
		exit(-1);

	strcpy(ret, "http://perma.cc/");
	strcat(ret, &idx[9]);

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
	sprintf(cmd, "pdftotext %s", sNameInPdf);

	e = system(cmd);
	free(cmd);

	if (e)
	{
		printf("E: Could not convert %s :(\n", sNameInPdf);
		return;
	}

	sNameInTxt = strdup(sNameInPdf);
	strcpy(&sNameInTxt[strlen(sNameInPdf) - 3], "txt");
	sNameOutCsv = strdup(sNameInPdf);
	strcpy(&sNameOutCsv[strlen(sNameInPdf) - 3], "csv");

	fp = fopen(sNameInTxt, "r");
	fseek(fp, 0, SEEK_END);
	iInLen = ftell(fp);
	sIn = malloc(iInLen + 1);
	rewind(fp);
	fread(sIn, sizeof(char), (size_t) iInLen, fp);
	fclose(fp);
	free(sNameInTxt);

	fp = fopen(sNameOutCsv, "w+");

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

/*
 * This function handles json files, this is VERY HACKY!
 * Most of it deals with the **bad** links in legal text
 *
 * Also, this function is as is from the hackathon version
 * as I have finals coming up and cannot afford to deal
 * with this at the moment.
 *
 * Hall of Fame:
 *
 *	http://www.whitehouse drugpolicy.gov/streetterms/ByType.asp?int TypeID =2
 *	Cocaine,\nhttp://www.whitehousedrugpolicy.gov/streetterms/ByType.asp?intTyp\neID=2
 *	http://www.usdoj.gov/oig/special/s 0608/full&#8212;report.pdf.</p>\n
 *	at\nhttp://www.usdoj.gov/oig/special/s0608/full_report.pdf.\n
 *	2011),\nhttp://www.census.gov/newsroom/releases/archives/2010_census/cb11\n-cn181.html;
 *	http://www.hcch.\n\nnet/upload/wop/abd_pd02efs2006.pdf
 *	http://www.nacdl.org/public.nsf/2cdd02b415ea 3a64852566d6000daa79/departures/$FILE/Rehnquist_letter.pdf.
 *	http://www.nacdl.org/ public.nsf/2cdd02b415ea3a64852566d6000daa79/departures/$FILE/stcg_ comm_current.pdf.
 *	http://www.epa.gov/OUST/ pubs/musts.pdf
 *	\"\nhttp://www.epa.gov/ogwdw/contaminants/dw_contamfs/benzene.html\n(last
 */

void jso2perma(char *fNameIn)
{
	char *pStrIn, *fNameOut;
	unsigned fSizeIn;
	FILE *fpIn, *fpOut;

	fNameOut = malloc(strlen(fNameIn) + 7);
	strcpy(fNameOut, fNameIn);
	strcpy(&fNameOut[strlen(fNameOut) - 5], "_perma.json");

	fpIn  = fopen(fNameIn, "r");
	fpOut = fopen(fNameOut, "w+");

	if (!fpIn)
		printf("E: Could not open input file %s\n", fNameIn);
	if (!fpOut)
		printf("E: Could not open output file %s\n", fNameOut);

	if (!fpIn || !fpOut)
		return;

	fseek(fpIn, 0, SEEK_END);
	fSizeIn = ftell(fpIn);
	rewind(fpIn);
	pStrIn = malloc(fSizeIn * sizeof(char));
	fread(pStrIn, sizeof(char), fSizeIn, fpIn);
	fclose(fpIn);

	char *fin = strstr(pStrIn, "\"plain_text\"");
	char *p01 = strstr(pStrIn, "\"html");
	char *cur = p01 ? p01 : pStrIn;
	char *end = pStrIn, *wrh = pStrIn;

	while (cur = strstr(cur, "http"))
	{
		if (cur > fin)
			break;
		if ((cur[4] == 's')
		||  (cur[4] == ':'))
		{
			end = strchr(cur, '<');
			end = MIN_PTR(strchr(cur, '>'), end);
			end = MIN_PTR(strchr(cur, '%'), end);
			end = MIN_PTR(strchr(cur, '|'), end);
			end = MIN_PTR(strchr(cur, '^'), end);
			end = MIN_PTR(strchr(cur, '['), end);
			char *sc = strchr(cur, ';');
			char *am = strchr(cur, '&');
			if (am > sc)
				end = MIN_PTR(sc, end);
			end = MIN_PTR(strchr(cur, '('), end);
			end = MIN_PTR(strchr(cur, ')'), end);
			end = MIN_PTR(strchr(cur, ','), end);
			end = MIN_PTR(strchr(cur, '\''), end);
			end = MIN_PTR(strchr(cur, '\"'), end);
			end = MIN_PTR(strstr(cur+1, "http"), end);

			char *p = cur;
			for (;;) {
				p = strchr(p+1, '.');
				if (!p || p > end)
					break;
				if (p[1] == ' ')
				{
					if (p[2] < 'a' || p[2] > 'z')
						end = p;
				} else {
					if (p[1] < 'a' || p[1] > 'z')
						end = p;
				}
			}

			char t = end[0];
			end[0] = '\0';
			fprintf(fpOut, "%s", wrh);

			char *cURL = strdup(cur);
			char *tURL = cURL;
			int i = 0;
			while (*tURL)
			{
				if (*tURL != ' '
				&& (!(tURL[0] == '\\' && tURL[1] == 'n'))
				&& (!(tURL[0] == 'n' && tURL[-1] == '\\'))
				)
					cURL[i++] = *tURL;
				tURL++;
			}
			cURL[i] = '\0';
			p = cURL;
			for(;;) {
				p = strchr(p+1, '&');
				if (!p || p >= &cURL[strlen(cURL)])
					break;
				if (strncmp(p, "&#8212;", 7) == 0)
				{
					*p = '_';
					strcpy(p+1, p+7);
				} else if (strncmp(p, "&amp;", 5) == 0) {
					strcpy(p+1, p+5);
				} else if (strncmp(p, "&gt;", 4) == 0) {
					*p = '>';
					strcpy(p+1, p+4);
				}
			}
			char *pURL = getPermLink(cURL);
			free(cURL);
			if (pURL)
			{
				fprintf(fpOut, " (%s)", pURL);
				free(pURL);
			}
			end[0] = t;

			wrh = end;
			cur = end + 1;
		}
		cur++;
	}

	fprintf(fpOut, "%s", wrh);
	fclose(fpOut);
	free(pStrIn);
	free(fNameOut);
}

int main(int argc, char *argv[])
{
	curl_global_init(CURL_GLOBAL_ALL);

	for (int i = 1; i < argc; i++)
	{
		char *pptr = strstr(argv[i], ".pdf");
		char *jptr = strstr(argv[i], ".json");

		while (pptr != NULL && pptr[4] != '\0')
			pptr = strstr(pptr+4, ".pdf");
		while (jptr != NULL && jptr[5] != '\0')
			jptr = strstr(jptr+5, ".json");

		if (pptr == NULL && jptr == NULL)
		{
			printf("Usage: ./url2perma [*.pdf|*.json]+\n");
		} else if (pptr == NULL && jptr != NULL) {
			jso2perma(argv[i]);
		} else if (pptr != NULL && jptr == NULL) {
			pdf2perma(argv[i]);
		} // else Impossibru!
	}

	curl_global_cleanup();
}

