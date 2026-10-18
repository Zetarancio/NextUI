#include "btagent.hpp"
#include "../common/bt_policy.h"
#include <iostream>
#include <cstring>

extern "C" void PLAT_bluetoothSetDiscoveryOwner(int (*set_fn)(int on), int (*get_fn)(void));
extern "C" void PLAT_bluetoothSetScanOwner(int (*fn)(struct bt_seen *out, int max));

static Bluetooth::PairingAgent *disc_owner = nullptr;
static constexpr int BT_DBUS_MS = 2000;

extern "C" int zlyme_bt_disc_set(int on)
{
    if (!disc_owner)
        return 0;
    if (on)
        return disc_owner->startDiscovery() ? 1 : 0;
    disc_owner->stopDiscovery();
    return 1;
}

extern "C" int zlyme_bt_disc_get(void)
{
    return disc_owner && disc_owner->discovering() ? 1 : 0;
}

extern "C" int zlyme_bt_scan(struct bt_seen *out, int max)
{
    if (!disc_owner || !out || max <= 0)
        return 0;
    return disc_owner->copyScan(out, max);
}

static constexpr const char* AGENT_PATH = "/com/nextui/agent";

static void agent_method_call(GDBusConnection*,
                                const gchar*,
                                const gchar*,
                                const gchar*,
                                const gchar* method_name,
                                GVariant*,
                                GDBusMethodInvocation* invocation,
                                gpointer);

static void properties_changed(GDBusConnection*,
                                const gchar*,
                                const gchar*,
                                const gchar* object_path,
                                const gchar* interface,
                                GVariant* params,
                                gpointer user_data);

using namespace Bluetooth;

PairingAgent::PairingAgent()
{
    bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, nullptr);
}

void PairingAgent::startPairingWindow()
{
    if (agent_registered)
        return;

    registerAgent();
    subscribeDeviceSignals();
    setAdapterPairable(true);

    std::cout << "Pairing window opened\n";
}

void PairingAgent::registerAgent()
{
    static const GDBusInterfaceVTable vtable = {
        agent_method_call, nullptr, nullptr
    };

    static const gchar* xml = R"XML(
<node>
 <interface name="org.bluez.Agent1">
  <method name="Release"/>
  <method name="RequestPinCode">
   <arg type="o" direction="in"/>
   <arg type="s" direction="out"/>
  </method>
  <method name="RequestPasskey">
   <arg type="o" direction="in"/>
   <arg type="u" direction="out"/>
  </method>
  <method name="RequestConfirmation">
   <arg type="o" direction="in"/>
   <arg type="u" direction="in"/>
  </method>
  <method name="RequestAuthorization">
   <arg type="o" direction="in"/>
  </method>
  <method name="Cancel"/>
 </interface>
</node>
)XML";

    introspection = g_dbus_node_info_new_for_xml(xml, nullptr);

    agent_reg_id = g_dbus_connection_register_object(
        bus,
        AGENT_PATH,
        introspection->interfaces[0],
        &vtable,
        nullptr, nullptr, nullptr);

    std::cout << "Agent registered at " << AGENT_PATH << "\n";

    g_dbus_connection_call_sync(
        bus, "org.bluez", "/org/bluez",
        "org.bluez.AgentManager1",
        "RegisterAgent",
        g_variant_new("(os)", AGENT_PATH, "NoInputNoOutput"),
        nullptr, G_DBUS_CALL_FLAGS_NONE, BT_DBUS_MS, nullptr, nullptr);

    std::cout << "Agent registered with BlueZ\n";

    g_dbus_connection_call_sync(
        bus, "org.bluez", "/org/bluez",
        "org.bluez.AgentManager1",
        "RequestDefaultAgent",
        g_variant_new("(o)", AGENT_PATH),
        nullptr, G_DBUS_CALL_FLAGS_NONE, BT_DBUS_MS, nullptr, nullptr);
    
    std::cout << "Agent set as default\n";

    agent_registered = true;
}

void PairingAgent::subscribeDeviceSignals()
{
    signal_id = g_dbus_connection_signal_subscribe(
        bus,
        "org.bluez",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        nullptr,
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        properties_changed,
        this,
        nullptr);
    
    std::cout << "Subscribed to device property changes\n";
}

void PairingAgent::stopPairingWindow()
{
    if (!agent_registered)
        return;

    g_dbus_connection_call_sync(
        bus, "org.bluez", "/org/bluez",
        "org.bluez.AgentManager1",
        "UnregisterAgent",
        g_variant_new("(o)", AGENT_PATH),
        nullptr, G_DBUS_CALL_FLAGS_NONE, BT_DBUS_MS, nullptr, nullptr);

    std::cout << "Agent unregistered from BlueZ\n";

    setAdapterPairable(false);

    std::cout << "Adapter set to non-pairable\n";

    g_dbus_connection_signal_unsubscribe(bus, signal_id);
    g_dbus_connection_unregister_object(bus, agent_reg_id);

    agent_registered = false;

    std::cout << "Pairing window closed\n";
}

void PairingAgent::setAdapterPairable(bool on)
{
    g_dbus_connection_call_sync(
        bus,
        "org.bluez",
        "/org/bluez/hci0",
        "org.freedesktop.DBus.Properties",
        "Set",
        g_variant_new("(ssv)", "org.bluez.Adapter1",
                      "Discoverable",
                      g_variant_new_boolean(on)),
        nullptr, G_DBUS_CALL_FLAGS_NONE, BT_DBUS_MS, nullptr, nullptr);

    std::cout << "Adapter Discoverable set to " << (on ? "true" : "false") << "\n";

    g_dbus_connection_call_sync(
        bus,
        "org.bluez",
        "/org/bluez/hci0",
        "org.freedesktop.DBus.Properties",
        "Set",
        g_variant_new("(ssv)", "org.bluez.Adapter1",
                        "Pairable",
                        g_variant_new_boolean(on)),
        nullptr, G_DBUS_CALL_FLAGS_NONE, BT_DBUS_MS, nullptr, nullptr);
    
    std::cout << "Adapter Pairable set to " << (on ? "true" : "false") << "\n";
}

bool PairingAgent::ensureBus()
{
    if (bus && !g_dbus_connection_is_closed(bus))
        return true;
    if (bus) {
        g_object_unref(bus);
        bus = nullptr;
        agent_registered = false;
    }
    bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, nullptr);
    return bus != nullptr;
}

bool PairingAgent::adapterBool(const char *name, bool *out)
{
    GError *err = nullptr;
    GVariant *ret;
    GVariant *val = nullptr;

    if (!ensureBus())
        return false;
    ret = g_dbus_connection_call_sync(
        bus, "org.bluez", "/org/bluez/hci0",
        "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", "org.bluez.Adapter1", name),
        G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE, BT_DBUS_MS, nullptr, &err);
    if (!ret) {
        if (err)
            g_error_free(err);
        return false;
    }
    g_variant_get(ret, "(v)", &val);
    if (val && g_variant_is_of_type(val, G_VARIANT_TYPE_BOOLEAN))
        *out = g_variant_get_boolean(val);
    else {
        if (val)
            g_variant_unref(val);
        g_variant_unref(ret);
        return false;
    }
    g_variant_unref(val);
    g_variant_unref(ret);
    return true;
}

bool PairingAgent::startDiscovery()
{
    GError *err = nullptr;
    bool powered = false;
    bool already = false;
    int result;

    if (!ensureBus())
        return false;
    if (!adapterBool("Powered", &powered) || !powered)
        return false;
    if (adapterBool("Discovering", &already) && already)
        return true;

    g_dbus_connection_call_sync(
        bus, "org.bluez", "/org/bluez/hci0",
        "org.bluez.Adapter1", "StartDiscovery",
        nullptr, nullptr,
        G_DBUS_CALL_FLAGS_NONE, BT_DBUS_MS, nullptr, &err);
    result = bt_disc_classify(1, err ? err->message : nullptr);
    if (err) {
        std::cout << "StartDiscovery: " << err->message << "\n";
        g_error_free(err);
    }
    already = false;
    adapterBool("Discovering", &already);
    return bt_disc_is_active(result, already ? 1 : 0) != 0;
}

void PairingAgent::stopDiscovery()
{
    GError *err = nullptr;

    if (!bus || g_dbus_connection_is_closed(bus))
        return;
    g_dbus_connection_call_sync(
        bus, "org.bluez", "/org/bluez/hci0",
        "org.bluez.Adapter1", "StopDiscovery",
        nullptr, nullptr,
        G_DBUS_CALL_FLAGS_NONE, BT_DBUS_MS, nullptr, &err);
    if (err)
        g_error_free(err);
}

bool PairingAgent::discovering()
{
    bool on = false;

    if (!adapterBool("Discovering", &on))
        return false;
    return on;
}

int PairingAgent::copyScan(::bt_seen *out, int max)
{
    GError *err = nullptr;
    GVariant *ret;
    GVariant *body;
    GVariantIter iter;
    const char *path = nullptr;
    GVariant *ifaces = nullptr;
    int count = 0;

    if (!ensureBus() || !out || max <= 0)
        return 0;
    ret = g_dbus_connection_call_sync(
        bus, "org.bluez", "/",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects",
        nullptr, G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
        G_DBUS_CALL_FLAGS_NONE, BT_DBUS_MS, nullptr, &err);
    if (!ret) {
        if (err)
            g_error_free(err);
        return 0;
    }
    body = g_variant_get_child_value(ret, 0);
    g_variant_iter_init(&iter, body);
    while (count < max && g_variant_iter_next(&iter, "{&o@a{sa{sv}}}", &path, &ifaces)) {
        GVariant *device;
        GVariant *dict = nullptr;
        GVariant *v;
        char addr[18];
        char name[249];
        char icon[64];
        unsigned int class_val = 0;
        int paired = 0;
        int kind;
        int have_name = 0;

        addr[0] = name[0] = icon[0] = '\0';
        if (!path || !strstr(path, "/dev_")) {
            g_variant_unref(ifaces);
            continue;
        }
        device = g_dbus_connection_call_sync(
            bus, "org.bluez", path,
            "org.freedesktop.DBus.Properties", "GetAll",
            g_variant_new("(s)", "org.bluez.Device1"),
            G_VARIANT_TYPE("(a{sv})"),
            G_DBUS_CALL_FLAGS_NONE, BT_DBUS_MS, nullptr, nullptr);
        g_variant_unref(ifaces);
        if (!device)
            continue;
        g_variant_get(device, "(@a{sv})", &dict);
        if (!dict) {
            g_variant_unref(device);
            continue;
        }
        v = g_variant_lookup_value(dict, "Address", G_VARIANT_TYPE_STRING);
        if (v) {
            strncpy(addr, g_variant_get_string(v, nullptr), sizeof(addr) - 1);
            g_variant_unref(v);
        }
        v = g_variant_lookup_value(dict, "Name", G_VARIANT_TYPE_STRING);
        if (!v)
            v = g_variant_lookup_value(dict, "Alias", G_VARIANT_TYPE_STRING);
        if (v) {
            strncpy(name, g_variant_get_string(v, nullptr), sizeof(name) - 1);
            have_name = name[0] != '\0';
            g_variant_unref(v);
        }
        v = g_variant_lookup_value(dict, "Icon", G_VARIANT_TYPE_STRING);
        if (v) {
            strncpy(icon, g_variant_get_string(v, nullptr), sizeof(icon) - 1);
            g_variant_unref(v);
        }
        v = g_variant_lookup_value(dict, "Class", G_VARIANT_TYPE_UINT32);
        if (v) {
            class_val = g_variant_get_uint32(v);
            g_variant_unref(v);
        }
        v = g_variant_lookup_value(dict, "Paired", G_VARIANT_TYPE_BOOLEAN);
        if (v) {
            paired = g_variant_get_boolean(v) ? 1 : 0;
            g_variant_unref(v);
        }
        g_variant_unref(dict);
        g_variant_unref(device);
        if (!bt_mac_ok(addr))
            continue;
        kind = bt_kind_from_class(class_val);
        if (!kind)
            kind = bt_kind_from_icon(icon);
        memset(&out[count], 0, sizeof(out[count]));
        strncpy(out[count].addr, addr, sizeof(out[count].addr) - 1);
        strncpy(out[count].name, have_name ? name : addr, sizeof(out[count].name) - 1);
        out[count].kind = kind;
        out[count].paired = paired;
        count++;
    }
    g_variant_unref(body);
    g_variant_unref(ret);
    return count;
}

void PairingAgent::bindDiscoveryOwner()
{
    disc_owner = this;
    PLAT_bluetoothSetDiscoveryOwner(zlyme_bt_disc_set, zlyme_bt_disc_get);
    PLAT_bluetoothSetScanOwner(zlyme_bt_scan);
}

void PairingAgent::unbindDiscoveryOwner()
{
    if (disc_owner == this)
        disc_owner = nullptr;
    PLAT_bluetoothSetDiscoveryOwner(nullptr, nullptr);
    PLAT_bluetoothSetScanOwner(nullptr);
}

static void agent_method_call(GDBusConnection*,
                                const gchar*,
                                const gchar*,
                                const gchar*,
                                const gchar* method_name,
                                GVariant*,
                                GDBusMethodInvocation* invocation,
                                gpointer)
{
    if (strcmp(method_name, "RequestPinCode") == 0) {
        std::cout << "RequestPinCode called\n";
        g_dbus_method_invocation_return_value(
            invocation, g_variant_new("(s)", "0000"));
        return;
    }

    if (strcmp(method_name, "RequestPasskey") == 0) {
        std::cout << "RequestPasskey called\n";
        g_dbus_method_invocation_return_value(
            invocation, g_variant_new("(u)", 0));
        return;
    }

    g_dbus_method_invocation_return_value(invocation, nullptr);
}

static void properties_changed(GDBusConnection*,
                                const gchar*,
                                const gchar*,
                                const gchar* object_path,
                                const gchar* interface,
                                GVariant* params,
                                gpointer user_data)
{
    auto* self = static_cast<PairingAgent*>(user_data);

    const gchar* iface = nullptr;
    GVariantIter* iter = nullptr;
    GVariant* val = nullptr;
    const gchar* key = nullptr;

    /* params = (sa{sv}as) */
    g_variant_get(params, "(&sa{sv}as)", &iface, &iter, nullptr);

    /* Adapter1 Discovering is read on the next poll. A Device1
     * Paired signal still closes the pairing window. */
    if (strcmp(iface, "org.bluez.Device1") != 0) {
        g_variant_iter_free(iter);
        return;
    }

    while (g_variant_iter_next(iter, "{sv}", &key, &val)) {
        if (strcmp(key, "Paired") == 0 &&
            g_variant_get_boolean(val)) {

            std::cout << "Device paired: " << object_path << "\n";
            self->stopPairingWindow();
        }
        g_variant_unref(val);
    }

    g_variant_iter_free(iter);
}