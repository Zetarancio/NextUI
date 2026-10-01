#include "zlymemenu.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <mutex>
#include <sys/wait.h>
#include <unistd.h>
#include <fstream>
#include <string>
#include <vector>

static std::string trim(std::string s)
{
	while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
		s.pop_back();
	return s;
}

static std::string ctl_get(const char *name)
{
	std::string cmd = std::string("zlyme-ctl get ") + name;
	FILE *f = popen(cmd.c_str(), "r");
	if (!f)
		return "";
	char buf[128] = {0};
	if (!fgets(buf, sizeof(buf), f)) {
		pclose(f);
		return "";
	}
	pclose(f);
	return trim(buf);
}

static bool ctl_on(const char *name)
{
	std::string v = ctl_get(name);
	return v == "on" || v == "1" || v == "yes";
}

static void ctl_set(const char *name, const char *val)
{
	std::string cmd = std::string("zlyme-ctl set ") + name + " " + val;
	system(cmd.c_str());
}

static int g_leave_settings = 0;

void Zlyme_requestLeave(void)
{
	g_leave_settings = 1;
}

int Zlyme_leaveRequested(void)
{
	return g_leave_settings;
}

void Zlyme_captureBootState(void)
{
	system("zlyme-bootcfg capture /tmp/zlyme-bootcfg");
}

static int wait_ab_game(const std::string &msg, const char *aLabel, const char *bLabel);

static void service_apply(const char *name, const char *init, bool on)
{
	ctl_set(name, on ? "on" : "off");
	std::string cmd = std::string(init) + (on ? " start" : " stop");
	system(cmd.c_str());
}

static std::string hdmi_conn()
{
	const char *cands[] = {
		"/sys/class/drm/card0-HDMI-A-1",
		"/sys/class/drm/card1-HDMI-A-1",
		nullptr,
	};
	for (int i = 0; cands[i]; i++) {
		std::ifstream in(std::string(cands[i]) + "/modes");
		if (in.good())
			return cands[i];
	}
	return "";
}

static std::vector<std::string> hdmi_modes()
{
	std::vector<std::string> out;
	std::string conn = hdmi_conn();
	if (conn.empty())
		return out;
	std::ifstream in(conn + "/modes");
	std::string m;
	while (in >> m)
		out.push_back(m);
	return out;
}

InputReactionHint Zlyme_cycleHdmi(AbstractMenuItem &item)
{
	(void)item;
	auto modes = hdmi_modes();
	if (modes.empty()) {
		MenuList::showOverlay("DSI 640x480@60 only", OverlayDismissMode::DismissOnA);
		return NoOp;
	}
	std::string cur = ctl_get("display_mode");
	std::string next = modes[0];
	bool found = false;
	for (size_t i = 0; i < modes.size(); i++) {
		if (found) {
			next = modes[i];
			break;
		}
		if (modes[i] == cur)
			found = true;
	}
	ctl_set("display_mode", next.c_str());
	std::string conn = hdmi_conn();
	if (!conn.empty()) {
		std::ofstream out(conn + "/mode");
		if (out)
			out << next;
	}
	MenuList::showOverlay(std::string("Display resolution ") + next, OverlayDismissMode::DismissOnA);
	return NoOp;
}

void Zlyme_appendDisplayItems(std::vector<AbstractMenuItem *> &items)
{
	const std::vector<std::any> hz_v = {std::string("60"), std::string("50"), std::string("40")};
	const std::vector<std::string> hz_l = {"60 Hz", "50 Hz (PAL)", "40 Hz"};
	items.push_back(new MenuItem{ListItemType::Generic, "Panel refresh",
		"DSI modes from the ROCKNIX panel timings (60 / 50 / 40).",
		hz_v, hz_l,
		[]() -> std::any {
			std::string r = ctl_get("refresh");
			if (r != "50" && r != "40")
				r = "60";
			return r;
		},
		[](const std::any &v) {
			ctl_set("refresh", std::any_cast<std::string>(v).c_str());
			system("zlyme-ctl apply-refresh");
		},
		[]() {
			ctl_set("refresh", "60");
			system("zlyme-ctl apply-refresh");
		}});
}

static InputReactionHint Zlyme_backup(AbstractMenuItem &item)
{
	(void)item;
	int r = system("sync; tar -acf /storage/zlyme-backup.tar.gz -C /storage .config");
	MenuList::showOverlay(r == 0 ? "Saved /storage/zlyme-backup.tar.gz" : "Backup failed",
		OverlayDismissMode::DismissOnA);
	return NoOp;
}

static InputReactionHint Zlyme_restoreBackup(AbstractMenuItem &item)
{
	(void)item;
	if (!std::ifstream("/storage/zlyme-backup.tar.gz")) {
		MenuList::showOverlay("No /storage/zlyme-backup.tar.gz", OverlayDismissMode::DismissOnA);
		return NoOp;
	}
	int r = system("tar -axf /storage/zlyme-backup.tar.gz -C /storage && sync");
	MenuList::showOverlay(r == 0 ? "Restored. Reboot to apply." : "Restore failed",
		OverlayDismissMode::DismissOnA);
	return NoOp;
}

void Zlyme_appendNetworkItems(std::vector<AbstractMenuItem *> &items)
{
	const std::vector<std::any> on_off_v = {false, true};
	const std::vector<std::string> on_off = {"Off", "On"};

	items.push_back(new MenuItem{ListItemType::Generic, "SSH",
		"OpenSSH with SFTP. Applies immediately.",
		on_off_v, on_off,
		[]() -> std::any { return ctl_on("ssh"); },
		[](const std::any &v) { service_apply("ssh", "/etc/init.d/S50sshd", std::any_cast<bool>(v)); },
		[]() { service_apply("ssh", "/etc/init.d/S50sshd", true); }});
	items.push_back(new MenuItem{ListItemType::Generic, "Samba",
		"File share of /storage. Applies immediately.",
		on_off_v, on_off,
		[]() -> std::any { return ctl_on("samba"); },
		[](const std::any &v) { service_apply("samba", "/etc/init.d/S70samba", std::any_cast<bool>(v)); },
		[]() { service_apply("samba", "/etc/init.d/S70samba", false); }});
	items.push_back(new MenuItem{ListItemType::Generic, "Syncthing",
		"Web UI on :8384. Applies immediately.",
		on_off_v, on_off,
		[]() -> std::any { return ctl_on("syncthing"); },
		[](const std::any &v) { service_apply("syncthing", "/etc/init.d/S75syncthing", std::any_cast<bool>(v)); },
		[]() { service_apply("syncthing", "/etc/init.d/S75syncthing", false); }});
}

void Zlyme_appendStatusLed(std::vector<AbstractMenuItem *> &items)
{
	const std::vector<std::any> led_v = {
		std::string("battery"), std::string("green"), std::string("red"), std::string("off")};
	const std::vector<std::string> led_l = {"Auto", "Green", "Red", "Off"};
	items.push_back(new MenuItem{ListItemType::Generic, "Status LED",
		"Auto: green, red charging, flash if low.\nGreen/Red/Off lock the colour.",
		led_v, led_l,
		[]() -> std::any {
			std::string l = ctl_get("led");
			if (l != "green" && l != "red" && l != "off" && l != "amber")
				l = "battery";
			if (l == "amber")
				l = "red";
			return l;
		},
		[](const std::any &v) {
			std::string l = std::any_cast<std::string>(v);
			ctl_set("led", l.c_str());
			std::string cmd = std::string("zlyme-led ") + l;
			system(cmd.c_str());
		},
		[]() {
			ctl_set("led", "battery");
			system("zlyme-led battery");
		}});
}

static int run_reset(const char *mode)
{
	std::string cmd = std::string("zlyme-reset ") + mode;
	int status = system(cmd.c_str());
	return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void request_session_reboot(void)
{
	FILE *f = fopen("/tmp/reboot", "w");
	if (f)
		fclose(f);
	Zlyme_requestLeave();
}

static InputReactionHint Zlyme_resetSettings(AbstractMenuItem &item)
{
	(void)item;
	if (!wait_ab_game(
		"Reset Zlyme settings?\nGames, saves, Wi-Fi and paired devices are kept.",
		"RESET", "BACK"))
		return NoOp;
	if (!run_reset("settings")) {
		MenuList::showOverlay("Reset failed", OverlayDismissMode::DismissOnA);
		return NoOp;
	}
	MenuList::showOverlay("Settings reset", OverlayDismissMode::DismissOnA);
	return NoOp;
}

static InputReactionHint Zlyme_factoryReset(AbstractMenuItem &item)
{
	(void)item;
	if (!wait_ab_game(
		"Factory reset Zlyme?\nSettings and stock Tools/Emus will be restored.\nGames, saves, Wi-Fi and personal content are kept.",
		"RESET", "BACK"))
		return NoOp;
	if (!run_reset("factory")) {
		MenuList::showOverlay("Reset failed", OverlayDismissMode::DismissOnA);
		return NoOp;
	}
	request_session_reboot();
	return NoOp;
}

void Zlyme_appendSystemItems(std::vector<AbstractMenuItem *> &items)
{
	std::vector<AbstractMenuItem *> advanced;
	const std::vector<std::any> gpu_v = {std::string("panfrost"), std::string("libmali")};
	const std::vector<std::string> gpu_l = {"Panfrost", "mali_kbase"};
	const std::vector<std::any> uv_v = {
		std::string("off"), std::string("l1"), std::string("l2"), std::string("l3")};
	const std::vector<std::string> uv_l = {"Off", "L1", "L2", "L3"};

	advanced.push_back(new MenuItem{ListItemType::Generic, "GPU",
		"libmali (GLES+Vulkan) or Panfrost GLES.\nTakes effect on next boot.",
		gpu_v, gpu_l,
		[]() -> std::any {
			std::string g = ctl_get("gpu");
			return g.empty() ? std::string("libmali") : g;
		},
		[](const std::any &v) { ctl_set("gpu", std::any_cast<std::string>(v).c_str()); },
		[]() { ctl_set("gpu", "libmali"); }});
	advanced.push_back(new MenuItem{ListItemType::Generic, "CPU undervolt",
		"ROCKNIX opp-table overlays.\nTakes effect on next boot.",
		uv_v, uv_l,
		[]() -> std::any {
			std::string u = ctl_get("undervolt");
			if (u != "l1" && u != "l2" && u != "l3")
				u = "off";
			return u;
		},
		[](const std::any &v) {
			ctl_set("undervolt", std::any_cast<std::string>(v).c_str());
			system("zlyme-ctl apply-overlays");
		},
		[]() {
			ctl_set("undervolt", "off");
			system("zlyme-ctl apply-overlays");
		}});

	const std::vector<std::any> on_off_v = {false, true};
	const std::vector<std::string> on_off = {"Off", "On"};
	advanced.push_back(new MenuItem{ListItemType::Generic, "ZRAM swap",
		"384 MiB lz4 OOM net on 1 GiB.\nOff if a heavy emu feels spongy.",
		on_off_v, on_off,
		[]() -> std::any { return ctl_on("zram"); },
		[](const std::any &v) {
			ctl_set("zram", std::any_cast<bool>(v) ? "on" : "off");
			system("zlyme-ctl apply-zram");
		},
		[]() {
			ctl_set("zram", "on");
			system("zlyme-ctl apply-zram");
		}});
	advanced.push_back(new MenuItem{ListItemType::Generic, "USB OTG (top)",
		"Turning it off saves a little power.\nTakes effect on next boot.",
		on_off_v, on_off,
		[]() -> std::any { return ctl_on("otg"); },
		[](const std::any &v) {
			ctl_set("otg", std::any_cast<bool>(v) ? "on" : "off");
			system("zlyme-ctl apply-overlays");
		},
		[]() {
			ctl_set("otg", "on");
			system("zlyme-ctl apply-overlays");
		}});
	advanced.push_back(new MenuItem{ListItemType::Generic, "HDMI port",
		"Turning it off saves some power.\nTakes effect on next boot.",
		on_off_v, on_off,
		[]() -> std::any { return ctl_on("hdmi"); },
		[](const std::any &v) {
			ctl_set("hdmi", std::any_cast<bool>(v) ? "on" : "off");
			system("zlyme-ctl apply-overlays");
		},
		[]() {
			ctl_set("hdmi", "on");
			system("zlyme-ctl apply-overlays");
		}});
	advanced.push_back(new MenuItem{ListItemType::Generic, "Second SD slot",
		"Turning it off saves some power.\nTakes effect on next boot.",
		on_off_v, on_off,
		[]() -> std::any { return ctl_on("sd2"); },
		[](const std::any &v) {
			ctl_set("sd2", std::any_cast<bool>(v) ? "on" : "off");
			system("zlyme-ctl apply-overlays");
		},
		[]() {
			ctl_set("sd2", "on");
			system("zlyme-ctl apply-overlays");
		}});
	advanced.push_back(new MenuItem{ListItemType::Generic, "System logs",
		"Write boot and per-pak logs under /storage/.logs.",
		on_off_v, on_off,
		[]() -> std::any { return ctl_on("logs"); },
		[](const std::any &v) {
			ctl_set("logs", std::any_cast<bool>(v) ? "on" : "off");
			system("zlyme-ctl apply-logs");
		},
		[]() {
			ctl_set("logs", "off");
			system("zlyme-ctl apply-logs");
		}});
	advanced.push_back(new MenuItem{ListItemType::Button, "Reset Settings",
		"Return product settings to defaults.\nGames, saves, Wi-Fi and paired devices stay.",
		Zlyme_resetSettings});
	advanced.push_back(new MenuItem{ListItemType::Button, "Factory Reset",
		"Restore settings and stock Tools/Emus.\nGames, saves, Wi-Fi and personal content stay.",
		Zlyme_factoryReset});
	items.push_back(new MenuItem{ListItemType::Generic, "Advanced",
		"GPU, power features, logs, and reset.",
		{}, {}, nullptr, nullptr, DeferToSubmenu,
		new MenuList(MenuItemType::Fixed, "Advanced", advanced)});
}

void Zlyme_appendBackupItem(std::vector<AbstractMenuItem *> &items)
{
	items.push_back(new MenuItem{ListItemType::Button, "Backup now",
		"Save /storage/.config to /storage/zlyme-backup.tar.gz",
		Zlyme_backup});
	items.push_back(new MenuItem{ListItemType::Button, "Restore backup",
		"Unpack zlyme-backup.tar.gz. Reboot after.",
		Zlyme_restoreBackup});
}

static bool extra_volume_mounted()
{
	FILE *f = fopen("/proc/mounts", "r");
	if (!f)
		return false;
	char line[512];
	bool found = false;
	while (fgets(line, sizeof(line), f)) {
		if (strstr(line, " /mnt/sd2 ") || strstr(line, " /mnt/media/")) {
			found = true;
			break;
		}
	}
	fclose(f);
	return found;
}

static InputReactionHint Zlyme_ejectSd2(AbstractMenuItem &item)
{
	(void)item;
	system("zlyme-storage eject");
	MenuList::showOverlay("Library card ejected", OverlayDismissMode::DismissOnA);
	return NoOp;
}

static std::string pm_label(const std::string &root)
{
	if (root == "/storage")
		return "Main card";
	if (root == "/mnt/sd2")
		return "Second SD";
	std::string::size_type slash = root.rfind('/');
	if (slash != std::string::npos && slash + 1 < root.size())
		return root.substr(slash + 1);
	return root;
}

static int run_two(const char *bin, const char *a, const char *b)
{
	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execl(bin, bin, a, b, (char *)NULL);
		_exit(127);
	}
	int status = 1;
	if (waitpid(pid, &status, 0) < 0)
		return -1;
	return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

struct FormatDev {
	std::string dev;
	std::string label;
};

static void format_status(const char *msg)
{
	FILE *f = fopen("/tmp/zlyme-format.status", "w");
	if (f) {
		fprintf(f, "%s\n", msg);
		fclose(f);
	}
	const char *log = getenv("ZLYME_PAK_LOG");
	if (!log || !log[0])
		return;
	FILE *p = fopen(log, "a");
	if (!p)
		return;
	fprintf(p, "format: %s\n", msg);
	fclose(p);
}

static std::vector<FormatDev> format_devices()
{
	std::vector<FormatDev> out;
	FILE *f = popen("/usr/sbin/zlyme-storage-format list", "r");
	if (!f)
		return out;
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		std::string row = trim(line);
		if (row.empty())
			continue;
		std::string::size_type tab = row.find('\t');
		FormatDev d;
		if (tab == std::string::npos) {
			d.dev = row;
			d.label = row;
		} else {
			d.dev = row.substr(0, tab);
			d.label = row.substr(tab + 1);
			if (d.label.empty())
				d.label = d.dev;
		}
		if (!d.dev.empty())
			out.push_back(d);
	}
	pclose(f);
	return out;
}

static bool format_present(const std::string &dev)
{
	for (const FormatDev &d : format_devices()) {
		if (d.dev == dev)
			return true;
	}
	return false;
}

static int format_exec(const char *dev, const char *fs)
{
	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execl("/usr/sbin/zlyme-storage-format", "zlyme-storage-format",
			"format", dev, fs, "ZLYME-LIB", (char *)NULL);
		_exit(127);
	}
	int status = 1;
	if (waitpid(pid, &status, 0) < 0)
		return -1;
	return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static InputReactionHint format_confirmed(const std::string &dev, const std::string &label,
	const std::string &fsname, const char *fs)
{
	if (!format_present(dev)) {
		format_status("device disappeared");
		MenuList::showOverlay("Device disappeared", OverlayDismissMode::DismissOnA);
		return NoOp;
	}
	std::string msg = label + "\nErase as " + fsname + "?\nALL DATA WILL BE LOST";
	if (!wait_ab_game(msg, "ERASE", "BACK")) {
		format_status("cancelled");
		return NoOp;
	}
	if (!format_present(dev)) {
		format_status("device disappeared");
		MenuList::showOverlay("Device disappeared", OverlayDismissMode::DismissOnA);
		return NoOp;
	}
	format_status("confirmed");
	if (format_exec(dev.c_str(), fs) == 0) {
		format_status("format succeeded");
		MenuList::showOverlay("Format finished", OverlayDismissMode::DismissOnA);
	} else {
		format_status("format failed");
		MenuList::showOverlay("Format failed", OverlayDismissMode::DismissOnA);
	}
	return NoOp;
}

class FormatDeviceMenu : public MenuList
{
public:
	FormatDeviceMenu()
		: MenuList(MenuItemType::Fixed, "Format storage", {})
	{
	}

	void onShow() override
	{
		std::unique_lock<std::shared_mutex> lock(itemLock);
		for (AbstractMenuItem *it : items)
			delete it;
		items.clear();
		for (const FormatDev &d : format_devices()) {
			std::string dev = d.dev;
			std::string label = d.label;
			std::vector<AbstractMenuItem *> fs;
			fs.push_back(new MenuItem{ListItemType::Button, "exFAT", "",
				[dev, label](AbstractMenuItem &) {
					return format_confirmed(dev, label, "exFAT", "exfat");
				}});
			fs.push_back(new MenuItem{ListItemType::Button, "ext4", "",
				[dev, label](AbstractMenuItem &) {
					return format_confirmed(dev, label, "ext4", "ext4");
				}});
			items.push_back(new MenuItem{ListItemType::Button, label, dev,
				DeferToSubmenu, new MenuList(MenuItemType::Fixed, "Filesystem", fs)});
		}
		if (items.empty()) {
			items.push_back(new MenuItem{ListItemType::Button, "No removable storage", "",
				[](AbstractMenuItem &) { return Exit; }});
		}
		scope.selected = 0;
		layout_called = false;
	}
};

void Zlyme_appendStorageItems(std::vector<AbstractMenuItem *> &items)
{
	std::vector<AbstractMenuItem *> storage;

	if ((ctl_on("sd2") || ctl_on("otg")) && extra_volume_mounted()) {
		storage.push_back(new MenuItem{ListItemType::Button, "Eject library card",
			"Unmount the second SD or USB disk before pulling it.\nDo not eject while a game from that card is running.",
			Zlyme_ejectSd2});
	}

	std::vector<std::any> roots;
	std::vector<std::string> labels;
	FILE *list = popen("/usr/sbin/zlyme-portmaster-root list", "r");
	if (list) {
		char line[512];
		while (fgets(line, sizeof(line), list)) {
			std::string root = trim(line);
			if (root.empty())
				continue;
			roots.push_back(root);
			labels.push_back(pm_label(root));
		}
		pclose(list);
	}
	if (!roots.empty()) {
		storage.push_back(new MenuItem{ListItemType::Generic, "PortMaster location",
			"Where new PortMaster games are installed.\nExisting games stay where they are.",
			roots, labels,
			[]() -> std::any {
				FILE *f = popen("/usr/sbin/zlyme-portmaster-root get", "r");
				std::string cur = "/storage";
				if (f) {
					char buf[512] = {0};
					if (fgets(buf, sizeof(buf), f))
						cur = trim(buf);
					pclose(f);
				}
				return cur;
			},
			[](const std::any &v) {
				std::string root = std::any_cast<std::string>(v);
				run_two("/usr/sbin/zlyme-portmaster-root", "set", root.c_str());
			},
			[]() {
				run_two("/usr/sbin/zlyme-portmaster-root", "set", "/storage");
			}});
	}

	storage.push_back(new MenuItem{ListItemType::Button, "Format removable storage",
		"Erase a second SD or USB disk. The main card cannot be selected.",
		DeferToSubmenu, new FormatDeviceMenu()});

	if (storage.empty())
		return;
	items.push_back(new MenuItem{ListItemType::Generic, "Storage",
		"Library cards, PortMaster, and formatting.",
		{}, {}, nullptr, nullptr, DeferToSubmenu,
		new MenuList(MenuItemType::Fixed, "Storage", storage)});
}

static int wait_ab_game(const std::string &msg, const char *aLabel, const char *bLabel)
{
	for (;;) {
		GFX_startFrame();
		PAD_poll();
		if (PAD_justPressed(BTN_A)) {
			MenuList::hideOverlay();
			return 1;
		}
		if (PAD_justPressed(BTN_B)) {
			MenuList::hideOverlay();
			return 0;
		}
		MenuList::showOverlayAB(msg, aLabel, bLabel);
		GFX_sync();
	}
}

void Zlyme_promptRebootOnExit(void)
{
	if (access("/tmp/reboot", F_OK) == 0)
		return;
	int status = system("zlyme-bootcfg dirty /tmp/zlyme-bootcfg");
	if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return;
	if (wait_ab_game("Restart to apply changes?", "RESTART", "LATER"))
		request_session_reboot();
}

static int cleanup_count(const char *action, std::string *extra)
{
	char cmd[384];
	snprintf(cmd, sizeof(cmd),
		"zlyme-game-cleanup %s --dry-run 2>/dev/null", action);
	FILE *f = popen(cmd, "r");
	if (!f)
		return -1;
	int n = 0;
	bool saw = false;
	char line[512];
	std::string rest;
	while (fgets(line, sizeof(line), f)) {
		if (!strncmp(line, "COUNT=", 6)) {
			n = atoi(line + 6);
			saw = true;
		} else
			rest += line;
	}
	int status = pclose(f);
	if (!saw || status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;
	if (extra)
		*extra = rest;
	return n;
}

static bool cleanup_run(const char *action)
{
	char cmd[256];
	snprintf(cmd, sizeof(cmd),
		"zlyme-game-cleanup %s >/dev/null 2>&1", action);
	int status = system(cmd);
	return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static InputReactionHint cleanup_button(const char *action, const char *empty_msg, const char *ask_fmt)
{
	std::string extra;
	int n = cleanup_count(action, &extra);
	if (n < 0) {
		MenuList::showOverlay("Scan failed", OverlayDismissMode::DismissOnA);
		return NoOp;
	}
	if (n == 0) {
		MenuList::showOverlay(empty_msg, OverlayDismissMode::DismissOnA);
		return NoOp;
	}
	char ask[512];
	snprintf(ask, sizeof(ask), ask_fmt, n);
	if (!extra.empty()) {
		strncat(ask, "\n", sizeof(ask) - strlen(ask) - 1);
		strncat(ask, extra.c_str(), sizeof(ask) - strlen(ask) - 1);
	}
	if (!wait_ab_game(ask, "DELETE", "BACK"))
		return NoOp;
	if (!cleanup_run(action)) {
		MenuList::showOverlay("Cleanup failed", OverlayDismissMode::DismissOnA);
		return NoOp;
	}
	MenuList::showOverlay("Done", OverlayDismissMode::DismissOnA);
	return NoOp;
}

void Zlyme_appendGameCleanup(std::vector<AbstractMenuItem *> &items)
{
	items.push_back(new MenuItem{ListItemType::Button, "Clean junk",
		"Remove desktop metadata and trash folders.",
		[](AbstractMenuItem &item) -> InputReactionHint {
			(void)item;
			return cleanup_button("junk", "No junk files", "Delete %d junk files?");
		}});
	items.push_back(new MenuItem{ListItemType::Button, "Orphan saves / states",
		"Remove saves whose ROM is missing from that same card.",
		[](AbstractMenuItem &item) -> InputReactionHint {
			(void)item;
			return cleanup_button("orphan-saves", "No orphan saves", "Delete %d orphan save files?");
		}});
	items.push_back(new MenuItem{ListItemType::Button, "Orphan boxart",
		"Remove artwork whose ROM is missing.",
		[](AbstractMenuItem &item) -> InputReactionHint {
			(void)item;
			return cleanup_button("orphan-media", "No orphan boxart", "Delete %d orphan images?");
		}});
	items.push_back(new MenuItem{ListItemType::Button, "Clear Recents",
		"Clear the Recently Played list.",
		[](AbstractMenuItem &item) -> InputReactionHint {
			(void)item;
			return cleanup_button("recents", "Recents already empty", "Clear %d recent entries?");
		}});
	items.push_back(new MenuItem{ListItemType::Button, "Reset RetroArch core options",
		"Reset per-core RetroArch options.",
		[](AbstractMenuItem &item) -> InputReactionHint {
			(void)item;
			return cleanup_button("ra-cores", "No core options", "Delete %d RetroArch option files?");
		}});
	items.push_back(new MenuItem{ListItemType::Button, "Reset standalone settings",
		"Reset standalone emulator settings.\nGames and saves are kept.",
		[](AbstractMenuItem &item) -> InputReactionHint {
			(void)item;
			return cleanup_button("standalones", "No standalone settings", "Reset %d standalone settings?");
		}});
	items.push_back(new MenuItem{ListItemType::Button, "Orphan per-ROM RetroArch configs",
		"Remove per-ROM RetroArch configs whose ROM is missing.",
		[](AbstractMenuItem &item) -> InputReactionHint {
			(void)item;
			return cleanup_button("ra-rom-cfg", "No orphan RA configs", "Delete %d per-ROM configs?");
		}});
}
