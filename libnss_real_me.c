#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <nss.h>

#include <arpa/inet.h>

#define SERVER4 "v4.ifconfig.co"
#define SERVER6 "v6.ifconfig.co"
#define HTTP_QUERY4              \
        "GET / HTTP/1.1\r\n"    \
        "Host: "SERVER4 "\r\n"    \
        "User-Agent: curl\r\n"  \
        "Connection: close\r\n" \
        "\r\n"
#define HTTP_QUERY6              \
        "GET / HTTP/1.1\r\n"    \
        "Host: "SERVER6 "\r\n"    \
        "User-Agent: curl\r\n"  \
        "Connection: close\r\n" \
        "\r\n"


#define ALIGN(x) (((x+sizeof(void*)-1)/sizeof(void*))*sizeof(void*))

inline size_t ADDRLEN (int proto) {
    return proto == AF_INET6 ? sizeof(struct in6_addr) : sizeof(struct in_addr);
}

static int
connect_server(int family)
{
	int sock, ret;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

	if (family == AF_INET)
		ret = getaddrinfo(SERVER4, "http", &hints, &res);
	else
		ret = getaddrinfo(SERVER6, "http", &hints, &res);

	if (ret != 0)
		return -1;

	if (family == AF_INET)
		sock = socket(AF_INET, SOCK_STREAM, 0);
	else
		sock = socket(AF_INET6, SOCK_STREAM, 0);

	ret = connect(sock, res->ai_addr, res->ai_addrlen);
	if (ret < 0) {
		close(sock);
		return -1;
	}

	freeaddrinfo(res);

	return sock;
}

static int
disconnect_server(int sock)
{
	return close(sock);
}

static int
find_real_my_addr(int family, char *address) {
	int sock, ret;
	char response[BUFSIZ];
	char *ip;

	sock = connect_server(family);
	if (sock < 0)
		return -1;

	if (family == AF_INET)
		ret = send(sock, HTTP_QUERY4, strlen(HTTP_QUERY4), 0);
	else
		ret = send(sock, HTTP_QUERY6, strlen(HTTP_QUERY6), 0);

	if (ret < 0) {
		disconnect_server(sock);
		return -1;
	}

	ret = recv(sock, response, BUFSIZ, 0);
	if (ret < 0) {
		disconnect_server(sock);
		return -1;
	}

	if ((strncmp(response, "HTTP/1.1 200 OK", 15) == 0)) {
                ip = strstr(response, "\r\n\r\n");
                if (ip) {
                        sscanf(ip, "\r\n\r\n%s", address);
                } else {
                        disconnect_server(sock);
                        return -1;
                }
        } else {
                disconnect_server(sock);
                return -1;
        }


	disconnect_server(sock);

	return 0;
}

static enum nss_status
fill_real_my_addr(const char *name,
		  int af,
                  struct hostent * result,
                  char *buffer,
                  size_t buflen,
                  int *errnop,
                  int *h_errnop)
{
	size_t n, offset, size;
        char *addr, *hostname, *aliases, *addr_list;
        size_t alen;
	char real_me[48];

	memset(real_me, 0, sizeof(real_me));

	if (find_real_my_addr(af, real_me) < 0) {
		*errnop = EAGAIN;
		*h_errnop = NO_RECOVERY;
                return NSS_STATUS_TRYAGAIN;
        }

	alen = ADDRLEN(af);

        n = strlen(name) + 1;
        size = ALIGN(n) + sizeof(char*) + ALIGN(alen) + sizeof(char*) * 2;
        if (buflen < size) {
                *errnop = ENOMEM;
                *h_errnop = NO_RECOVERY;
                return NSS_STATUS_TRYAGAIN;
        }

        // hostname
        hostname = buffer;
        memcpy(hostname, name, n);
        offset = ALIGN(n);

        // aliase
        aliases = buffer + offset;
        *(char**) aliases = NULL;
        offset += sizeof(char*);

        // address
        addr = buffer + offset;
	if (af == AF_INET)
		inet_pton(AF_INET, real_me, addr);
	else
		inet_pton(AF_INET6, real_me, addr);

        offset += ALIGN(alen);

        // address list
        addr_list = buffer + offset;
        ((char**) addr_list)[0] = addr;
        ((char**) addr_list)[1] = NULL;
        offset += sizeof(char*) * 2;

        result->h_name = hostname;
        result->h_aliases = (char**) aliases;
        result->h_addrtype = af;
        result->h_length = alen;
        result->h_addr_list = (char**) addr_list;

	return NSS_STATUS_SUCCESS;
}

enum nss_status
_nss_real_me_gethostbyname2_r(const char *name,
			      int af,
                              struct hostent * result,
	                      char *buffer,
	                      size_t buflen,
	                      int *errnop,
	                      int *h_errnop)
{
	if (af == AF_UNSPEC)
		af = AF_INET;

	if (af != AF_INET && af != AF_INET6) {
		*errnop = EINVAL;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_UNAVAIL;
	}

	if (!strcmp(name, "me.localhost")) {
		return fill_real_my_addr(name,
					 af,
				         result,
					 buffer,
					 buflen,
					 errnop,
                                         h_errnop);
	} else {
		*errnop = EINVAL;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_UNAVAIL;
	}
}

enum nss_status
_nss_real_me_gethostbyname_r(const char *name,
			     struct hostent * result,
	                     char *buffer,
	                     size_t buflen,
	                     int *errnop,
	                     int *h_errnop)
{
	return _nss_real_me_gethostbyname2_r(name,
                                             AF_UNSPEC,
                                             result,
                                             buffer,
                                             buflen,
                                             errnop,
                                             h_errnop);
}

