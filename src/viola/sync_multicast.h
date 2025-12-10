/*
 * sync_multicast.h - UDP broadcast/multicast for real-time sync
 *
 * Uses UDP broadcast (IPv4) and multicast (IPv6) for instant message delivery
 * between ViolaWWW instances. All browsers listen on the same port and
 * broadcast/multicast sync messages. Messages include page hash so only
 * matching pages process them.
 *
 * IPv4: Uses broadcast (255.255.255.255)
 * IPv6: Uses link-local multicast group (ff02::1:d47b)
 */

#ifndef SYNC_MULTICAST_H
#define SYNC_MULTICAST_H

/*
 * Initialize broadcast/multicast sockets.
 * Creates IPv4 socket always, IPv6 socket if available.
 * Returns 1 on success, 0 on failure.
 */
int sync_multicast_init(void);

/*
 * Set current page URL for filtering incoming messages.
 * Computes hash internally.
 */
void sync_multicast_set_page(const char* url);

/*
 * Get current page hash.
 */
unsigned int sync_multicast_get_hash(void);

/*
 * Broadcast a sync message to all peers.
 * Only peers with matching page hash will process it.
 * Sends on both IPv4 and IPv6 if available.
 * Format: id=object name, func=function to call, args=pipe-separated arguments
 */
void sync_multicast_broadcast(const char* id, const char* func, const char* args);

/*
 * Process incoming messages (non-blocking).
 * Call this from the main event loop.
 * Reads from both IPv4 and IPv6 sockets.
 */
void sync_multicast_process(void);

/*
 * Get IPv4 socket fd for external select() integration.
 * Returns -1 if not initialized.
 */
int sync_multicast_get_fd(void);

/*
 * Get IPv6 socket fd for external select() integration.
 * Returns -1 if not initialized or IPv6 not available.
 */
int sync_multicast_get_fd6(void);

/*
 * Check if IPv6 multicast is enabled.
 * Returns 1 if IPv6 is available and initialized, 0 otherwise.
 */
int sync_multicast_has_ipv6(void);

/*
 * Shutdown broadcast/multicast.
 */
void sync_multicast_shutdown(void);

#endif /* SYNC_MULTICAST_H */

