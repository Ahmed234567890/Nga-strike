#include "upnp.h"
#include "raylib.h"

#include <stddef.h> // upnpcommands.h relies on size_t being defined already
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/igd_desc_parse.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// UPnP port mapping via miniupnpc (https://miniupnp.tuxfamily.org/).
// The game asks the router's Internet Gateway Device to forward one UDP port
// to this machine while hosting. See upnp.h for the safety model.

#define UPNP_LEASE_SECONDS   7200  // router-side lease (2 h)
#define UPNP_REFRESH_SECONDS 3600  // re-add after 1 h (half the lease)
#define UPNP_DISCOVER_MS     2000  // SSDP wait (standard upnpc default)
#define UPNP_DESC            "Nga-Strike"

static struct UPNPUrls g_urls;    // control URLs of the found gateway
static struct IGDdatas g_data;
static char g_lanAddr[64];        // our address towards the gateway
static char g_extIP[46];          // public address as the router reports it
static int  g_port = 0;
static bool g_mapped = false;
static double g_lastAdd = 0.0;    // wall-clock time of the last mapping call

// Private / non-routable IPv4? Detects the address families that mean "this
// port mapping will not be reachable from the internet": RFC1918, loopback,
// link-local and especially CGNAT (100.64.0.0/10).
static bool upnp_private_v4(const char* ip) {
    struct in_addr a;
    if (inet_pton(AF_INET, ip, &a) != 1) return false;
    unsigned int v = ntohl(a.s_addr);
    if ((v >> 24) == 10)   return true; // 10.0.0.0/8
    if ((v >> 24) == 127)  return true; // loopback
    if ((v >> 20) == 0xAC1) return true; // 172.16.0.0/12
    if ((v >> 16) == 0xC0A8) return true; // 192.168.0.0/16
    if ((v >> 22) == 0x191) return true; // 100.64.0.0/10 (CGNAT)
    if ((v >> 16) == 0xA9FE) return true; // link-local
    return false;
}

bool UPnP_IsMapped(void) { return g_mapped; }

bool UPnP_MapPort(int udpPort, char* outPublicIP, int outSize) {
    if (outPublicIP && outSize > 0) outPublicIP[0] = 0;
    if (g_mapped) return true;

    // ---- find the router ----
    TraceLog(LOG_INFO, "UPnP: looking for a router (SSDP)...");
    int err = 0;
    struct UPNPDev* devs = upnpDiscover(UPNP_DISCOVER_MS, NULL, NULL,
                                        UPNP_LOCAL_PORT_ANY, 0, 2, &err);
    if (!devs) {
        TraceLog(LOG_WARNING, "UPnP: no router answered%s%s - staying LAN-only",
                 err ? " (" : "", err ? strupnperror(err) : "");
        return false;
    }
    char wan[46] = "";
    int igd = UPNP_GetValidIGD(devs, &g_urls, &g_data,
                               g_lanAddr, (int)sizeof(g_lanAddr),
                               wan, (int)sizeof(wan));
    freeUPNPDevlist(devs);
    if (igd == 0) { // no IGD found; urls are not allocated in this case
        TraceLog(LOG_WARNING, "UPnP: devices found but no usable internet gateway - staying LAN-only");
        return false;
    }

    // ---- public address (authoritative query; description value as fallback) ----
    char ext[46] = "";
    if (UPNP_GetExternalIPAddress(g_urls.controlURL, g_data.first.servicetype, ext) != 0 || !ext[0])
        snprintf(ext, sizeof(ext), "%s", wan);

    // ---- open the one UDP port the game hosts on ----
    char portTxt[8], lease[16];
    snprintf(portTxt, sizeof(portTxt), "%d", udpPort);
    snprintf(lease, sizeof(lease), "%d", UPNP_LEASE_SECONDS);
    int r = UPNP_AddPortMapping(g_urls.controlURL, g_data.first.servicetype,
                                portTxt, portTxt, g_lanAddr, UPNP_DESC,
                                "UDP", NULL, lease);
    if (r != 0) {
        TraceLog(LOG_WARNING, "UPnP: could not open UDP %d (%s) - staying LAN-only",
                 udpPort, strupnperror(r));
        FreeUPNPUrls(&g_urls);
        return false;
    }

    // ---- confirm the entry (not all routers allow reading it back) ----
    char qClient[64] = "", qPort[8] = "", qDesc[128] = "", qEnum[8] = "", qLease[32] = "";
    bool verified = UPNP_GetSpecificPortMappingEntry(g_urls.controlURL, g_data.first.servicetype,
                                                     portTxt, "UDP", NULL,
                                                     qClient, qPort, qDesc, qEnum, qLease) == 0;
    if (!verified)
        TraceLog(LOG_INFO, "UPnP: router accepted the mapping but does not allow querying it back");

    g_port = udpPort;
    g_mapped = true;
    g_lastAdd = (double)time(NULL);
    snprintf(g_extIP, sizeof(g_extIP), "%s", ext);
    if (outPublicIP && outSize > 0) snprintf(outPublicIP, outSize, "%s", ext);

    TraceLog(LOG_INFO, "UPnP: opened UDP %d -> %s (public %s, lease %dh)%s",
             udpPort, g_lanAddr, ext[0] ? ext : "unknown",
             UPNP_LEASE_SECONDS / 3600, verified ? " [verified]" : "");
    if (ext[0] && upnp_private_v4(ext))
        TraceLog(LOG_WARNING, "UPnP: public address %s is not internet-routable "
                              "(CGNAT/double NAT?) - internet joins may not work; "
                              "try Tailscale/ZeroTier instead", ext);
    return true;
}

void UPnP_Update(void) {
    if (!g_mapped) return;
    double now = (double)time(NULL);
    if (now - g_lastAdd < (double)UPNP_REFRESH_SECONDS) return;

    char portTxt[8], lease[16];
    snprintf(portTxt, sizeof(portTxt), "%d", g_port);
    snprintf(lease, sizeof(lease), "%d", UPNP_LEASE_SECONDS);
    int r = UPNP_AddPortMapping(g_urls.controlURL, g_data.first.servicetype,
                                portTxt, portTxt, g_lanAddr, UPNP_DESC,
                                "UDP", NULL, lease);
    g_lastAdd = now; // back off a full interval either way
    if (r == 0) TraceLog(LOG_DEBUG, "UPnP: lease refreshed");
    else        TraceLog(LOG_WARNING, "UPnP: lease refresh failed (%s)", strupnperror(r));
}

void UPnP_UnmapPort(void) {
    if (!g_mapped) return;
    char portTxt[8];
    snprintf(portTxt, sizeof(portTxt), "%d", g_port);
    int r = UPNP_DeletePortMapping(g_urls.controlURL, g_data.first.servicetype,
                                   portTxt, "UDP", NULL);
    if (r == 0) TraceLog(LOG_INFO, "UPnP: closed UDP %d", g_port);
    else        TraceLog(LOG_WARNING,
                        "UPnP: could not remove the mapping (%s); it expires with the lease",
                        strupnperror(r));
    FreeUPNPUrls(&g_urls);
    g_mapped = false;
    g_port = 0;
    g_extIP[0] = 0;
}
