/*
 * discovery.c - Peer discovery dispatcher
 *
 * This file routes the platform-independent discovery API to the appropriate
 * platform-specific implementation based on compile-time detection.
 *
 * Supported platforms:
 *   - macOS (__DARWIN__): Uses Bonjour/DNS-SD via discovery_bonjour.c
 *   - Linux (__linux__): Will use Avahi via discovery_avahi.c (TODO)
 *   - Other: Stub implementation (all functions are no-ops)
 *
 * To add support for a new platform:
 *   1. Create discovery_<platform>.h and discovery_<platform>.c
 *   2. Add a new #elif block below with the platform detection macro
 *   3. Include the header and route all functions to the implementation
 *
 * Copyright 2025. Part of the ViolaWWW restoration project.
 */

#include "discovery.h"
#include <stdio.h>

/*
 * Global state for discovery enable/disable
 * Discovery is only activated when a page contains SC attributes.
 */
static int discovery_enabled_flag = 0;

/*
 * discovery_enable - Called when SC attribute is encountered
 */
void discovery_enable(void)
{
    if (!discovery_enabled_flag) {
        fprintf(stderr, "[Discovery] Enabled by SC attribute\n");
        discovery_enabled_flag = 1;
    }
}

/*
 * discovery_reset - Called when starting to load a new page
 */
void discovery_reset(void)
{
    discovery_enabled_flag = 0;
}

/*
 * discovery_is_enabled - Check if discovery is enabled
 */
int discovery_is_enabled(void)
{
    return discovery_enabled_flag;
}

/* ============================================================================
 * macOS Implementation - Bonjour/DNS-SD
 * ============================================================================
 * Uses Apple's DNS Service Discovery framework which is built into macOS.
 * Registers a "_violawww._tcp" service and browses for other instances.
 */
#ifdef __DARWIN__

#include "discovery_bonjour.h"

int discovery_init(void) {
    return discovery_bonjour_init();
}

void discovery_set_page(const char* url) {
    /* Only proceed if discovery was enabled by SC attribute */
    if (!discovery_enabled_flag) {
        return;  /* No SC attributes on this page, skip discovery */
    }
    
    /* Lazy initialization - discovery is enabled on first set_page call */
    static int lazy_init_done = 0;
    if (!lazy_init_done) {
        fprintf(stderr, "[Discovery] Initializing (page has SC attributes)\n");
        discovery_bonjour_init();
        lazy_init_done = 1;
    }
    
    fprintf(stderr, "[Discovery] Page: %s\n", url ? url : "(null)");
    discovery_bonjour_set_page(url);
}

void discovery_process(void) {
    discovery_bonjour_process();
}

int discovery_get_fd(void) {
    return discovery_bonjour_get_fd();
}

void discovery_shutdown(void) {
    discovery_bonjour_shutdown();
}

unsigned int discovery_get_hash(void) {
    return discovery_bonjour_get_hash();
}

int discovery_supported(void) {
    return 1;
}

/* ============================================================================
 * Linux Implementation - Avahi (TODO)
 * ============================================================================
 * Will use libavahi-client to provide mDNS/DNS-SD functionality.
 * Avahi is the standard zero-configuration networking implementation on Linux.
 */
#elif defined(__linux__)

/* TODO: Implement discovery_avahi.c and uncomment:
 * #include "discovery_avahi.h"
 */

int discovery_init(void) {
    fprintf(stderr, "[Discovery] Avahi support not yet implemented for Linux\n");
    return 0;
}

void discovery_set_page(const char* url) {
    (void)url;  /* Unused parameter */
}

void discovery_process(void) {
    /* No-op until Avahi is implemented */
}

int discovery_get_fd(void) {
    return -1;  /* Not available */
}

void discovery_shutdown(void) {
    /* No-op until Avahi is implemented */
}

unsigned int discovery_get_hash(void) {
    return 0;  /* No hash available */
}

int discovery_supported(void) {
    return 0;  /* Will return 1 when Avahi is implemented */
}

/* ============================================================================
 * Unsupported Platform - Stub Implementation
 * ============================================================================
 * For platforms without mDNS support, all functions are no-ops.
 * This allows the code to compile and run without discovery features.
 */
#else

int discovery_init(void) {
    /* Discovery not supported on this platform */
    return 0;
}

void discovery_set_page(const char* url) {
    (void)url;  /* Unused parameter */
}

void discovery_process(void) {
    /* No-op */
}

int discovery_get_fd(void) {
    return -1;  /* Not available */
}

void discovery_shutdown(void) {
    /* No-op */
}

unsigned int discovery_get_hash(void) {
    return 0;  /* No hash available */
}

int discovery_supported(void) {
    return 0;  /* Discovery not available */
}

#endif /* Platform selection */
