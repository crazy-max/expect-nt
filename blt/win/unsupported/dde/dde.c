/*
 * example.c --
 *
 *	This file is an example of a Tcl dynamically loadable extension.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) example.c 1.4 96/04/26 10:42:55
 * To do: Deal with removing a list element when an interpreter is
 *           deleted.
 */

#include <windows.h>
#include <ddeml.h>
#include <dde.h>
#include <windowsx.h>
#include <tcl.h>
#include <tk.h>
#include <tclInt.h>
#include <ctype.h>

#pragma hdrstop

#if defined(__WIN32__)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef WIN32_LEAN_AND_MEAN

/*
 * VC++ has an alternate entry point called DllMain, so we need to rename
 * our entry point.
 */

#   if defined(_MSC_VER)
#	define EXPORT(a,b) __declspec(dllexport) a b
#	define DllEntryPoint DllMain
#   else
#	if defined(__BORLANDC__)
#	    define EXPORT(a,b) a _export b
#	else
#	    define EXPORT(a,b) a b
#	endif
#   endif
#else
#   define EXPORT(a,b) a b
#endif

#if	(TCL_MAJOR_VERSION>=8 && TCL_RELEASE_LEVEL>0)||TCL_MAJOR_VERSION>8
#	define STR_FROM(A) Tcl_GetStringFromObj(A,NULL)
#	define ARGV_T	Tcl_Obj *CONST*
#	define	DDE_CREATECOMMAND(I,N,P,d,D) Tcl_CreateObjCommand(I, N, P, d.objClientData, D)
#	define	TCL8b1OrBetter
#else
#	define STR_FROM(A) A
#	define ARGV_T	char**
#	define	DDE_CREATECOMMAND(I,N,P,d,D) Tcl_CreateCommand(I, N, P, d.clientData, D)
#endif
#define TCL_FUNCTIONS 1
#define TK_FUNCTIONS  2
/*
 * Declarations for functions defined in this file.
 */
extern int		Tcl_LoadCmd(ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv);

static int		Dde_LoadCmd(ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv);
static int		Dde_WinfoCmd(ClientData clientData,
			    Tcl_Interp *interp, int argc, ARGV_T argv);
static int		Dde_TkCmd(ClientData clientData,
			    Tcl_Interp *interp, int argc, ARGV_T argv);
static int		Dde_InterpCmd(ClientData clientData,
			    Tcl_Interp *interp, int argc, ARGV_T argv);
static int		DdeCmd(int is_send, ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv);
static void		GetBaseName(char *name,
			    Tcl_Interp* interp);
static int		GetAllNames(int *argc, char ***argv,
			    Tcl_Interp* interp);
static void		GetFinalName(char *name,
			    Tcl_Interp* interp);
static void		ShowDDEError(Tcl_Interp *interp);
static void		appendHszInterp(Tcl_Interp *interp,
			    char *interpName);
static HDDEDATA EXPENTRY DDECallback(WORD wType, WORD wFmt,
			    HCONV hConvX, HSZ hsz1, HSZ hsz2,
			    HDDEDATA hData, DWORD dwData1, DWORD dwData2);


EXTERN EXPORT(int,Dde_Init) (Tcl_Interp *interp);

typedef struct hszinterp {
    HSZ interpName;
    Tcl_Interp *interp;
    /*
     * Note that this is really used as an hszinterp *, but Borland C
     * can't compile that!?!?!?!
     */
    struct hszinterp *next;
    HCONV hConv;
    HDDEDATA hData;        /* for client-specific asynch code. */
    long result;
    char *string;
} hszinterp;

hszinterp *firstHI;
hszinterp *lastHI;

typedef struct resultitem {
    hszinterp *hszinterp;
    char *string;
    struct resultitem *next;
} resultitem;

resultitem *firstResultItem;
resultitem *lastResultItem;

HANDLE hInst,myInst;
DWORD  srvrInst = 0L;
Tcl_Interp *outinterp;
HSZ hszAppName;
HSZ hszTopic;
HSZ hszItem;
HDDEDATA       hData;
HCONV hConv = (HCONV)NULL;
HCONVLIST hConvList = (HCONVLIST)NULL;
#define CTOPICS 6
HSZ hszTop[CTOPICS];
int highHsz;
HSZPAIR hszPair[CTOPICS+1];
char Topics[]="Topics\tName";
char TopicsTopics[]="TopicItemList";
char TopicsName[]="Foo";
WORD           wFmt = CF_TEXT;         /*  Clipboard format                */

char szAppName[] = "DCA";
char otherApps[1023];
char *logfile;
FARPROC DdeProc;
Tcl_HashTable interps;
Tcl_HashTable interpnames;

/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This wrapper function is used by Windows to invoke the
 *	initialization code for the DLL.  If we are compiling
 *	with Visual C++, this routine will be renamed to DllMain.
 *	routine.
 *
 * Results:
 *	Returns TRUE;
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef __WIN32__
BOOL APIENTRY
DllEntryPoint(hInstance, reason, reserved)
    HINSTANCE hInstance;		/* Library instance handle. */
    DWORD reason;		/* Reason this function is being called. */
    LPVOID reserved;		/* Not used. */
{
    hInst = hInstance;
    myInst = hInstance;
    return TRUE;
}
#endif

static void
print(const char* fmt, ... )
{
    	va_list	ap;
	if (Tcl_GetVar(outinterp, "dde_verbosity", 0) != NULL) {
    		char	*buffer = (char *)ckalloc(1000);
        	strcpy(buffer, "puts {");
		va_start(ap,fmt);
		vsprintf(buffer+6,fmt,ap);
		strcat(buffer,"};update idletasks");
        	Tcl_Eval(outinterp, buffer);
        	ckfree(buffer);
    		}
	else if (logfile){
    		FILE	*ofile=fopen(logfile, "a+");
		fputc('\'',ofile);
		va_start(ap,fmt);
		vfprintf(ofile,fmt,ap);
		fputc('\'',ofile);
		fputc('\n',ofile);
		fclose(ofile);
		}
}

/*
 *----------------------------------------------------------------------
 *
 * ShowDDEError --
 *
 * This function is provided to simplify error reporting of the DDE state to
 * the Tcl interpreter.
 *
 * Results:
 *	Returns TCL_ERROR;
 *
 * Side effects:
 *	Puts the error string in the interp->result.
 *
 *----------------------------------------------------------------------
 */

static void
ShowDDEError(interp)
    Tcl_Interp *interp;  /* Interpreter to report the error to. */
{
    UINT xactresult;
    char szDDEString[255];

    xactresult = DdeGetLastError(srvrInst);
    switch(xactresult) {
	case DMLERR_ADVACKTIMEOUT:
	    Tcl_AppendResult(interp,"DMLERR_ADVACKTIMEOUT - ", NULL);
	    break;
	case DMLERR_BUSY:
	    Tcl_AppendResult(interp,"DMLERR_BUSY - ", NULL);
	    break;
	case DMLERR_DATAACKTIMEOUT:
	    Tcl_AppendResult(interp,"DMLERR_DATAACKTIMEOUT - ", NULL);
	    break;
	case DMLERR_DLL_NOT_INITIALIZED:
	    Tcl_AppendResult(interp,"DMLERR_DLL_NOT_ACTIVATED - ", NULL);
	    break;
	case DMLERR_DLL_USAGE:
	    Tcl_AppendResult(interp,"DMLERR_DLL_USAGE - ", NULL);
	    break;
	case DMLERR_EXECACKTIMEOUT:
	    Tcl_AppendResult(interp,"DMLERR_EXECACKTIMEOUT - ", NULL);
	    break;
	case DMLERR_INVALIDPARAMETER:
	    Tcl_AppendResult(interp,"DMLERR_INVALIDPARAMETER - ", NULL);
	    break;
	case DMLERR_LOW_MEMORY:
	    Tcl_AppendResult(interp,"DMLERR_LOW_MEMORY - ", NULL);
	    break;
	case DMLERR_MEMORY_ERROR:
	    Tcl_AppendResult(interp,"DMLERR_MEMORY_ERROR - ", NULL);
	    break;
	case DMLERR_NO_CONV_ESTABLISHED:
	    Tcl_AppendResult(interp,"NO_CONV_ESTABLISHED - ", NULL);
	    break;
	case DMLERR_NOTPROCESSED:
	    Tcl_AppendResult(interp,"DMLERR_NOTPROCESSED - ", NULL);
	    break;
	case DMLERR_POKEACKTIMEOUT:
	    Tcl_AppendResult(interp,"DMLERR_POKEACKTIMEOUT - ", NULL);
	    break;
	case DMLERR_POSTMSG_FAILED:
	    Tcl_AppendResult(interp,"DMLERR_POSTMSG_FAILED - ", NULL);
	    break;
	case DMLERR_REENTRANCY:
	    Tcl_AppendResult(interp,"DMLERR_REENTRANCY - ", NULL);
	    break;
	case DMLERR_SERVER_DIED:
	    Tcl_AppendResult(interp,"DMLERR_SERVER_DIED - ", NULL);
	    break;
	case DMLERR_SYS_ERROR:
	    Tcl_AppendResult(interp,"DMLERR_SYS_ERROR - ", NULL);
	    break;
	case DMLERR_UNADVACKTIMEOUT:
	    Tcl_AppendResult(interp,"DMLERR_UNADVACKTIMEOUT - ", NULL);
	    break;
	case DMLERR_UNFOUND_QUEUE_ID:
	    Tcl_AppendResult(interp,"DMLERR_UNFOUND_QUEUE_ID - ", NULL);
	    break;
	default:
	    sprintf(szDDEString,"%ld was the DDE result code...not specified.",xactresult);
	    Tcl_AppendResult(interp,szDDEString, NULL);
    }
}
/*
 *----------------------------------------------------------------------
 *
 * FindHSZInterp --
 *
 * Searches the linked list of hszinterp structures.
 *
 * Input -- one of {Tcl_Interp *|hszstring|hData} (If more than one provided,
 *          only the first non-null one is searched for.)
 *
 * Return -- A pointer if found, NULL if not.
 *
 *----------------------------------------------------------------------
 */
static hszinterp *
FindHSZInterp(Tcl_Interp *interp, HSZ hszstring, HDDEDATA hData)
{
    hszinterp *hi = firstHI;
    int which = (interp?0:(hszstring?1:(hData?2:-1)));

    while(hi) {
        switch(which) {
            case 0:
                if (interp == hi->interp)
                    return hi;
                break;
            case 1:
                if (!DdeCmpStringHandles(hszstring, hi->interpName))
                    return hi;
                break;
            case 2:
                if (hData == hi->hData)
                    return hi;
                break;
        }
        hi = (hszinterp *)hi->next;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ListInterps --
 *
 * This function puts a tab-separated list of the current interps in the
 * buffer you provide.
 *
 * Inputs: The buffer to store the string in.
 *
 * Results:
 *  Puts the tab-delimited list of interps into the string.  WARNING: no
 *  check is made to ensure we won't run out of space.  This needs to be
 *  fixed.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */
static void
ListInterps(char *dest)
{
    hszinterp* hi = firstHI;
    char buffer[255];
    strcpy(dest, "");
    while(hi) {
        if (hi->interpName) {
            if (strlen(dest)) {
                strcat(dest, "\t");
            }
            DdeQueryString(srvrInst, hi->interpName, buffer, 255, CP_WINANSI);
            strcat(dest, buffer);
        }
        hi= hi->next;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DDECallback --
 *
 * This function handles any DDE callbacks that occur.
 *
 * Results:
 *	Returns booleans or data handles to strings depending on the type of
 *      the callback.
 *
 * Side effects:
 *	Puts the error string in the interp->result.
 *
 *----------------------------------------------------------------------
 */
static HDDEDATA EXPENTRY
DDECallback ( WORD wType, WORD wFmt, HCONV hConvX, HSZ hsz1,
	      HSZ hsz2, HDDEDATA hData, DWORD dwData1,
	      DWORD dwData2 )
{
    char dest[255];
    char szDDEString[255];
    int i,j;
    CONVINFO ci;
    HDDEDATA temphData;
    hszinterp *hi;

	        DdeQueryString(srvrInst, hsz1, dest, 255, CP_WINANSI);
	        print("Callback hsz1 - %s",dest);
	        DdeQueryString(srvrInst, hsz2, dest, 255, CP_WINANSI);
    	    	print("Callback hsz2- %s",dest);
    switch ( wType ) {
    	case XTYP_REQUEST:
	        DdeQueryString(srvrInst, hsz1, dest, 255, CP_WINANSI);
	        print("NS - XTYP_REQUEST hsz1 - %s",dest);
	        DdeQueryString(srvrInst, hsz2, dest, 255, CP_WINANSI);
    	    	print("NS - XTYP_REQUEST hsz2- %s",dest);
	        DdeGetData(hData, (LPBYTE)dest, 255, 0);
    	    	print("NS - XTYP_REQUEST hsz2- %s",dest);
	        *dest = 0;
    	    if (hsz1 == hszTop[0] && hsz2 == hszTop[1]) {
        		/* System, Topics*/
        		ListInterps(dest);
	        	sprintf(szDDEString, "Topics\t%s",dest);
		        return DdeCreateDataHandle ( srvrInst, (LPBYTE) szDDEString,
			        strlen ( szDDEString )+1, 0L, hszTop[2], wFmt, 0 );
    	    } else if (hsz1 == hszTop[1] && hsz2 == hszTop[2]) {
        		/* Topics, TopicItemList */
        		ListInterps(dest);
		        return DdeCreateDataHandle ( srvrInst, (LPBYTE) dest,
			        strlen ( dest )+1, 0L, hszTop[2], wFmt, 0 );
    	    } else if (!DdeCmpStringHandles(hsz2,hszTop[2])) {
        		/* Interp, TopicItemList */
        		hi = firstHI;
        		while (hi) {
    	            if (hsz1 == hi->interpName) {
    	                DdeQueryString(srvrInst, hi->interpName, dest, 255, CP_WINANSI);
        	    	    return (HDDEDATA)DdeCreateDataHandle ( srvrInst,
	        	    	    (LPBYTE)dest, (strlen(dest)+1), (DWORD)0L, (HSZ)hi->interpName, (UINT)wFmt, (UINT) 0 );
        	        }
        	        hi = (hszinterp *)hi->next;
        		}
    	    } else if (!DdeCmpStringHandles(hsz2,hszTop[3])) {
    	        /* Result */
    	        if ((hi = FindHSZInterp(NULL, hsz1, NULL)) != NULL) {
    	            if (hi->string) {
            	        temphData = DdeCreateDataHandle ( srvrInst, (LPBYTE) hi->string,
	            		        strlen ( hi->string )+1, 0L, hszTop[3], wFmt, 0 );
			            return temphData;
    	            }
    	        }
    	        break;
    	    } else if (!DdeCmpStringHandles(hsz2,hszAppName)) {
        		/* Tcl - We know it's for Tcl...see if it's our interp. */
        		if ((hi = FindHSZInterp(NULL, hsz1, NULL))!= NULL) {
	                DdeQueryString(srvrInst, hi->interpName, dest, 255, CP_WINANSI);
    	    	    return (HDDEDATA)DdeCreateDataHandle ( srvrInst,
      	    	        (LPBYTE)dest, (strlen(dest)+1), (DWORD)0L,
      	    	        (HSZ)hi->interpName, (UINT)wFmt, (UINT) 0 );
        	    }
    	    }
	        break;
    	case XTYP_XACT_COMPLETE:
	        print("NS - XTYP_XACT_COMPLETE");
	        break;
    	case XTYP_EXECUTE:
	        DdeQueryString(srvrInst, hsz1, dest, 255, CP_WINANSI);
	        print("NS - XTYP_EXECUTE hsz1 - %s", dest);
	        DdeQueryString(srvrInst, hsz2, dest, 255,
		        CP_WINANSI);
    	    	print("NS - XTYP_EXECUTE hsz2- %s",dest);
	        DdeGetData ( hData, (LPBYTE) dest, 255, 0L );
    	    	print("NS - XTYP_EXECUTE hData- %s",dest);
	        hi = FindHSZInterp(NULL, hsz1, NULL);
            DdeQueryConvInfo(hConvX, (DWORD)NULL, &ci);	        
	        if (hi) {
        	    hi->result = Tcl_GlobalEval(hi->interp, dest);
        	    hi->string = (char *)ckalloc(strlen(hi->interp->result)+10);
        	    strcpy(hi->string, hi->interp->result);
        	    sprintf(hi->string, "%06d|%s",hi->result, hi->interp->result);
                hi->hConv = hConvX;
	        } else {
	            hi->string = NULL;
	            hi->result = TCL_ERROR;
	            hi->hConv = (HCONV)NULL;
	        }
    	    return (HDDEDATA)DDE_FACK;
    	case XTYP_ADVSTART:
	        print("NS - XTYP_ADVSTART");
    	case XTYP_ADVREQ:
	        print("NS - XTYP_ADVREQ");
	        break;
    	case XTYP_ADVDATA:
	        print("NS - XTYP_ADVDATA");
	        break;
    	case XTYP_ADVSTOP:
	        print("NS - XTYP_ADVSTOP");
	        break;
    	case XTYP_MONITOR:
	        print("NS - XTYP_MONITOR");
	        break;
    	case XTYP_REGISTER:
	        print("NS - XTYP_REGISTER");
	        DdeQueryString(srvrInst, hsz1, dest, 255, CP_WINANSI);
	        print("NS - XTYP_REGISTER hsz1 - %s", dest);
	        DdeQueryString(srvrInst, hsz2, dest, 255,
		        CP_WINANSI);
    	    	print("NS - XTYP_REGISTER hsz2- %s",dest);
	        break;
    	case XTYP_UNREGISTER:
	        print("NS - XTYP_UNREGISTER");
	        DdeQueryString(srvrInst, hsz1, dest, 255, CP_WINANSI);
	        print("NS - XTYP_EXECUTE hsz1 - %s", dest);
	        DdeQueryString(srvrInst, hsz2, dest, 255,
		        CP_WINANSI);
    	    	print("NS - XTYP_EXECUTE hsz2- %s",dest);
	        break;
    	case XTYP_WILDCONNECT:
	        print("NS - XTYP_WILDCONNECT");
	        if (hsz2 != hszAppName && hsz2 != NULL) {
    		    return FALSE;
    	    }
    	    j = 0;
	        for(i = 0; i < CTOPICS; i++) {
	        	if(hsz1 == NULL || hsz1 == hszTop[i]) {
		            hszPair[j].hszSvc = hszAppName;
		            hszPair[j].hszTopic = hszTop[i];
        		    j++;
	        	}   
	        }
    	    hszPair[j].hszSvc = (HSZ)NULL;
	        hszPair[j].hszTopic = (HSZ)NULL;
	        return (HDDEDATA)DdeCreateDataHandle(srvrInst,
		        (LPBYTE)&hszPair[0],sizeof(hszPair),0L,0,CF_TEXT,0);
    	case XTYP_CONNECT:
	        print("TCL - XTYP_CONNECT");
	        if(!DdeCmpStringHandles(hszAppName,hsz2)) {
    	    	for(i = 0; i < CTOPICS; i++) {
	    	        if(hsz1 == hszTop[i]) {
			            return (HDDEDATA)TRUE;
    		        }
    	    	}
    	    	hi = FindHSZInterp(NULL, hsz1, NULL);
    		    while(hi) {
        	        if (!DdeCmpStringHandles(hsz1,hi->interpName)) {
        	            return (HDDEDATA) TRUE;
        	        }
        	        hi = hi->next;
    		    }
	        }
	        print("Didn't find this Topic.");
	        break;
    	case XTYP_CONNECT_CONFIRM:
	        print("TCL - XTYP_CONNECT_CONFIRM");
	        return((HDDEDATA)TRUE);
    	case XTYP_DISCONNECT:
	        print("TCL - XTYP_DISCONNECT");
	        break;
    	case XTYP_ERROR:
	        print("TCL - XTYP_ERROR");
	        break;
    	default:
	        print("Got a case I didn't recognize.");
    }
    return ( (HDDEDATA) NULL );
}

/*
 *----------------------------------------------------------------------
 *
 * DdeCmd --
 *
 *	This function implements the Tcl "dde" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
DdeCmd(is_send, clientData, interp, argc, argv)
    int	is_send;
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int result = TCL_OK;
    size_t length;
    int c;
    HSZ tempAppName;
    HSZ tempTopic;
    HSZ tempItem;
    Tcl_DString tdAppName;
    char *AppName;
    Tcl_DString tdTopic;
    char *Topic;
    Tcl_DString tdItem;
    char *Item;
    Tcl_DString otherApps, evalCmd;
    HCONV          tempConv;
    CONVINFO ci;
    LPDWORD    ddeResult;
    char dest[255];
    char buffer[1024];
    char *tempchar, *temptab;
    int convinfoflag=0;
    hszinterp *hi;
    char	*argv0 = argv[0];
    int asynchronous=0;
    DWORD timeout=10000L;

	while( argc>1 && (c = argv[1][0])=='-'){
		argv++;
		argc--;
		if (!strcmp(argv[0],"--")) break;
		else	{
			length = strlen(argv[0]);
    			if (!strncmp(argv[0],"-convinfo",length)) {
        			convinfoflag++;
    				}
			else if (!strncmp(argv[0],"-asynch",length)) {
        			asynchronous++;
        			timeout = TIMEOUT_ASYNC;
    				}
			else if (is_send&&!strncmp(argv[0],"-displayof",length)) {
    				Tcl_AppendResult(interp,
	    			argv0, " Win32 doesn't yet support ", argv[0], (char *) NULL);
				return TCL_ERROR;
				}
			else	{
    				Tcl_AppendResult(interp,
	    				argv0, " bad argument ",
					argv[0], (char *) NULL);
				return TCL_ERROR;
				}
			}
		}
	if(argc<2){
    		Tcl_AppendResult(interp, "wrong # args: should be \"",
	    		argv0, " ?-convinfo? ?-async? ?--? option ?arg arg ...?\"", (char *) NULL);
    		return TCL_ERROR;
    		}
    
    length = strlen(argv[1]);
    Tcl_Preserve(clientData);
    if (is_send || (c == 'e') && (strncmp(argv[1], "exec", length) == 0)
	    && (length >= 2)) {
        Tcl_DStringInit(&tdAppName);
        Tcl_DStringInit(&tdTopic);
        if (argc > 3-is_send && strlen(argv[3-is_send])>0) {
            Tcl_DStringAppend(&tdTopic,argv[3-is_send],-1);
            Topic = Tcl_DStringValue(&tdTopic);
        } else {
            Topic = NULL;
        }
	if(is_send){
            	Tcl_DStringAppend(&tdAppName,"Tcl",-1);
            	AppName = Tcl_DStringValue(&tdAppName);
		}
	else if (argc > 2 && strlen(argv[2])>0) {
            	Tcl_DStringAppend(&tdAppName,argv[2],-1);
            	AppName = Tcl_DStringValue(&tdAppName);
        	}
	else	{
		AppName = NULL;
		}
        if (argc == 1) {
            Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv0, " exec app topic command\"",
		    (char *) NULL);
            Tcl_DStringFree(&tdTopic);
            Tcl_DStringFree(&tdAppName);
            result = TCL_ERROR;
        } else {
            Tcl_DStringInit(&evalCmd);
            if (AppName) {
                tempAppName = DdeCreateStringHandle(srvrInst, AppName, CP_WINANSI);
            } else {
		tempAppName = (HSZ) AppName;
	    }
            if (Topic) {
                tempTopic = DdeCreateStringHandle(srvrInst, Topic, CP_WINANSI);
            } else {
                tempTopic = (HSZ) Topic;
            }
            for(c=4-is_send;c<argc;c++) {
                Tcl_DStringAppend(&evalCmd, argv[c], -1);
                if (c < argc-1) {
                    Tcl_DStringAppend(&evalCmd, " ", -1);
                }
            }
            hConvList = DdeConnectList ( srvrInst, tempAppName, tempTopic, (HCONVLIST)0L,
		    (PCONVCONTEXT) NULL );
            if (hConvList == (HCONVLIST)NULL) {
        		Tcl_AppendResult(interp, "Unsuccessful DDE connection.", (char *)NULL );
		        result = TCL_ERROR;
        		ShowDDEError (interp);
            } else {
                /*
                 * I assume that there's only 1 transaction going on at a time
                 * within one interpreter.  If this is invalid, this code needs
                 * to be re-whacked.
                 */
                hi = FindHSZInterp(interp, NULL, NULL);
                if (hi) {
    		        tempConv = NULL;
           	        while ((tempConv = DdeQueryNextServer(hConvList,tempConv))!=(HCONV)NULL) {
       	           	    hData = (HDDEDATA) DdeClientTransaction(
                			(LPBYTE)Tcl_DStringValue(&evalCmd),
			                (DWORD)(Tcl_DStringLength(&evalCmd)+1), (HCONV)tempConv,
            		    	(UINT)0L, (UINT)0L, (UINT)XTYP_EXECUTE, timeout,
			                (LPDWORD)&ddeResult);
    			        /*
	    		         * hData is either FALSE (DDEML error)
		    	         * or TRUE (Tcl may or may not have erred, don't know yet.)
			             */
			            if (hData && !asynchronous) {
                		    hData = (HDDEDATA) DdeClientTransaction(
		                           	(LPBYTE)NULL,
                        			(DWORD)0, (HCONV)tempConv,
                		        	(HSZ)hszTop[3], (UINT)wFmt, (UINT)XTYP_REQUEST, (DWORD)10000,
                        			(LPDWORD)&ddeResult);
                    		if (hData) {
                    		    length = DdeGetData(hData, (LPBYTE)NULL, 1, 0);
                    		    if (length) {
                    		        tempchar = (char *)ckalloc(length+1);
                    		        DdeGetData(hData, (LPBYTE)tempchar, length, 0);
                    		        c = atoi(tempchar);
                    		        Tcl_SetResult(interp, tempchar+7, TCL_VOLATILE);
                    		        return c;
                                }
                    		} else {
                    		    ShowDDEError(interp);
                    		    return TCL_ERROR;
                    		}
    			        } else {
    			            ShowDDEError(interp);
    			            return TCL_ERROR;
    			        }
           	        }
        		} else {
                    Tcl_AppendResult(interp,"Couldn't find interp in exec in DdeCmd.",(char *)NULL);
                    result = TCL_ERROR;
                    goto done;
        		}
            }
            DdeDisconnectList(hConvList);
        }	            
    } else if ((c == 'e') && (strncmp(argv[1], "export", length) == 0)
	    && (length >= 2)) {
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)
            && (length >= 2)) {
        Tcl_DStringInit(&tdAppName);
        Tcl_DStringInit(&tdTopic);
        Tcl_DStringInit(&tdItem);
        if (argc > 4 && strlen(argv[4])>0) {
            Tcl_DStringAppend(&tdItem,argv[4],-1);
            Item = Tcl_DStringValue(&tdItem);
        } else {
            Item = NULL;
        }
        if (argc > 3 && strlen(argv[3])>0) {
            Tcl_DStringAppend(&tdTopic,argv[3],-1);
            Topic = Tcl_DStringValue(&tdTopic);
        } else {
            Topic = NULL;
        }
        if (argc > 2 && strlen(argv[2])>0) {
            Tcl_DStringAppend(&tdAppName,argv[2],-1);
            AppName = Tcl_DStringValue(&tdAppName);
        } else {
            AppName = NULL;
        }
        if (argc == 1) {
            Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv0, " get ?app ?topic ?item???\"",
		    (char *) NULL);
            Tcl_DStringFree(&tdItem);
            Tcl_DStringFree(&tdTopic);
            Tcl_DStringFree(&tdAppName);
            result = TCL_ERROR;
            goto done;
        }
        Tcl_DStringInit(&otherApps);
        if (AppName) {
            tempAppName = DdeCreateStringHandle(srvrInst, AppName, CP_WINANSI);
        } else {
	        tempAppName = (HSZ) AppName;
    	}
        if (Topic) {
            tempTopic = DdeCreateStringHandle(srvrInst, Topic, CP_WINANSI);
        } else {
            tempTopic = (HSZ) Topic;
        }
        hConvList = DdeConnectList ( srvrInst, tempAppName, tempTopic, (HCONVLIST)0L,
        	(PCONVCONTEXT) NULL );
        if ( hConvList == (HCONVLIST)NULL ) {
    	    Tcl_AppendResult(interp, "Unsuccessful DDE connection.", (char *)NULL );
	        result = TCL_ERROR;
	        ShowDDEError (interp);
        } else {
            tempConv = NULL;
	        tempItem = DdeCreateStringHandle(srvrInst, Item, CP_WINANSI);
       	    while ((tempConv =
	        	    DdeQueryNextServer(hConvList,tempConv))!=(HCONV)NULL) {
    		    DdeQueryConvInfo(tempConv, QID_SYNC,
            			(PCONVINFO) &ci);
                if (convinfoflag) {
    	            strcpy(buffer,"{{");
        		    DdeQueryString(srvrInst, ci.hszSvcPartner,dest,255,CP_WINANSI);
    	            strcat(buffer,dest);
		            strcat(buffer,"} {");
            	    DdeQueryString(srvrInst, ci.hszServiceReq,dest,255,CP_WINANSI);
    	            strcat(buffer,dest);
        		    strcat(buffer,"} {");
		            DdeQueryString(srvrInst, ci.hszTopic,dest,255,CP_WINANSI);
    	            strcat(buffer,dest);
        		    strcat(buffer,"} {");
		            DdeQueryString(srvrInst, ci.hszItem,dest,255,CP_WINANSI);
        		    strcat(buffer,dest);
		            strcat(buffer,"}}");
      				
        		    if (Tcl_DStringLength(&otherApps) > 0) {
		            	Tcl_DStringAppend(&otherApps," ",-1);
        		    }					
		            Tcl_DStringAppend(&otherApps, buffer, -1);
                }
                if (Item != 0) {
        		    hData = (HDDEDATA) DdeClientTransaction(
		                	(LPBYTE)Tcl_DStringValue(&tdItem),
                			(DWORD)(Tcl_DStringLength(&tdItem)+1), (HCONV)tempConv,
                			(HSZ)tempItem, (UINT)wFmt, (UINT)XTYP_REQUEST, (DWORD)10000,
                			(LPDWORD)&ddeResult);
               	    if (hData) {
       	                DdeGetData ( hData, (LPBYTE) dest, 80L, 0L );
       	                tempchar = dest;
       	                temptab = tempchar;
       	                while(temptab) {
       	            	    temptab=strchr(tempchar,'\11');
            			    if (temptab) {
    		            		*(temptab) = '\0';
            			    }
			                sprintf(buffer, "{{%s} {%s} {%s} {%s}}",
            				    Tcl_DStringValue(&tdAppName),
			            	    Tcl_DStringValue(&tdTopic),
            				    Tcl_DStringValue(&tdItem),
			            	    tempchar);
            			    if (Tcl_DStringLength(&otherApps) > 0) {
            	    			Tcl_DStringAppend(&otherApps," ",-1);
			                }
            			    Tcl_DStringAppend(&otherApps, buffer, -1);
       	   	                if (temptab) {
            		    		tempchar = temptab+1;
			                }
            			}
               	    }
        		}
	        }
    	    DdeDisconnectList(hConvList);
	        result = TCL_OK; 
    	}
	    Tcl_DStringResult(interp, &otherApps);
    	Tcl_DStringFree(&tdItem);
    	Tcl_DStringFree(&tdTopic);
    	Tcl_DStringFree(&tdAppName);
	    Tcl_DStringFree(&otherApps);
    } else if ((c == 'i') && (strncmp(argv[1], "import", length) == 0)
	    && (length >= 2)) {
    } else if ((c == 'l') && (strncmp(argv[1], "listen", length) == 0)
	    && (length >= 2)) {
    } else {
    	Tcl_AppendResult(interp, "bad option \"", argv[1],
	    	"\": must be get, exec, export, import, listen", (char *) NULL);
    	result = TCL_ERROR;
    }

    done:
    Tcl_Release(clientData);
    return result;
}

static int
DdeDDE(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    return DdeCmd(0, clientData, interp, argc, argv);
}

static int
DdeSend(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    return DdeCmd(2, clientData, interp, argc, argv);
}

/*
 *----------------------------------------------------------------------
 *
 * GetBaseName --
 *
 *	This function tries to come up with a believable basename.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
GetBaseName(bname, interp)
    char *bname;
    Tcl_Interp *interp;
{
    char *tempbeg, *tempend;

    tempbeg = bname;
    while (*(tempbeg)) {
      *(tempbeg) = (char)(tolower(*(tempbeg)));
      tempbeg++;
    }
    tempbeg = strrchr(bname, '\\');
    if (!tempbeg) {
	tempbeg = strchr(bname, '/');
    }
    if (tempbeg) {
        tempbeg++;
   	} else {
   	    tempbeg = bname;
    }
    tempend = strrchr(tempbeg, '.');
    if (tempend) {
        *(tempend) = '\0';
    }
    strcpy(bname, tempbeg);
}

/*
 *----------------------------------------------------------------------
 *
 * GetAllNames --
 *
 *	This function uses the basename and a list of the other DDE apps
 *      that Tcl is providing to come up with the final name.
 *
 * Results:
 *	A standard Tcl DString.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static int
GetAllNames(int *argc, char ***argv, Tcl_Interp* interp)
{
    int number=0,numberConvs=0;
    int i,j;
    char buffer[255], *endstr, **largv;
    HCONVLIST hConvList;
    HCONV tempConv;
    char *tempbeg, *tempend;
    char **temparray;
    HSZ AppName = DdeCreateStringHandle ( srvrInst,
	    "Tcl", CP_WINANSI );
    HSZ Topic = DdeCreateStringHandle ( srvrInst,
	    "Topics", CP_WINANSI );
    HSZ tempItem = DdeCreateStringHandle ( srvrInst,
	    "TopicItemList", CP_WINANSI );
    *argc = 0;
    *argv = NULL;

    hConvList = DdeConnectList ( srvrInst, AppName, Topic, (HCONVLIST)0L,
    	(PCONVCONTEXT) NULL );
    if ( hConvList == (HCONVLIST)NULL ) {
        Tcl_AppendResult(interp, "Unsuccessful DDE connection.", (char *)NULL );
        ShowDDEError (interp);
    	return number;
    } else {
        /*
         * Count the conversations to find out how many strings to store.
         */
        tempConv = NULL;
        while ((tempConv = (HCONV)DdeQueryNextServer(hConvList,tempConv))!=(HCONV)NULL) {
            numberConvs++;
        }
        if (numberConvs) {
            temparray = (char **)ckalloc( (sizeof(char *)) * numberConvs );

            /*
             * First, get the tab-delimited list of interps from each
             * process.
             */
            while ((tempConv = (HCONV)DdeQueryNextServer(hConvList,tempConv))!=(HCONV)NULL) {
       	        hData = (HDDEDATA) DdeClientTransaction(
            	    	(LPBYTE)NULL,
      	           	    (DWORD)0, (HCONV)tempConv,
    		            tempItem, (UINT)wFmt, (UINT)XTYP_REQUEST, (DWORD)10000,
	        	        NULL);
                if (hData) {
                    DdeGetData ( hData, (LPBYTE) buffer, 80L, 0L );
                    temparray[number] = (char *)ckalloc(strlen(buffer)+1);
                    tempbeg = buffer;
                    tempend = temparray[number];
                    while(*(tempbeg)) {
                        *(tempend++) = (char)(tolower(*(tempbeg++)));
                    }
                    *(tempend++) = '\0';
                    number++;
                }
            }
            
            /*
             * Okay, now count the pieces by # of '\t'+1 per string,
             * if the string has any contents. If it doesn't, ignore it.
             */
            number = j = 0; 
            for(i=0;i<numberConvs;i++) {
                tempbeg = temparray[i];
                if (*(tempbeg)) {
                    	number++;
		    	j+=strlen(tempbeg);
                	}
                while(*(tempbeg)) {
                    if (*(tempbeg++) == '\t') {
                        number++;
                    }
                }
            }

            /*
             * Okay, now chop those up and put into argv.
             */
            largv = (char **)ckalloc( (i=sizeof(char *)*number)+j+number);
	    endstr= (char*)largv+i;
             
            for(j=0,i=0;i<numberConvs;i++) {
                    tempbeg = temparray[i];
                    while ((tempend = strchr(tempbeg, '\t'))!= NULL) {
                        *tempend = '\0';
                        strcpy(endstr, tempbeg);
                        largv[j++] = endstr;
			endstr += strlen(endstr)+1;
                	tempbeg=tempend+1;
                    	}
                    
                    strcpy(endstr, tempbeg);
                    largv[j++] = endstr;
		    endstr += strlen(endstr)+1;
		    ckfree(temparray[i]);
            }
	    ckfree((char *) temparray);
            *(argc) = j;
	    *argv = largv;
        }
    }
    DdeDisconnectList(hConvList);
    return number;
}


/*
 *----------------------------------------------------------------------
 *
 * GetFinalName --
 *
 *	This function uses the basename and a list of the other DDE apps
 *      that Tcl is providing to come up with the final name.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Windows is case insensitive, and doesn't preserve case, so, we lose case
 *  in the appname.  Tough.
 *
 *----------------------------------------------------------------------
 */

static void
GetFinalName(name, interp)
    char *name;
    Tcl_Interp *interp;
{
    int new=0;
    int numnames, idx;
    Tcl_HashTable nameTable;
    int argc;
    char **argv;
    char buffer[255];
    GetBaseName(name,interp);
    new = GetAllNames(&argc, &argv, interp);
    Tcl_InitHashTable(&nameTable,TCL_STRING_KEYS);
    if (new) {        
    	for (numnames=0;numnames<argc;numnames++) {
    	    	        Tcl_CreateHashEntry(&nameTable, argv[numnames], &new);
    	}
    } else {
        numnames=0;
    }
    strcpy(buffer, name);
    new = (!(Tcl_FindHashEntry(&nameTable, buffer)));
    idx = 2;
    while (!new) {
    	sprintf(buffer,"%s #%d",name,idx++);
	    new = (!(Tcl_FindHashEntry(&nameTable, buffer)));
    }
    strcpy(name,buffer);
    sprintf(buffer, "tk_no_dde appname %s", name);
    Tcl_GlobalEval(interp, buffer);
    Tcl_DeleteHashTable(&nameTable);
    if(argv) ckfree((char *) argv);
}

static int
pointInterp(Tcl_Interp *interp, int tkfunctions)
{
    char tempstring[255];
    Tcl_CmdInfo cmdinfo;

    if (tkfunctions & TCL_FUNCTIONS) {
        if (Tcl_Eval(interp,"info comm load") == TCL_OK) {
            strcpy(tempstring, interp->result);
            if (!strlen(tempstring)) {
                print("tk command not found...is Tk installed correctly?");
                return  TCL_OK;
            } else {
                if (Tcl_Eval(interp,"info comm load_no_dde") == TCL_OK) {
                    strcpy(tempstring, interp->result);
                    if (!strlen(tempstring)) {
                        print("load_no_dde not found...creating");
                    } else {
                        print("Oops...load_no_dde found. Deleting and creating.");
                        Tcl_Eval(interp,"rename load {}");
                        Tcl_Eval(interp,"rename load_no_dde load");
                    }
                    Tcl_Eval(interp,"rename load load_no_dde");
                    Tcl_GetCommandInfo(interp, "load_no_dde",&cmdinfo);
                    Tcl_CreateCommand(interp, "load", Dde_LoadCmd, cmdinfo.clientData, NULL);
                }
            }
        }
        if (Tcl_Eval(interp,"info comm interp") == TCL_OK) {
            strcpy(tempstring, interp->result);
            if (!strlen(tempstring)) {
                print("interp command not found...is Tcl installed correctly?");
                return  TCL_OK;
            } else {
                if (Tcl_Eval(interp,"info comm interp_no_dde") == TCL_OK) {
                    strcpy(tempstring, interp->result);
                    if (!strlen(tempstring)) {
                        print("interp_no_dde not found...creating");
                    } else {
                        print("Oops...interp_no_dde found. Deleting and creating.");
                        Tcl_Eval(interp,"rename interp {}");
                        Tcl_Eval(interp,"rename interp_no_dde interp");
                    }
                    Tcl_Eval(interp,"rename interp interp_no_dde");
                    Tcl_GetCommandInfo(interp, "interp_no_dde",&cmdinfo);
                    DDE_CREATECOMMAND(interp, "interp", Dde_InterpCmd, cmdinfo, NULL);
                }
            }
        }
    }
    if (tkfunctions & TK_FUNCTIONS) {
        if (Tcl_Eval(interp,"info comm winfo") == TCL_OK) {
            strcpy(tempstring, interp->result);
            if (!strlen(tempstring)) {
                print("winfo command not found...is Tk installed correctly?");
                return  TCL_OK;
            } else {
                if (Tcl_Eval(interp,"info comm winfo_no_dde") == TCL_OK) {
                    strcpy(tempstring, interp->result);
                    if (!strlen(tempstring)) {
                        print("winfo_no_dde not found...creating");
                    } else {
                        print("Oops...winfo_no_dde found. Deleting and creating.");
                        Tcl_Eval(interp,"rename winfo {}");
                        Tcl_Eval(interp,"rename winfo_no_dde winfo");
                    }
                    Tcl_Eval(interp,"rename winfo winfo_no_dde");
                    Tcl_GetCommandInfo(interp, "winfo_no_dde",&cmdinfo);
                    DDE_CREATECOMMAND(interp, "winfo", Dde_WinfoCmd, cmdinfo, NULL);
                } else {
                    strcpy(tempstring, interp->result);
                    print("info comm winfo_no_dde returned an error:");
                    print(tempstring);
                }
            }
        }
        if (Tcl_Eval(interp,"info comm tk") == TCL_OK) {
            strcpy(tempstring, interp->result);
            if (!strlen(tempstring)) {
                print("tk command not found...is Tk installed correctly?");
                return  TCL_OK;
            } else {
                if (Tcl_Eval(interp,"info comm tk_no_dde") == TCL_OK) {
                    strcpy(tempstring, interp->result);
                    if (!strlen(tempstring)) {
                        print("tk_no_dde not found...creating");
                    } else {
                        print("Oops...tk_no_dde found. Deleting and creating.");
                        Tcl_Eval(interp,"rename tk {}");
                        Tcl_Eval(interp,"rename tk_no_dde tk");
                    }
                    Tcl_Eval(interp,"rename tk tk_no_dde");
                    Tcl_GetCommandInfo(interp, "tk_no_dde",&cmdinfo);
                    DDE_CREATECOMMAND(interp, "tk", Dde_TkCmd, cmdinfo, NULL);
                }
            }
        }
#if	0
        Tcl_Eval(interp,"proc send {args} {\n  uplevel 1 dde exec Tcl $args\n}");
#else
	Tcl_CreateCommand(outinterp, "send", DdeSend, cmdinfo.clientData, NULL);        
#endif
	Tcl_CreateCommand(outinterp, "dde", DdeDDE, cmdinfo.clientData, NULL);        
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Dde_Init --
 *
 *	This procedure initializes the example command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

EXPORT(int,Dde_Init)(master_interp)
    Tcl_Interp *master_interp;
{
    char szDDEString[255];
    char *temp;
    int	i, first_del;

	outinterp = master_interp;
    if(first_del=(firstHI==NULL)){
    	firstHI = (hszinterp *)ckalloc(sizeof(hszinterp));
    	lastHI = firstHI;
    	firstResultItem = 0;
    	lastResultItem = 0;
    	logfile = getenv("LOGFILE");
    	if (logfile) {
        	unlink(logfile);
    		}
    	if (Tcl_Eval(outinterp,"info comm tk") == TCL_OK) {
        	strcpy(szDDEString, outinterp->result);
        	if (!strlen(szDDEString)) {
            	print("tk command not found...is Tk installed correctly?");
            	return TCL_OK;
        		}
    		}
    	DdeProc = MakeProcInstance ( (FARPROC) DDECallback, hInst );
    	if ( DdeInitialize ( (LPDWORD)&srvrInst, (PFNCALLBACK)DdeProc,
	    	APPCLASS_STANDARD, 0L ) ) {
    		print ( "Name server DDE initialization failure." );
	    	return ( FALSE );
    		}
    	hszTop[0] = DdeCreateStringHandle ( srvrInst, SZDDESYS_TOPIC, CP_WINANSI );
    	hszTop[1] = DdeCreateStringHandle ( srvrInst, "Topics", CP_WINANSI );
    	hszTop[2] = DdeCreateStringHandle ( srvrInst, "TopicItemList", CP_WINANSI );
    	hszTop[3] = DdeCreateStringHandle ( srvrInst, "Result", CP_WINANSI );
	    highHsz=3;
    	for(i=4;i<CTOPICS;i++) {
	        hszTop[i]=NULL;
    		}
		}

	if (Tcl_Eval(outinterp,"tk appname") == TCL_OK) {
		strcpy(szDDEString, outinterp->result);
		if (!strlen(szDDEString)) {
			strcpy(szDDEString, "wish");
			}
		}
	GetFinalName(szDDEString,outinterp);
	appendHszInterp( outinterp, szDDEString );
	if(first_del){
		ckfree((char*)firstHI);
		firstHI = lastHI;
		}
	temp = (char *) ckalloc(strlen(szDDEString)+1);

	hszAppName = DdeCreateStringHandle ( srvrInst, "Tcl", CP_WINANSI );
	if (!DdeNameService(srvrInst, hszAppName, (HSZ) NULL, DNS_REGISTER)) {
		print("Got error while naming service.");
		return TCL_OK;
		}
	pointInterp(outinterp, TCL_FUNCTIONS|TK_FUNCTIONS);
	return Tcl_PkgProvide(outinterp, "dde", "1.1");
}
/*
 *----------------------------------------------------------------------
 *
 * appendHszInterp --
 *
 *	This function creates and populates a new hszinterp structure,
 *  and appends it on the list for this dll.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
appendHszInterp(interp, interpName)
    Tcl_Interp *interp;
    char *interpName;
{
    hszinterp *hi=(hszinterp *)ckalloc(sizeof(hszinterp));
    char * tempchar=(char *)ckalloc(strlen(interpName)+1);

    strcpy(tempchar, interpName);

    lastHI->next = hi;
    lastHI = hi;
    
    hi->interpName = DdeCreateStringHandle ( srvrInst, tempchar, CP_WINANSI );
    hi->interp = interp;
    hi->next = (hszinterp *)NULL;    
}


/*
 *----------------------------------------------------------------------
 *
 * Dde_LoadCmd --
 *
 *	This function traps the Tcl "load" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Dde_LoadCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int result;
    char name[255];

    if (argc!=3) {
    	result = Tcl_LoadCmd(clientData, interp, argc, argv);
    } else {
        strcpy(name, argv[2]);
        result = Tcl_LoadCmd(clientData, interp, argc, argv);
    	if (result == TCL_OK) {
    	    if (argv[2]) {
    	        if (strncmp(name,"Tk",2) == 0) {
    	            GetFinalName(name, interp);
                    appendHszInterp(interp, name);
                    pointInterp(interp, TK_FUNCTIONS);
    	        }
    	    }
    	}
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Dde_WinfoCmd --
 *
 *	This function traps the Tcl "winfo" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Dde_WinfoCmd(ClientData clientData, Tcl_Interp *interp, int argc, ARGV_T argv)
{
    int ac, result;
    char **av;
    char *names;

#ifdef	TCL8b1OrBetter
	Tcl_ResetResult(interp);
#endif
    
    if (argc == 2) {
        if (!strcmp("interps", STR_FROM(argv[1]))) {
            GetAllNames(&ac, &av, interp);
            names = Tcl_Merge(ac, av);
#ifdef	TCL8b1OrBetter
			Tcl_SetStringObj(Tcl_GetObjResult(interp),names,-1);
#else
            Tcl_AppendResult(interp, names, (char *)NULL);
#endif
    	    if(av) ckfree((char*)av);
    	    ckfree(names);
	    return TCL_OK;
        }
    }
#ifdef	TCL8b1OrBetter
    result = Tk_WinfoObjCmd(clientData, interp, argc, argv);
#else
    result = Tk_WinfoCmd(clientData, interp, argc, argv);
#endif
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Dde_TkCmd --
 *
 *	This function traps the Tcl "tk" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Dde_TkCmd(ClientData clientData, Tcl_Interp *interp, int argc, ARGV_T argv)
{
    hszinterp *this;
    char name[255];
    int result;

#ifdef	TCL8b1OrBetter
	Tcl_ResetResult(interp);
#endif
    if (argc >= 2) {
        if (!strcmp("appname", STR_FROM(argv[1]))) {
            this = FindHSZInterp(interp, NULL, NULL);
            if (argc > 2) {
                if (this) {
                    DdeFreeStringHandle(srvrInst, this->interpName);
                    strcpy(name, STR_FROM(argv[2]));
        	        GetFinalName(name,interp);
        	        this->interpName = DdeCreateStringHandle(srvrInst, name, CP_WINANSI);
	                result=TCL_OK;
	                goto done;
                }
            } else {
                if (this) {
                    DdeQueryString(srvrInst, this->interpName, name, 255, CP_WINANSI);
#ifdef	TCL8b1OrBetter
					Tcl_SetStringObj(Tcl_GetObjResult(interp),name,-1);
#else
                    Tcl_AppendResult(interp, name, (char *)NULL);
#endif
                    result=TCL_OK;
                    goto done;
                }
            }
#ifdef	TCL8b1OrBetter
			Tcl_SetStringObj(Tcl_GetObjResult(interp),"Couldn't find DDE table entry for this interpreter!",-1);
#else
            Tcl_AppendResult(interp, "Couldn't find DDE table entry for this interpreter!");
#endif
            result=TCL_ERROR;
            goto done;
        }
    }
#ifdef	TCL8b1OrBetter
    result=Tk_TkObjCmd(clientData, interp, argc, argv);
#else
    result=Tk_TkCmd(clientData, interp, argc, argv);
#endif

done:
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Dde_InterpCmd --
 *
 *	This function traps the Tcl "interp" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Dde_InterpCmd(ClientData clientData, Tcl_Interp *interp, int argc, ARGV_T argv)
{
    int result;
#ifdef	TCL8b1OrBetter
	Tcl_ResetResult(interp);
    result = Tcl_InterpObjCmd(clientData, interp, argc, argv);
#else
    result = Tcl_InterpCmd(clientData, interp, argc, argv);
#endif
    
    if (result == TCL_OK && argc >= 2 && !strcmp("create", STR_FROM(argv[1])) ) {
#ifdef	TCL8b1OrBetter
		pointInterp(Tcl_GetSlave(interp,Tcl_GetStringResult(interp)), TCL_FUNCTIONS);
#else
		pointInterp(Tcl_GetSlave(interp,interp->result), TCL_FUNCTIONS);
#endif
        }
	return result;
}
