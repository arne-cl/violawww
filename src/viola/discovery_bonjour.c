/*
 * discovery_bonjour.c - Bonjour/DNS-SD peer discovery for ViolaWWW (macOS)
 *
 * Copyright 2025. Part of the ViolaWWW restoration project.
 *
 * Uses Apple's DNS Service Discovery API to:
 * 1. Register this browser instance as a service
 * 2. Browse for other ViolaWWW instances on the network
 * 3. Detect when peers are viewing the same page (by URL hash)
 */

#ifdef __DARWIN__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dns_sd.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "discovery_bonjour.h"

/* Service type for ViolaWWW browsers */
#define VIOLA_SERVICE_TYPE "_violawww._tcp"
#define VIOLA_SERVICE_PORT 8519  /* Arbitrary port, not actually used */

/* Maximum peers to track */
#define MAX_PEERS 32

/* Peer information */
typedef struct {
    char name[64];
    char host[256];
    unsigned int hash;
    int active;
} BonjourPeer;

/* Module state */
static struct {
    int initialized;
    DNSServiceRef register_ref;
    DNSServiceRef browse_ref;
    char instance_name[64];
    unsigned int current_hash;
    char current_url[1024];
    BonjourPeer peers[MAX_PEERS];
    int peer_count;
} bonjour_state = {0};

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

/*
 * Build TXT record with current page hash
 */
static void build_txt_record(TXTRecordRef* txt)
{
    char hash_str[16];
    
    TXTRecordCreate(txt, 0, NULL);
    
    snprintf(hash_str, sizeof(hash_str), "%08x", bonjour_state.current_hash);
    TXTRecordSetValue(txt, "hash", strlen(hash_str), hash_str);
    
    /* Also include truncated URL for debugging */
    if (bonjour_state.current_url[0]) {
        int url_len = strlen(bonjour_state.current_url);
        if (url_len > 200) url_len = 200;
        TXTRecordSetValue(txt, "url", url_len, bonjour_state.current_url);
    }
}

/*
 * Find peer by name
 */
static BonjourPeer* find_peer(const char* name)
{
    for (int i = 0; i < MAX_PEERS; i++) {
        if (bonjour_state.peers[i].active && 
            strcmp(bonjour_state.peers[i].name, name) == 0) {
            return &bonjour_state.peers[i];
        }
    }
    return NULL;
}

/*
 * Add or update peer
 */
static BonjourPeer* add_peer(const char* name)
{
    /* Check if already exists */
    BonjourPeer* peer = find_peer(name);
    if (peer) return peer;
    
    /* Find empty slot */
    for (int i = 0; i < MAX_PEERS; i++) {
        if (!bonjour_state.peers[i].active) {
            peer = &bonjour_state.peers[i];
            memset(peer, 0, sizeof(*peer));
            strncpy(peer->name, name, sizeof(peer->name) - 1);
            peer->active = 1;
            bonjour_state.peer_count++;
            return peer;
        }
    }
    return NULL;
}

/*
 * Remove peer
 */
static void remove_peer(const char* name)
{
    BonjourPeer* peer = find_peer(name);
    if (peer) {
        peer->active = 0;
        bonjour_state.peer_count--;
        fprintf(stderr, "[Bonjour] Peer left: %s\n", name);
    }
}

/*
 * Check if peer has same hash and notify
 */
static void check_peer_match(BonjourPeer* peer)
{
    if (!peer || !peer->active) return;
    
    /* Skip self */
    if (strcmp(peer->name, bonjour_state.instance_name) == 0) return;
    
    if (peer->hash == bonjour_state.current_hash && bonjour_state.current_hash != 0) {
        fprintf(stderr, "[Bonjour] *** PEER MATCH! %s is viewing the same page ***\n", 
                peer->name);
        fprintf(stderr, "[Bonjour]     URL hash: %08x\n", peer->hash);
        if (bonjour_state.current_url[0]) {
            fprintf(stderr, "[Bonjour]     URL: %s\n", bonjour_state.current_url);
        }
    }
}

/*
 * Callback for TXT record resolution
 */
static void DNSSD_API resolve_callback(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    DNSServiceErrorType errorCode,
    const char* fullname,
    const char* hosttarget,
    uint16_t port,
    uint16_t txtLen,
    const unsigned char* txtRecord,
    void* context)
{
    BonjourPeer* peer = (BonjourPeer*)context;
    
    if (errorCode != kDNSServiceErr_NoError) {
        fprintf(stderr, "[Bonjour] Resolve error: %d\n", errorCode);
        DNSServiceRefDeallocate(sdRef);
        return;
    }
    
    if (!peer || !peer->active) {
        DNSServiceRefDeallocate(sdRef);
        return;
    }
    
    strncpy(peer->host, hosttarget, sizeof(peer->host) - 1);
    
    /* Parse TXT record for hash */
    uint8_t hash_len;
    const char* hash_val = (const char*)TXTRecordGetValuePtr(txtLen, txtRecord, "hash", &hash_len);
    
    if (hash_val && hash_len > 0) {
        char hash_str[16] = {0};
        int copy_len = (hash_len < 15) ? hash_len : 15;
        memcpy(hash_str, hash_val, copy_len);
        
        unsigned int old_hash = peer->hash;
        peer->hash = (unsigned int)strtoul(hash_str, NULL, 16);
        
        /* Check for match */
        if (peer->hash != old_hash) {
            check_peer_match(peer);
        }
    }
    
    DNSServiceRefDeallocate(sdRef);
}

/*
 * Callback for browse results
 */
static void DNSSD_API browse_callback(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    DNSServiceErrorType errorCode,
    const char* serviceName,
    const char* regtype,
    const char* replyDomain,
    void* context)
{
    if (errorCode != kDNSServiceErr_NoError) {
        fprintf(stderr, "[Bonjour] Browse error: %d\n", errorCode);
        return;
    }
    
    if (flags & kDNSServiceFlagsAdd) {
        /* New service found */
        BonjourPeer* peer = add_peer(serviceName);
        if (peer) {
            fprintf(stderr, "[Bonjour] Peer found: %s\n", serviceName);
            
            /* Resolve to get TXT record */
            DNSServiceRef resolve_ref;
            DNSServiceErrorType err = DNSServiceResolve(
                &resolve_ref,
                0,
                interfaceIndex,
                serviceName,
                regtype,
                replyDomain,
                resolve_callback,
                peer);
            
            if (err == kDNSServiceErr_NoError) {
                /* Process resolve immediately */
                DNSServiceProcessResult(resolve_ref);
            }
        }
    } else {
        /* Service removed */
        remove_peer(serviceName);
    }
}

/*
 * Callback for registration
 */
static void DNSSD_API register_callback(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    DNSServiceErrorType errorCode,
    const char* name,
    const char* regtype,
    const char* domain,
    void* context)
{
    if (errorCode == kDNSServiceErr_NoError) {
        strncpy(bonjour_state.instance_name, name, sizeof(bonjour_state.instance_name) - 1);
        fprintf(stderr, "[Bonjour] Registered as: %s\n", name);
    } else {
        fprintf(stderr, "[Bonjour] Registration error: %d\n", errorCode);
    }
}

/*
 * Initialize Bonjour
 */
int discovery_bonjour_init(void)
{
    DNSServiceErrorType err;
    TXTRecordRef txt;
    
    if (bonjour_state.initialized) return 1;
    
    fprintf(stderr, "[Bonjour] Initializing peer discovery...\n");
    
    /* Generate unique instance name */
    char hostname[64] = "ViolaWWW";
    gethostname(hostname, sizeof(hostname) - 1);
    snprintf(bonjour_state.instance_name, sizeof(bonjour_state.instance_name),
             "ViolaWWW-%s-%d", hostname, getpid());
    
    /* Build initial TXT record */
    build_txt_record(&txt);
    
    /* Register service */
    err = DNSServiceRegister(
        &bonjour_state.register_ref,
        0,                          /* flags */
        0,                          /* interfaceIndex (all) */
        bonjour_state.instance_name,
        VIOLA_SERVICE_TYPE,
        NULL,                       /* domain (default) */
        NULL,                       /* host (default) */
        htons(VIOLA_SERVICE_PORT),
        TXTRecordGetLength(&txt),
        TXTRecordGetBytesPtr(&txt),
        register_callback,
        NULL);
    
    TXTRecordDeallocate(&txt);
    
    if (err != kDNSServiceErr_NoError) {
        fprintf(stderr, "[Bonjour] Failed to register service: %d\n", err);
        return 0;
    }
    
    /* Browse for other instances */
    err = DNSServiceBrowse(
        &bonjour_state.browse_ref,
        0,                          /* flags */
        0,                          /* interfaceIndex (all) */
        VIOLA_SERVICE_TYPE,
        NULL,                       /* domain (default) */
        browse_callback,
        NULL);
    
    if (err != kDNSServiceErr_NoError) {
        fprintf(stderr, "[Bonjour] Failed to start browsing: %d\n", err);
        DNSServiceRefDeallocate(bonjour_state.register_ref);
        return 0;
    }
    
    bonjour_state.initialized = 1;
    fprintf(stderr, "[Bonjour] Peer discovery active\n");
    
    return 1;
}

/*
 * Update current page
 */
void discovery_bonjour_set_page(const char* url)
{
    if (!bonjour_state.initialized || !url) return;
    
    unsigned int new_hash = djb2_hash(url);
    
    /* Skip if same page */
    if (new_hash == bonjour_state.current_hash) return;
    
    bonjour_state.current_hash = new_hash;
    strncpy(bonjour_state.current_url, url, sizeof(bonjour_state.current_url) - 1);
    
    fprintf(stderr, "[Bonjour] Page changed: %08x %s\n", new_hash, url);
    
    /* Update TXT record */
    TXTRecordRef txt;
    build_txt_record(&txt);
    
    DNSServiceErrorType err = DNSServiceUpdateRecord(
        bonjour_state.register_ref,
        NULL,                       /* primary record */
        0,                          /* flags */
        TXTRecordGetLength(&txt),
        TXTRecordGetBytesPtr(&txt),
        0);                         /* TTL (default) */
    
    TXTRecordDeallocate(&txt);
    
    if (err != kDNSServiceErr_NoError) {
        fprintf(stderr, "[Bonjour] Failed to update record: %d\n", err);
    }
    
    /* Check existing peers for matches */
    for (int i = 0; i < MAX_PEERS; i++) {
        if (bonjour_state.peers[i].active) {
            check_peer_match(&bonjour_state.peers[i]);
        }
    }
}

/*
 * Process pending events
 */
void discovery_bonjour_process(void)
{
    if (!bonjour_state.initialized) return;
    
    /* Process browse events */
    if (bonjour_state.browse_ref) {
        int fd = DNSServiceRefSockFD(bonjour_state.browse_ref);
        if (fd >= 0) {
            fd_set readfds;
            struct timeval tv = {0, 0}; /* non-blocking */
            
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);
            
            if (select(fd + 1, &readfds, NULL, NULL, &tv) > 0) {
                DNSServiceProcessResult(bonjour_state.browse_ref);
            }
        }
    }
    
    /* Process register events */
    if (bonjour_state.register_ref) {
        int fd = DNSServiceRefSockFD(bonjour_state.register_ref);
        if (fd >= 0) {
            fd_set readfds;
            struct timeval tv = {0, 0};
            
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);
            
            if (select(fd + 1, &readfds, NULL, NULL, &tv) > 0) {
                DNSServiceProcessResult(bonjour_state.register_ref);
            }
        }
    }
}

/*
 * Get file descriptor for external select
 */
int discovery_bonjour_get_fd(void)
{
    if (!bonjour_state.initialized || !bonjour_state.browse_ref) return -1;
    return DNSServiceRefSockFD(bonjour_state.browse_ref);
}

/*
 * Shutdown
 */
void discovery_bonjour_shutdown(void)
{
    if (!bonjour_state.initialized) return;
    
    fprintf(stderr, "[Bonjour] Shutting down...\n");
    
    if (bonjour_state.browse_ref) {
        DNSServiceRefDeallocate(bonjour_state.browse_ref);
        bonjour_state.browse_ref = NULL;
    }
    
    if (bonjour_state.register_ref) {
        DNSServiceRefDeallocate(bonjour_state.register_ref);
        bonjour_state.register_ref = NULL;
    }
    
    bonjour_state.initialized = 0;
}

/*
 * Get current hash (for debugging)
 */
unsigned int discovery_bonjour_get_hash(void)
{
    return bonjour_state.current_hash;
}

#endif /* __DARWIN__ */

