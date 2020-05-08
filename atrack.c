#include "fcgi_stdio.h"

#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#ifdef DBGLOG
static FILE *dbgf;
#define DPRINTF(FMT...) \
	do { fprintf(dbgf, FMT); fflush(dbgf); } while (0)
#else
#define DPRINTF(FMT...)
#endif

typedef unsigned long long u64;
typedef unsigned u32;
typedef unsigned char u8;

#define MAX_POST_LEN 50000
#define MAX_PATH_LEN 500

static const char *getEnvStr(const char *envVar)
{
	const char *res=getenv(envVar);
	if (!res)
		res="";
	return res;
}

static int getEnvInt(const char *envVar)
{
	const char *envStr=getEnvStr(envVar);
	return atoi(envStr);
}

struct envCGI {
	int contLen;
	const char *docRoot;
	const char *scriptName;
	const char *remoteAddr;
	const char *reqMethod;
};

static void getCGIEnv(struct envCGI *env)
{
	env->contLen=getEnvInt("CONTENT_LENGTH");
	env->docRoot=getEnvStr("DOCUMENT_ROOT");
	env->scriptName=getEnvStr("SCRIPT_NAME");
	env->remoteAddr=getEnvStr("REMOTE_ADDR");
	env->reqMethod=getEnvStr("REQUEST_METHOD");
	DPRINTF("ENV: postLen=%d, docRoot=%s, scriptName=%s, remoteAddr=%s, reqMethod=%s\n",
		env->contLen, env->docRoot, env->scriptName, env->remoteAddr, env->reqMethod
		);
}

#define PACK_POS_SIZE 12
static int packPos(const char *latStr, const char *lonStr, u64 t, u8 packed[PACK_POS_SIZE])
{
	if (t<1)
		return -1;
	char *endptr=NULL;

	double lat=strtod(latStr, &endptr);
	if (endptr==latStr)
		return -1;
	lat=(lat+90)*(UINT_MAX/180.0);

	double lon=strtod(lonStr, &endptr);
	if (endptr==lonStr)
		return -1;
	lon=(lon+180)*(UINT_MAX/360.0);
	u32 *p=(u32 *)packed;

	if (lat<=0) {
		p[0]=0;
	} else if (lat>=UINT_MAX) {
		p[0]=UINT_MAX;
	} else {
		p[0]=lat;
	}
	if (lon<=0) {
		p[1]=0;
	} else if (lon>=UINT_MAX) {
		p[1]=UINT_MAX;
	} else {
		p[1]=lon;
	}
	p[2]=(u32)t;
	return 0;
}

static void makeGpsBinName(struct envCGI *env, char fileName[MAX_PATH_LEN+1])
{
	fileName[0]='\0';
	const char *scriptPost=strchr(env->scriptName, '/');
	if (scriptPost==env->scriptName)
		scriptPost=strchr(scriptPost+1, '/');
	if (!scriptPost)
		return;

	size_t prefLen=scriptPost-env->scriptName;
	if (prefLen>MAX_PATH_LEN)
		return;

	++scriptPost;
	if (!scriptPost[0])
		return;

	char scriptPref[MAX_PATH_LEN+1];
	memcpy(scriptPref, env->scriptName, prefLen);
	scriptPref[prefLen]='\0';

	int res=snprintf(fileName, MAX_PATH_LEN, "%s%s_data/%s.gpsbin", env->docRoot, scriptPref, scriptPost);
	fileName[MAX_PATH_LEN]='\0';
	if (res>MAX_PATH_LEN)
		fileName[0]='\0';
}

static int hexdigit(char d)
{
	if (d>='0' && d<='9')
		return d-'0';
	if (d>='a' && d<='f')
		return d-('a'-10);
	if (d>='A' && d<='F')
		return d-('A'-10);
	return -1;
}

// request=start_activity&activity=motorcycling&privacy=private&source=OruxMaps&title=2019-10-26+23%3A2920191026_2329&version=70422&points=42.6166067+23.41849221+589.788+1572121755+

static int parseMmtReqPoints(struct envCGI *env, char *wbuf)
{
	DPRINTF("parseMmtReqPoints: %s\n", wbuf);

	char fileName[MAX_PATH_LEN+1];
	makeGpsBinName(env, fileName);
	int fd=open(fileName, O_CREAT|O_APPEND|O_RDWR, 0666);
	int tknIdx=0;
	u64 lastT=0;
	// lat, lon, alt, time
	const char *tkn[4]={ NULL, NULL, NULL, NULL };
	int idx=0, start=0;
	while (wbuf[idx]) {
		char c=wbuf[idx];
		if (c=='+' || c==' ') {
			wbuf[idx]='\0';
			tkn[tknIdx++]=wbuf+start;
			if (tknIdx>=4) {
				u64 newT=atoll(tkn[3]);
				u8 pos[PACK_POS_SIZE];
				if (newT>lastT && !packPos(tkn[0], tkn[1], newT, pos)) {
					lastT=newT;
					if (fd>=0)
						write(fd, pos, PACK_POS_SIZE);
				}
				tknIdx=0;
			}
			start=idx+1;
		}
		++idx;
	}
	if (fd>=0)
		close(fd);
	return 0;
}

static const char *parseMmtReq(struct envCGI *env, char *wbuf)
{
	const char *reqType="";
	const char *var=NULL;
	char *val=NULL;
	int ridx=0, widx=0;
	int start=0;
	char c;
	do {
		c=wbuf[ridx++];
		char wc=c;
		if (c=='%') {
			int c0=hexdigit(wbuf[ridx++]);
			if (c0<0) {
				DPRINTF("parseMmtReq: non hex 1\n");
				return NULL;
			}
			int c1=hexdigit(wbuf[ridx++]);
			if (c1<0) {
				DPRINTF("parseMmtReq: non hex 2\n");
				return NULL;
			}
			wc=(c0<<4)|c1;
		}
		if (c=='=' || c=='&')
			wc=0;
		wbuf[widx++]=wc;
		if (c=='=' && !var) {
			var=wbuf+start;
			val=NULL;
			start=widx;
		} else if (c=='&' || c=='\0') {
			val=wbuf+start;
			if (!var) {
				var=val;
				val="";
			}
			start=widx;
			DPRINTF("!!! var=%s, val=%s\n", var, val);
			if (!strcmp(var, "request")) {
				reqType=val;
			} else if (!strcmp(var, "points")) {
				int res=parseMmtReqPoints(env, val);
				DPRINTF("parseMmtReqPoints: %d\n", res);
				if (res)
					return NULL;
			}
			var=NULL;
			val=NULL;
		}
		
	} while (c);

	if (!strcmp(reqType, "start_activity"))
		return "activity_started";
	if (!strcmp(reqType, "update_activity"))
		return "activity_updated"; 
	if (!strcmp(reqType, "stop_activity"))
		return "activity_stopped"; 
	return NULL;
}

static void sendResponse(const char *type, u64 activityId)
{
	char respBuf[2000];
	if (type) {
		char *actStr="";
		char actStrBuf[200];
		if (activityId) {
			sprintf(actStrBuf, " <activity_id>%llu</activity_id>\r\n", activityId);
			actStr=actStrBuf;
		}
		snprintf(respBuf, sizeof(respBuf), "Content-type: text/xml\r\n"
			"\r\n"
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
			"<message>\r\n"
			" <type>%s</type>\r\n"
			"%s"
			"</message>\r\n",
			type, actStr);
		respBuf[sizeof(respBuf)-1]='\0';
	} else {
		strcpy(respBuf, "Status: 400 Bad Request\r\n");
	}
	//fwrite(respBuf, strlen(respBuf), 1, stdout); // fcgilib flushes it?
	printf("%s", respBuf);

	DPRINTF("<<< %s >>>\n", respBuf);
}

static void processRequest(struct envCGI *env)
{
	const u64 actId=1234;
	int error=(0!=strcmp(env->reqMethod, "POST"));
	if (!error)
		error=(env->contLen<1 || env->contLen>=MAX_POST_LEN);
	if (!error) {
		char buf[env->contLen+1];
		int n=fread(buf, env->contLen, 1, stdin);
		buf[env->contLen]='\0';
		error=(n!=1);

		DPRINTF("err:%d,str:%s\n", error, buf);

		const char *respType=parseMmtReq(env, buf);
		sendResponse(respType, actId);
	}
}

int main(void)
{
#ifdef DBGLOG
	dbgf=fopen("/tmp/dbglog.txt", "w");
	DPRINTF("Started\n");
#endif

	while (FCGI_Accept()>=0) {
		struct envCGI env;
		getCGIEnv(&env);
		processRequest(&env);
	}
	return 0;
}
