
class netlink {
 protected:
    SOCKET net;
 public:
    netlink();
    ~netlink();

    int connect(int debug, struct hostent *host, 
		struct sockaddr_in *sin, 
		const char *srcroute, int srlen,
		int tos);
    void close(int doshutdown);

    int setdebug(int debug);
    void oobinline();
    void nonblock(int onoff);

    int stilloob();

    int send(const char *buf, int len, int flags);

    SOCKET getfd();
};

extern netlink nlink;
