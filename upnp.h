#ifndef UPNP_H
#define UPNP_H

#include <stdbool.h>

// Best-effort UPnP (IGD) port mapping so a LAN host can also be joined from
// the internet without manual router configuration.
//
// Safety model:
//   * only ever maps the single UDP port the game itself hosts on
//   * the mapping carries a limited lease (~2 h) that is refreshed hourly,
//     so even a crash self-cleans instead of leaving a permanent hole
//   * the mapping is removed on clean exit
//   * every failure path is non-fatal: the game simply stays LAN-only
//
// All functions are safe to call when UPnP is unavailable or never mapped.

// Try to map udpPort on the router found via SSDP ("UPnP").
// On success returns true and writes the router's public IPv4 into
// outPublicIP ("" when the router does not report one). Logs progress and
// the reason for any failure.
bool UPnP_MapPort(int udpPort, char* outPublicIP, int outSize);

// Re-add the mapping shortly before the lease expires (no-op otherwise).
// Cheap: call once per frame while hosting.
void UPnP_Update(void);

// Remove the mapping (clean shutdown). No-op when nothing is mapped.
void UPnP_UnmapPort(void);

bool UPnP_IsMapped(void);

#endif
