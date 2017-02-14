#ifdef __WIN32__
#include <windows.h>
#include <winsock.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifndef __WIN32__
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netdb.h>
#endif

#include "netlink.h"
#include "proto.h"
#include "ring.h"
#include "externs.h"

/* In linux, this is an enum */
#ifdef __linux__
#define HAS_IPPROTO_IP
#endif

#ifdef IPPROTO_IP
#define HAS_IPPROTO_IP
#endif


netlink nlink;

class netchannel : public ringbuf::source {
  public:
    virtual int read(char *buf, int maxlen) {
	int net = nlink.getfd();
	int l = recv(net, buf, maxlen, 0);
	if (l<0 && errno == WSAEWOULDBLOCK) l = 0;
	return l;
    }
};

class netchannel2 : public datasink {
  public:
    virtual int write(const char *buf, int len) {
	int r = nlink.send(buf, len, 0);
	if (r==-1 && (errno==WSAENOBUFS || errno==WSAEWOULDBLOCK)) return 0;
	return r;
    }
    virtual int writeurg(const char *buf, int len) {
	    /*
	     * In 4.2 (and 4.3) systems, there is some question about
	     * what byte in a sendOOB operation is the "OOB" data.
	     * To make ourselves compatible, we only send ONE byte
	     * out of band, the one WE THINK should be OOB (though
	     * we really have more the TCP philosophy of urgent data
	     * rather than the Unix philosophy of OOB data).
	     */
	if (len==0) return 0;
	int r = nlink.send(buf, 1, MSG_OOB);
	if (r==-1 && (errno==WSAENOBUFS || errno==WSAEWOULDBLOCK)) r = 0;
	if (r<=0) return r;
	int rr = nlink.send(buf+1, len-r, 0);
	if (rr==-1 && (errno==WSAENOBUFS || errno==WSAEWOULDBLOCK)) rr = 0;
	if (rr<=0) return r;   /* less than ideal */
	return r+rr;
    }
};

static netchannel chan;
static netchannel2 chan2;
datasink *netsink = &chan2;
ringbuf::source *netsrc = &chan;


/*
 * SetSockOpt()
 *
 * Compensate for differences in 4.2 and 4.3 systems.
 */
static int SetSockOpt(int fd, int level, int option, int yesno) {
#ifndef	NOT43
    return setsockopt(fd, level, option, (char *)&yesno, sizeof(yesno));
#else	/* NOT43 */
    if (yesno == 0) {		/* Can't do that in 4.2! */
	fprintf(stderr, "Error: attempt to turn off an option 0x%x.\n",
				option);
	return -1;
    }
    return setsockopt(fd, level, option, 0, 0);
#endif	/* NOT43 */
}


netlink::netlink() { net = INVALID_SOCKET; }
netlink::~netlink() { ::closesocket(net); }


int netlink::setdebug(int debug) {
#ifndef	NOT43
    if (net > 0 &&
	(SetSockOpt(net, SOL_SOCKET, SO_DEBUG, debug)) < 0) {
	sock_perror("setsockopt (SO_DEBUG)");
    }
#else	/* NOT43 */
    if (debug) {
	if (net > 0 && SetSockOpt(net, SOL_SOCKET, SO_DEBUG, 0, 0) < 0)
	    sock_perror("setsockopt (SO_DEBUG)");
    } else {
	printf("Cannot turn off socket debugging\n");
	fflush(stdout);
    }
#endif	/* NOT43 */
    return 1;
}

void netlink::close(int doshutdown) {
    if (doshutdown) {
	shutdown(net, 2);
    }
    ::closesocket(net);
}

int netlink::connect(int debug, struct hostent *host, 
		     struct sockaddr_in *sn, 
		     const char *srcroute, int srlen, int tos) 
{
    net = socket(AF_INET, SOCK_STREAM, 0);
#ifndef __WIN32__
    setuid(getuid());
    if (net < 0) {
	sock_perror("telnet: socket");
	return 0;
    }
#endif

#if defined(IP_OPTIONS) && defined(HAS_IPPROTO_IP)
    if (srcroute) {
	if (setsockopt(net, IPPROTO_IP, IP_OPTIONS, srcroute, srlen) < 0)
	    sock_perror("setsockopt (IP_OPTIONS)");
    }
#endif

#if defined(HAS_IPPROTO_IP) && defined(IP_TOS)
#if defined(HAS_GETTOS)
    struct tosent *tp;
    if (tos < 0 && (tp = gettosbyname("telnet", "tcp")))
	tos = tp->t_tos;
#endif
    if (tos < 0) tos = 020;	/* Low Delay bit */
    if (tos && (setsockopt(net, IPPROTO_IP, IP_TOS, (char *) &tos, sizeof(int)) < 0)
	&& (errno != WSAENOPROTOOPT))
	sock_perror("telnet: setsockopt (IP_TOS) (ignored)");
#endif	/* defined(IPPROTO_IP) && defined(IP_TOS) */

    if (debug && SetSockOpt(net, SOL_SOCKET, SO_DEBUG, 1) < 0) {
	sock_perror("setsockopt (SO_DEBUG)");
    }
    
    if (::connect(net, (struct sockaddr *)sn, sizeof(*sn)) < 0) {
#if defined(h_addr)		/* In 4.3, this is a #define */
	if (host && host->h_addr_list[1]) {
	    int oerrno = errno;
	    
	    fprintf(stderr, "telnet: connect to address %s: ",
		    inet_ntoa(sn->sin_addr));
	    errno = oerrno;
	    sock_perror(NULL);
	    host->h_addr_list++;
	    memcpy(&sn->sin_addr, host->h_addr_list[0], host->h_length);
	    close(net);
	    return 1;
	}
#endif	/* defined(h_addr) */

	sock_perror("telnet: Unable to connect to remote host");
	return 0;
    }
    return 2;
}


void netlink::oobinline() {
    /* Systems without SO_OOBINLINE probably won't work */
    if (SetSockOpt(net, SOL_SOCKET, SO_OOBINLINE, 1) == -1) {
	sock_perror("SetSockOpt");
    }
}


/*
 * Check to see if any out-of-band data exists on a socket (for
 * Telnet "synch" processing).
 */

int netlink::stilloob(void) {
    static struct timeval timeout = { 0 };
    fd_set excepts;
    int value;

    do {
	FD_ZERO(&excepts);
	FD_SET(net, &excepts);
	value = select(net+1, NULL, NULL, &excepts, &timeout);
    } while ((value == -1) && (errno == WSAEINTR));

    if (value < 0) {
	sock_perror("select");
	quit();
	/* NOTREACHED */
    }
    if (FD_ISSET(net, &excepts)) {
	return 1;
    } else {
	return 0;
    }
}

int netlink::send(const char *s, int n, int f) {
    return ::send(net, s, n, f);
}

void netlink::nonblock(int onoff) {
    ioctlsocket(net, FIONBIO, (unsigned long *) &onoff);
}

SOCKET netlink::getfd() {
    return net;
}
