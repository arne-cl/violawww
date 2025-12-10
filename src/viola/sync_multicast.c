/*
 * sync_multicast.c - UDP broadcast/multicast for real-time sync
 *
 * Uses UDP broadcast (IPv4) and multicast (IPv6) for instant message delivery
 * between ViolaWWW instances. All browsers listen on the same port and
 * broadcast/multicast sync messages. Messages include page hash so only
 * matching pages process them.
 *
 * IPv4: Uses broadcast (255.255.255.255)
 * IPv6: Uses link-local multicast group (ff02::1:d47b)
 */

#include "sync_multicast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <net/if.h>

/* External function to register socket with Xt input handler */
extern void registerSyncSocket(int fd);

#ifdef __APPLE__
/* External function to start wakeup timer when sync is enabled */
extern void vw_start_wakeup_timer(void);
#endif

/*
 * djb2 hash function - simple and effective for strings
 */
static unsigned int djb2_hash(const char* str)
{
    unsigned int hash = 5381;
    int c;
    
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    
    return hash;
}

/* Broadcast port for sync messages */
#define SYNC_PORT 54379

/* IPv6 multicast group for sync (link-local scope)
 * ff02 = link-local multicast
 * ::1:d47b = 1:54379 (port number in hex as part of group address)
 */
#define SYNC_MCAST6_ADDR "ff02::1:d47b"

/* Message format: "VIOLA|hash|instance|seq|id|func|args" */
#define MSG_PREFIX "VIOLA"
#define MAX_MSG_SIZE 1024

/* Forward declaration */
extern void discovery_dispatch_sync(const char* id, const char* func, const char* args);

/* Module state */
static struct {
    int initialized;
    int sock_fd;                      /* IPv4 socket */
    int sock6_fd;                     /* IPv6 socket (-1 if not available) */
    struct sockaddr_in bcast_addr;    /* IPv4 broadcast destination */
    struct sockaddr_in6 mcast6_addr;  /* IPv6 multicast destination */
    unsigned int page_hash;
    unsigned int send_seq;
    unsigned long instance_id;        /* Unique ID for this browser instance */
} sync_state = {0};

/*
 * Initialize IPv6 multicast socket (internal helper)
 * Returns socket fd or -1 if IPv6 not available
 */
static int init_ipv6_socket(void)
{
    int sock6;
    int reuse = 1;
    int flags;
    struct sockaddr_in6 bind_addr6;
    struct ipv6_mreq mreq;
    
    /* Create IPv6 UDP socket */
    sock6 = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock6 < 0) {
        /* IPv6 not available - this is fine, we'll use IPv4 only */
        return -1;
    }
    
    /* Allow multiple sockets to use same port */
    if (setsockopt(sock6, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("[Sync] IPv6 SO_REUSEADDR");
    }
    
#ifdef SO_REUSEPORT
    if (setsockopt(sock6, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        perror("[Sync] IPv6 SO_REUSEPORT");
    }
#endif
    
    /* Don't receive IPv4-mapped addresses on this socket */
    int v6only = 1;
    if (setsockopt(sock6, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0) {
        perror("[Sync] IPV6_V6ONLY");
    }
    
    /* Bind to receive multicasts */
    memset(&bind_addr6, 0, sizeof(bind_addr6));
    bind_addr6.sin6_family = AF_INET6;
    bind_addr6.sin6_addr = in6addr_any;
    bind_addr6.sin6_port = htons(SYNC_PORT);
    
    if (bind(sock6, (struct sockaddr*)&bind_addr6, sizeof(bind_addr6)) < 0) {
        perror("[Sync] IPv6 bind");
        close(sock6);
        return -1;
    }
    
    /* Join multicast group on all interfaces */
    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(AF_INET6, SYNC_MCAST6_ADDR, &mreq.ipv6mr_multiaddr) != 1) {
        fprintf(stderr, "[Sync] Invalid IPv6 multicast address\n");
        close(sock6);
        return -1;
    }
    mreq.ipv6mr_interface = 0;  /* All interfaces */
    
    if (setsockopt(sock6, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0) {
        perror("[Sync] IPV6_JOIN_GROUP");
        close(sock6);
        return -1;
    }
    
    /* Enable multicast loopback so we can test with multiple instances on same machine */
    int loop = 1;
    if (setsockopt(sock6, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
        perror("[Sync] IPV6_MULTICAST_LOOP");
    }
    
    /* Set multicast hops (TTL) - 1 for link-local */
    int hops = 1;
    if (setsockopt(sock6, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops)) < 0) {
        perror("[Sync] IPV6_MULTICAST_HOPS");
    }
    
    /* Set non-blocking */
    flags = fcntl(sock6, F_GETFL, 0);
    fcntl(sock6, F_SETFL, flags | O_NONBLOCK);
    
    /* Setup multicast destination address */
    memset(&sync_state.mcast6_addr, 0, sizeof(sync_state.mcast6_addr));
    sync_state.mcast6_addr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, SYNC_MCAST6_ADDR, &sync_state.mcast6_addr.sin6_addr);
    sync_state.mcast6_addr.sin6_port = htons(SYNC_PORT);
    
    return sock6;
}

/*
 * Initialize UDP broadcast/multicast sockets
 */
int sync_multicast_init(void)
{
    int sock;
    int reuse = 1;
    int bcast = 1;
    int flags;
    
    if (sync_state.initialized) return 1;
    
    /* Initialize IPv6 socket to -1 (not available) */
    sync_state.sock6_fd = -1;
    
    /* Initializing UDP broadcast */
    
    /* Create IPv4 UDP socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("[Sync] socket");
        return 0;
    }
    
    /* Allow multiple sockets to use same port */
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("[Sync] SO_REUSEADDR");
    }
    
#ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        perror("[Sync] SO_REUSEPORT");
    }
#endif
    
    /* Enable broadcast */
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast)) < 0) {
        perror("[Sync] SO_BROADCAST");
    }
    
    /* Bind to receive broadcasts */
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(SYNC_PORT);
    
    if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("[Sync] bind");
        close(sock);
        return 0;
    }
    
    /* Set non-blocking */
    flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    /* Setup broadcast destination (255.255.255.255) */
    memset(&sync_state.bcast_addr, 0, sizeof(sync_state.bcast_addr));
    sync_state.bcast_addr.sin_family = AF_INET;
    sync_state.bcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sync_state.bcast_addr.sin_port = htons(SYNC_PORT);
    
    sync_state.sock_fd = sock;
    sync_state.initialized = 1;
    sync_state.send_seq = 0;
    
    /* Generate unique instance ID from PID, time, and hostname */
    {
        char hostname[256];
        unsigned long host_hash = 0;
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            host_hash = djb2_hash(hostname);
        }
        sync_state.instance_id = ((unsigned long)getpid() << 32) 
                               | ((unsigned long)time(NULL) ^ host_hash);
    }
    
    /* Try to initialize IPv6 socket (optional) */
    sync_state.sock6_fd = init_ipv6_socket();
    
    /* Register IPv4 socket with Xt for immediate notification */
    registerSyncSocket(sock);
    
    /* Register IPv6 socket if available */
    if (sync_state.sock6_fd >= 0) {
        registerSyncSocket(sync_state.sock6_fd);
    }
    
#ifdef __APPLE__
    /* Start wakeup timer now that sync is enabled */
    vw_start_wakeup_timer();
#endif
    
    return 1;
}

/*
 * Set current page URL - computes hash for filtering
 */
void sync_multicast_set_page(const char* url)
{
    if (url && url[0]) {
        sync_state.page_hash = djb2_hash(url);
    } else {
        sync_state.page_hash = 0;
    }
}

/*
 * Get current page hash
 */
unsigned int sync_multicast_get_hash(void)
{
    return sync_state.page_hash;
}

/*
 * Broadcast a sync message (sends on both IPv4 and IPv6 if available)
 */
void sync_multicast_broadcast(const char* id, const char* func, const char* args)
{
    char msg[MAX_MSG_SIZE];
    int len;
    
    if (!sync_state.initialized || !id || !func) return;
    if (sync_state.page_hash == 0) return;
    
    sync_state.send_seq++;
    
    /* Format: VIOLA|hash|instance|seq|id|func|args */
    len = snprintf(msg, sizeof(msg), "%s|%08x|%016lx|%u|%s|%s|%s",
                   MSG_PREFIX,
                   sync_state.page_hash,
                   sync_state.instance_id,
                   sync_state.send_seq,
                   id, func,
                   args ? args : "");
    
    /* Send via IPv4 broadcast */
    sendto(sync_state.sock_fd, msg, len, 0,
           (struct sockaddr*)&sync_state.bcast_addr,
           sizeof(sync_state.bcast_addr));
    
    /* Send via IPv6 multicast if available */
    if (sync_state.sock6_fd >= 0) {
        sendto(sync_state.sock6_fd, msg, len, 0,
               (struct sockaddr*)&sync_state.mcast6_addr,
               sizeof(sync_state.mcast6_addr));
    }
}

/*
 * Process messages from a single socket (internal helper)
 */
static void process_socket(int sock_fd)
{
    char buf[MAX_MSG_SIZE];
    ssize_t len;
    struct sockaddr_storage from_addr;
    socklen_t from_len;
    
    /* Read all available messages from this socket */
    while (1) {
        from_len = sizeof(from_addr);
        len = recvfrom(sock_fd, buf, sizeof(buf) - 1, 0,
                       (struct sockaddr*)&from_addr, &from_len);
        
        if (len <= 0) break;
        
        buf[len] = '\0';
        
        /* Parse message: VIOLA|hash|instance|seq|id|func|args */
        char* p = buf;
        char* prefix = strsep(&p, "|");
        char* hash_str = strsep(&p, "|");
        char* inst_str = strsep(&p, "|");
        char* seq_str = strsep(&p, "|");
        char* id = strsep(&p, "|");
        char* func = strsep(&p, "|");
        char* args = p;  /* Rest is args */
        
        if (!prefix || !hash_str || !inst_str || !seq_str || !id || !func) continue;
        if (strcmp(prefix, MSG_PREFIX) != 0) continue;
        
        /* Check page hash */
        unsigned int msg_hash = (unsigned int)strtoul(hash_str, NULL, 16);
        if (msg_hash != sync_state.page_hash) continue;
        
        /* Skip our own messages */
        unsigned long msg_inst = strtoul(inst_str, NULL, 16);
        if (msg_inst == sync_state.instance_id) continue;
        
        (void)seq_str;  /* Sequence number available if needed */
        
        /* Dispatch to object */
        discovery_dispatch_sync(id, func, args);
    }
}

/*
 * Process incoming messages from all sockets
 */
void sync_multicast_process(void)
{
    if (!sync_state.initialized) return;
    
    /* Process IPv4 socket */
    process_socket(sync_state.sock_fd);
    
    /* Process IPv6 socket if available */
    if (sync_state.sock6_fd >= 0) {
        process_socket(sync_state.sock6_fd);
    }
}

/*
 * Get IPv4 socket fd for select()
 */
int sync_multicast_get_fd(void)
{
    return sync_state.initialized ? sync_state.sock_fd : -1;
}

/*
 * Get IPv6 socket fd for select() (-1 if not available)
 */
int sync_multicast_get_fd6(void)
{
    return sync_state.initialized ? sync_state.sock6_fd : -1;
}

/*
 * Check if IPv6 is enabled
 */
int sync_multicast_has_ipv6(void)
{
    return sync_state.initialized && sync_state.sock6_fd >= 0;
}

/*
 * Shutdown
 */
void sync_multicast_shutdown(void)
{
    if (!sync_state.initialized) return;
    
    /* Close IPv4 socket */
    close(sync_state.sock_fd);
    
    /* Close IPv6 socket if open */
    if (sync_state.sock6_fd >= 0) {
        close(sync_state.sock6_fd);
        sync_state.sock6_fd = -1;
    }
    
    sync_state.initialized = 0;
    fprintf(stderr, "[Sync] Shutdown\n");
}
