#pragma once
#include <gio/gio.h>
#include <glib.h>
#include "../common/bt_policy.h"

namespace Bluetooth
{
class PairingAgent {
public:
    PairingAgent();
    void startPairingWindow();
    void stopPairingWindow();
    /* StartDiscovery and StopDiscovery use this object's bus. */
    bool startDiscovery();
    void stopDiscovery();
    bool discovering();
    int copyScan(::bt_seen *out, int max);
    void bindDiscoveryOwner();
    void unbindDiscoveryOwner();

private:
    void registerAgent();
    void subscribeDeviceSignals();
    void setAdapterPairable(bool on);
    bool adapterBool(const char *name, bool *out);
    bool ensureBus();

private:
    GDBusConnection* bus = nullptr;
    GDBusNodeInfo* introspection = nullptr;
    guint agent_reg_id = 0;
    guint signal_id = 0;
    bool agent_registered = false;
};
} // namespace Bluetooth