/*
 * 2perma.c
 * Brought to you by the HackHarvard AutoArchival team.
 *
 * Shantanu Gupta <sg3273(AT)drexel.edu>
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>

#define E_NOMEM (1)
#define E_SYSTM (2)
#define E_ASSRT (3)

#define MTO_API "http://timetravel.mementoweb.org/api/json/0/"

#define PRM_OLD "4e4b3c5a831c61c77ae7343d91bcb7f548942ca0"
#define PRM_KEY "38c8262054e15f908eff3893c336f518a7f35d0b"
#define PRM_API "https://api.perma.cc/v1/archives/?api_key=" PRM_KEY

#define MIN_PTR(a, b)                                                          \
({                                                                             \
	__typeof__ (a) _a = (a);                                                   \
	__typeof__ (b) _b = (b);                                                   \
	__typeof__ (a) _r;                                                         \
                                                                               \
	if (_a == NULL)                                                            \
		_r = _b; else                                                          \
	if (_b == NULL)                                                            \
		_r = _a;                                                               \
	else                                                                       \
		_r = _b < _a ? _b : _a;                                                \
                                                                               \
	_r;                                                                        \
})

/*
 * Internal callback function to handle JSON response.
 */
static size_t JSONCallBack(void *contents, size_t size, size_t nmemb, void *userp)
{
	char **ptr = userp;

	// Something isn't right! but waiting it out seems to fix it.
	if (*ptr)
		usleep(100000);
	*ptr = malloc((size * nmemb) + 1);
	if (!*ptr)
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

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (!curl)
		exit(E_NOMEM);

	curl_easy_setopt(curl, CURLOPT_URL, pURL);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	if (!JSONReq) {
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
	curl_global_cleanup();

	return JSONRes;
}


char *getMemtoLink(char *sURL)
{
	char *JSONEndp, *JSONRes, *idx, *ret = NULL;

	JSONEndp = malloc(strlen(sURL) + strlen(MTO_API) + 1);
	if (JSONEndp) {
		strcpy(JSONEndp, MTO_API);
		strcat(JSONEndp, sURL);

		JSONRes = cUrlPerform(JSONEndp, NULL);
		if (JSONRes) {
			idx = strstr(JSONRes, "\"last\":{\"datetime\":\"");
			if (idx) {
				idx = strstr(idx, "\"uri\":[\"");
				ret = &idx[8];
				idx = MIN_PTR(strstr(idx, "\","), strstr(idx, "\"]"));
				idx[0] = '\0';
				ret = strdup(ret);
			}

			free(JSONRes);
		}

		free(JSONEndp);
	}

	return ret;
}

char *getPermaLink(char *sURL)
{
	char *JSONReq, *JSONRes, *idx, *ret = NULL;

////////////////////////////////////////////////////////////////////////////////
	if (sURL)
		return strdup("E_PERMACC");
////////////////////////////////////////////////////////////////////////////////

	JSONReq = malloc(strlen(sURL) + 32);
	if (!JSONReq)
		exit(E_NOMEM);

	sprintf(JSONReq, "{\n\t\"url\": \"%s\",\n\t\"title\": \"\"\n}\n", sURL);

	JSONRes = cUrlPerform(PRM_API, JSONReq);
	if (!JSONRes)
		goto cleanupEx;

	idx = strstr(JSONRes, "\"guid\": \"");
	if (!idx)
		goto cleanup;

	idx[18] = '\0';
	ret = strdup("http://perma.cc/         ");
	if (!ret)
		exit(E_NOMEM);
	strcpy(&ret[16], &idx[9]);

cleanup:
	free(JSONRes);
cleanupEx:
	free(JSONReq);

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

static void pbar(float x)
{
	printf("\r");
	if (x != 100)
		printf("Progress: %.2f%%", x);
	fflush(stdout);
}

void pdf2perma(char *sNameInPdf)
{
	int   n;
	FILE *fp;
	char *cmd;
	char *cur;
	char *sIn;
	char *sNameInTxt;
	char *sNameOut;
	long  iInLen;

	pbar(0);

	// Convert file to text
	cmd = malloc(11 + strlen(sNameInPdf));
	if (!cmd)
		exit(E_NOMEM);
	sprintf(cmd, "pdftotext %s", sNameInPdf);
	n = system(cmd);
	free(cmd);
	if (n)
		return;

	pbar(10);

	// Build filenames
	sNameInTxt = strdup(sNameInPdf);
	if (!sNameInTxt)
		exit(E_NOMEM);
	strcpy(&sNameInTxt[strlen(sNameInPdf) - 3], "txt");
	sNameOut = strdup(sNameInPdf);
	if (!sNameOut)
		exit(E_NOMEM);
	strcpy(&sNameOut[strlen(sNameInPdf) - 3], "tex");

	pbar(15);

	// Read text
	fp = fopen(sNameInTxt, "r");
	if (!fp)
		goto cleanup;
	fseek(fp, 0, SEEK_END);
	iInLen = ftell(fp);
	sIn = malloc(iInLen + 1);
	if (!sIn)
		exit(E_NOMEM);
	rewind(fp);
	fread(sIn, sizeof(char), (size_t) iInLen, fp);
	fclose(fp);
	remove(sNameInTxt);

	pbar(18);

	// Create Tex file
	fp = fopen(sNameOut, "w+");
	if (!fp)
		goto cleanupEx;

	fprintf(fp, "\\documentclass{article}\n"
				"\\usepackage[english]{babel}\n"
				"\\usepackage[hidelinks]{hyperref}\n"
				"\\usepackage{longtable}\n"
				"\\begin{document}\n"
				"\\begingroup\n"
				"\\vspace*{-6\\baselineskip}\n"
				"\\setlength{\\LTleft}{-20cm plus -1fill}\n"
				"\\setlength{\\LTright}{\\LTleft}\n"
				"\t\\begin{sloppypar}\n"
				"\t\\def\\arraystretch{1.6}%\n"
				"\t\\begin{longtable}{ | p{8cm} | p{8cm} | } \\hline\n"
				"\t\\centering Source URL &	\\centering Perma URL\n"
				"\t\\tabularnewline \\hline\n");

	pbar(20);

	// Process text
	cur = sIn;
	while((cur = strstr(cur, "http")) != NULL)
	{
		pbar(20 + (60*(((float) (cur-sIn))/iInLen)));

		if (cur[4] == ':'
		|| (cur[4] == 's' && cur[5] == ':'))
		{
			unsigned i = 4;
			char *tmp, *end, *dec;

			// Find end of the URL
			end = NULL;
			end = MIN_PTR(end, strchr(cur, ' '));
			end = MIN_PTR(end, strchr(cur, '('));
			end = MIN_PTR(end, strchr(cur, ')'));
			end = MIN_PTR(end, strchr(cur, ','));
			end = MIN_PTR(end, strchr(cur, ';'));
			for (char chr = 1; chr < 32; chr++)
				if (chr != '\n')
					end = MIN_PTR(end, strchr(cur, chr));
			end = MIN_PTR(end, strstr(cur, ".\n"));

			if (end[-1] == '.')
				end--;

			// Weed out known bad characters "\\n", "\n" and " ".
			tmp = &cur[i];
			do {
				if (*tmp != ' '
				&& (*tmp != '\n'))
					 cur[i++] = *tmp;
			} while (++tmp < end);
			cur[i] = '\0';

			tmp = getPermLink(cur);
			if (tmp == NULL)
				tmp = strdup("E_API");
			if (tmp == NULL)
				exit(E_NOMEM);

			// Because
			// https://groups.google.com/forum/#!topic/google-gson/JDHUo9DWyyM
			// "=" encoded as "\u003d"
			while ((dec = strstr(tmp, "\\u003d")) != NULL)
			{
				*dec = '=';
				memmove(&dec[1], &dec[6], strlen(&dec[6]) + 1);
			}
			// "&" encoded as "\u0026"
			while ((dec = strstr(tmp, "\\u0026")) != NULL)
			{
				*dec = '=';
				memmove(&dec[1], &dec[6], strlen(&dec[6]) + 1);
			}

			fprintf(fp, "\t\\url{%s} & \\url{%s} \\\\ \\hline\n", cur, tmp);

			free(tmp);
			cur = end + 1;
		}
	}

	fprintf(fp, "\t\\end{longtable}\n"
				"\t\\end{sloppypar}\n"
				"\\endgroup\n"
				"\\end{document}\n");
	fclose(fp);

	pbar(80);

	cur = strstr(sNameInTxt, ".txt");
	cur[0] = '\0';

	// Compile to pdf
	cmd = malloc(48 + (2*strlen(sNameInPdf)));
	if (!cmd)
		exit(E_NOMEM);
	sprintf(cmd, "pdflatex -jobname %stemp %s >/dev/null", sNameInTxt, sNameOut);
	n = system(cmd);
	if (n)
		exit(E_SYSTM);

	// Remove temp files
	remove(sNameOut);
	sprintf(cmd, "%stemp.aux", sNameInTxt);
	remove(cmd);
	sprintf(cmd, "%stemp.log", sNameInTxt);
	remove(cmd);
	sprintf(cmd, "%stemp.out", sNameInTxt);
	remove(cmd);

	pbar(90);

	sprintf(cmd, "pdftk %s.pdf %stemp.pdf cat output %s_perma.pdf",
			sNameInTxt,
			sNameInTxt,
			sNameInTxt);
	n = system(cmd);

	sprintf(cmd, "%stemp.pdf", sNameInTxt);
	if (!n)
		remove(cmd);

	pbar(95);

cleanupEx:
	free(sIn);
cleanup:
	free(cmd);
	free(sNameOut);
	free(sNameInTxt);
	pbar(100);
}

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; i++)
	{
		char *pptr = strstr(argv[i], ".pdf");

		while (pptr != NULL && pptr[4] != '\0')
			pptr = strstr(pptr+4, ".pdf");

		printf("[%d/%d] %s\n", i, (argc-1), argv[i]);

		if (pptr)
		{
			pdf2perma(argv[i]);
		} else {
			printf("Usage: ./url2perma [*.pdf]+\n");
		}

	}
}

