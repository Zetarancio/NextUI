#include "zlymemenu.hpp"
#include "keyboardprompt.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <mutex>
#include <sys/wait.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
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

struct ProxyState {
	bool enabled = false;
	std::string protocol = "http";
	std::string host;
	std::string port;
};

static int proxy_run(const std::vector<std::string> &args, std::string *out_text, std::string *err_text)
{
	int outp[2] = {-1, -1};
	int errp[2] = {-1, -1};
	if (pipe(outp) != 0)
		return -1;
	if (err_text && pipe(errp) != 0) {
		close(outp[0]);
		close(outp[1]);
		return -1;
	}
	pid_t pid = fork();
	if (pid < 0) {
		close(outp[0]);
		close(outp[1]);
		if (err_text) {
			close(errp[0]);
			close(errp[1]);
		}
		return -1;
	}
	if (pid == 0) {
		dup2(outp[1], STDOUT_FILENO);
		if (err_text)
			dup2(errp[1], STDERR_FILENO);
		else
			dup2(outp[1], STDERR_FILENO);
		close(outp[0]);
		close(outp[1]);
		if (err_text) {
			close(errp[0]);
			close(errp[1]);
		}
		std::vector<char *> argv;
		argv.push_back(const_cast<char *>("zlyme-proxy"));
		for (const auto &arg : args)
			argv.push_back(const_cast<char *>(arg.c_str()));
		argv.push_back(nullptr);
		execvp("zlyme-proxy", argv.data());
		_exit(127);
	}
	close(outp[1]);
	if (err_text)
		close(errp[1]);
	auto read_fd = [](int fd) {
		std::string text;
		char buf[256];
		ssize_t n;
		while ((n = ::read(fd, buf, sizeof(buf))) > 0)
			text.append(buf, static_cast<size_t>(n));
		close(fd);
		return text;
	};
	if (out_text)
		*out_text = read_fd(outp[0]);
	else
		close(outp[0]);
	if (err_text)
		*err_text = read_fd(errp[0]);
	int status = 0;
	if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status))
		return -1;
	return WEXITSTATUS(status);
}

static ProxyState proxy_load()
{
	ProxyState state;
	std::string out;
	if (proxy_run({"get"}, &out, nullptr) != 0)
		return state;
	std::istringstream in(out);
	std::string line;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		std::string key = line.substr(0, eq);
		std::string value = line.substr(eq + 1);
		if (key == "enabled")
			state.enabled = (value == "1");
		else if (key == "protocol")
			state.protocol = (value == "socks5") ? "socks5" : "http";
		else if (key == "host")
			state.host = value;
		else if (key == "port")
			state.port = value;
	}
	return state;
}

static bool proxy_store(const ProxyState &state, std::string *err)
{
	std::string message;
	std::vector<std::string> args;
	args.push_back("set");
	args.push_back(state.enabled ? "1" : "0");
	args.push_back(state.protocol);
	args.push_back(state.host);
	args.push_back(state.port);
	int rc = proxy_run(args, nullptr, &message);
	while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
		message.pop_back();
	if (err)
		*err = message;
	return rc == 0;
}

static void Zlyme_appendProxyItem(std::vector<AbstractMenuItem *> &items)
{
	const std::vector<std::any> on_off_v = {false, true};
	const std::vector<std::string> on_off = {"Off", "On"};
	const std::vector<std::any> proto_v = {std::string("http"), std::string("socks5")};
	const std::vector<std::string> proto_l = {"HTTP", "SOCKS5"};

	auto *host_prompt = new KeyboardPrompt("Proxy host", [](AbstractMenuItem &item) -> InputReactionHint {
		ProxyState state = proxy_load();
		state.host = item.getName();
		std::string err;
		if (!proxy_store(state, &err)) {
			MenuList::showOverlay(err.empty() ? "Could not save host." : err,
				OverlayDismissMode::DismissOnA);
			return NoOp;
		}
		return Exit;
	});
	auto *port_prompt = new KeyboardPrompt("Proxy port", [](AbstractMenuItem &item) -> InputReactionHint {
		ProxyState state = proxy_load();
		state.port = item.getName();
		std::string err;
		if (!proxy_store(state, &err)) {
			MenuList::showOverlay(err.empty() ? "Could not save port." : err,
				OverlayDismissMode::DismissOnA);
			return NoOp;
		}
		return Exit;
	});

	std::vector<AbstractMenuItem *> proxy_items = {
		new MenuItem{ListItemType::Generic, "Proxy",
			"Off leaves applications direct.",
			on_off_v, on_off,
			[]() -> std::any { return proxy_load().enabled; },
			[](const std::any &value) {
				ProxyState state = proxy_load();
				state.enabled = std::any_cast<bool>(value);
				std::string err;
				if (!proxy_store(state, &err))
					MenuList::showOverlay(err.empty() ? "Could not save proxy." : err,
						OverlayDismissMode::DismissOnA);
			},
			[]() {
				ProxyState state = proxy_load();
				state.enabled = false;
				proxy_store(state, nullptr);
			}},
		new MenuItem{ListItemType::Generic, "Protocol",
			"SOCKS5 resolves names through the proxy.",
			proto_v, proto_l,
			[]() -> std::any { return proxy_load().protocol; },
			[](const std::any &value) {
				ProxyState state = proxy_load();
				state.protocol = std::any_cast<std::string>(value);
				std::string err;
				if (!proxy_store(state, &err))
					MenuList::showOverlay(err.empty() ? "Could not save protocol." : err,
						OverlayDismissMode::DismissOnA);
			},
			[]() {
				ProxyState state = proxy_load();
				state.protocol = "http";
				proxy_store(state, nullptr);
			}},
		new TextInputMenuItem{"Host", "Hostname or address. No username.",
			[]() -> std::any {
				std::string host = proxy_load().host;
				return host.empty() ? std::string("(not set)") : host;
			},
			[host_prompt](AbstractMenuItem &item) -> InputReactionHint {
				host_prompt->setInitialText(proxy_load().host);
				item.defer(true);
				return NoOp;
			}, host_prompt},
		new TextInputMenuItem{"Port", "1-65535.",
			[]() -> std::any {
				std::string port = proxy_load().port;
				return port.empty() ? std::string("(not set)") : port;
			},
			[port_prompt](AbstractMenuItem &item) -> InputReactionHint {
				port_prompt->setInitialText(proxy_load().port);
				item.defer(true);
				return NoOp;
			}, port_prompt},
		new MenuItem{ListItemType::Button, "Test proxy",
			"A short GitHub HTTPS request.",
			[](AbstractMenuItem &) -> InputReactionHint {
				std::string out;
				std::string err;
				proxy_run({"test"}, &out, &err);
				std::string msg = trim(out.empty() ? err : out);
				if (msg.empty())
					msg = "Proxy test failed.";
				MenuList::showOverlay(msg, OverlayDismissMode::DismissOnA);
				return NoOp;
			}},
	};
	items.push_back(new MenuItem{ListItemType::Generic, "Proxy",
		"HTTP or SOCKS5 for applications. Not a VPN.",
		{}, {}, nullptr, nullptr, DeferToSubmenu,
		new MenuList(MenuItemType::Fixed, "Proxy", std::move(proxy_items))});
}

void Zlyme_appendNetworkItems(std::vector<AbstractMenuItem *> &items)
{
	const std::vector<std::any> on_off_v = {false, true};
	const std::vector<std::string> on_off = {"Off", "On"};

	Zlyme_appendProxyItem(items);
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

static std::string run_argv(char *const argv[], int *code)
{
	int fds[2];
	if (pipe(fds) != 0) {
		*code = -1;
		return "could not start the command";
	}
	pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		*code = -1;
		return "could not start the command";
	}
	if (pid == 0) {
		close(fds[0]);
		if (dup2(fds[1], 1) < 0 || dup2(fds[1], 2) < 0)
			_exit(127);
		if (fds[1] > 2)
			close(fds[1]);
		execv(argv[0], argv);
		_exit(127);
	}
	close(fds[1]);
	std::string out;
	char buf[256];
	ssize_t n;
	while ((n = read(fds[0], buf, sizeof(buf))) > 0) {
		if (out.size() < 1200)
			out.append(buf, buf + n);
	}
	close(fds[0]);
	int status = 0;
	if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status))
		*code = -1;
	else
		*code = WEXITSTATUS(status);
	return trim(out);
}

static InputReactionHint recovery_cancel(AbstractMenuItem &)
{
	return Exit;
}

static std::string kv_last(const std::string &text, const char *key)
{
	const std::string prefix = std::string(key) + "=";
	std::istringstream in(text);
	std::string line;
	std::string val;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.compare(0, prefix.size(), prefix) == 0)
			val = line.substr(prefix.size());
	}
	return val;
}

static MenuList *preloader_status_page(const std::string &text, int code)
{
	std::vector<AbstractMenuItem *> rows;
	std::string error = kv_last(text, "error");
	// An empty value string makes SDL_ttf return NULL and the page exits.
	// A failed or empty backend still opens a normal fixed page.
	if (code != 0 || !error.empty() || text.empty()) {
		rows.push_back(new StaticMenuItem{ListItemType::Generic, "Preloader status",
			"The recovery backend did not return a status.",
			[]() -> std::any { return std::string("Could not be read."); }});
	} else {
		std::string mode = kv_last(text, "mode");
		std::string rec = kv_last(text, "recovery");
		std::string src = kv_last(text, "source_backup");
		std::string stock = kv_last(text, "stock_restore");
		std::string bat = kv_last(text, "battery");
		std::string chg = kv_last(text, "charger");
		std::string bat_l = bat.empty() ? "Unknown" : bat + "%";
		if (chg == "charging")
			bat_l += " / charging";
		std::string pre_l = "Unknown";
		if (mode == "normal")
			pre_l = "Normal";
		else if (mode == "recovery")
			pre_l = "Recovery armed";
		std::string rec_l = "Unknown";
		if (rec == "ready")
			rec_l = "Ready";
		else if (rec == "armed")
			rec_l = "Armed";
		else if (rec == "not-prepared")
			rec_l = "Not prepared";
		else if (rec == "invalid")
			rec_l = "Invalid";
		std::string src_l = src == "available" ? "Available" : "Missing";
		std::string stock_l = stock == "available" ? "Available" : "Unavailable";
		rows.push_back(new StaticMenuItem{ListItemType::Generic, "Preloader",
			"Internal boot image.",
			[pre_l]() -> std::any { return pre_l; }});
		rows.push_back(new StaticMenuItem{ListItemType::Generic, "Recovery",
			"MASKROM recovery state.",
			[rec_l]() -> std::any { return rec_l; }});
		rows.push_back(new StaticMenuItem{ListItemType::Generic, "Source backup",
			"Saved pre-recovery image.",
			[src_l]() -> std::any { return src_l; }});
		rows.push_back(new StaticMenuItem{ListItemType::Generic, "Stock restore",
			"Original Miyoo preloader.",
			[stock_l]() -> std::any { return stock_l; }});
		rows.push_back(new StaticMenuItem{ListItemType::Generic, "Battery",
			"From zlyme-preloader.",
			[bat_l]() -> std::any { return bat_l; }});
	}
	return new MenuList(MenuItemType::Fixed, "Preloader status", rows);
}

static InputReactionHint recovery_status(AbstractMenuItem &item)
{
	char arg0[] = "/usr/sbin/zlyme-preloader";
	char arg1[] = "status-machine";
	char *argv[] = {arg0, arg1, nullptr};
	int code = 0;
	std::string text = run_argv(argv, &code);
	item.setSubMenu(preloader_status_page(text, code));
	item.defer(true);
	return NoOp;
}

static InputReactionHint recovery_restore_now(AbstractMenuItem &)
{
	char arg0[] = "/usr/sbin/zlyme-preloader";
	char arg1[] = "restore";
	char *argv[] = {arg0, arg1, nullptr};
	int code = 0;
	std::string text = run_argv(argv, &code);
	std::string msg;
	if (code == 0)
		msg = "Restore finished. The readback hash matched the backup.";
	else if (text.find("previous preloader restored and verified") != std::string::npos)
		msg = "Restore failed; previous preloader restored and verified.";
	else if (text.find("CRITICAL:") != std::string::npos)
		msg = "CRITICAL: restore failed and rollback could not be verified.";
	else if (!text.empty())
		msg = text;
	else
		msg = "Restore failed.";
	MenuList::showOverlay(msg, OverlayDismissMode::DismissOnA);
	return NoOp;
}

static std::string recovery_brief(const std::string &text, const char *fallback)
{
	std::string line = text;
	size_t nl = line.find('\n');
	if (nl != std::string::npos)
		line.resize(nl);
	if (line.size() > 180)
		line.resize(180);
	return line.empty() ? fallback : line;
}

static InputReactionHint recovery_arm_now(AbstractMenuItem &)
{
	char arg0[] = "/usr/sbin/zlyme-preloader";
	char prep[] = "prepare-recovery";
	char *pargv[] = {arg0, prep, nullptr};
	int code = 0;
	std::string text = run_argv(pargv, &code);
	if (code != 0) {
		MenuList::showOverlay(
			recovery_brief(text, "Prepare failed. Recovery was not armed."),
			OverlayDismissMode::DismissOnA);
		return NoOp;
	}
	char arm[] = "arm-recovery";
	char *aargv[] = {arg0, arm, nullptr};
	text = run_argv(aargv, &code);
	std::string msg;
	if (code == 0)
		msg = "Recovery armed. Keep a bootable card in the right slot for normal boot. To enter MASKROM, shut down, remove the right card, then power on.";
	else if (text.find("source preloader restored and verified") != std::string::npos)
		msg = "Arm failed. The previous preloader was restored and verified.";
	else if (text.find("CRITICAL:") != std::string::npos)
		msg = "CRITICAL: arm failed and rollback could not be verified.";
	else
		msg = recovery_brief(text, "Arm failed.");
	MenuList::showOverlay(msg, OverlayDismissMode::DismissOnA);
	return NoOp;
}

static InputReactionHint recovery_disarm_now(AbstractMenuItem &)
{
	char arg0[] = "/usr/sbin/zlyme-preloader";
	char cmd[] = "disarm-recovery";
	char *argv[] = {arg0, cmd, nullptr};
	int code = 0;
	std::string text = run_argv(argv, &code);
	std::string msg;
	if (code == 0)
		msg = "Normal preloader restored and verified.";
	else if (text.find("recovery preloader restored and verified") != std::string::npos)
		msg = "Disarm failed. The recovery preloader was restored and verified.";
	else if (text.find("CRITICAL:") != std::string::npos)
		msg = "CRITICAL: disarm failed and rollback could not be verified.";
	else if (text.find("not the recovery image") != std::string::npos
		|| text.find("recovery binding refused") != std::string::npos
		|| text.find("recovery manifest is missing") != std::string::npos)
		msg = "Disarm refused. This preloader is not an armed recovery image.";
	else
		msg = recovery_brief(text, "Disarm refused.");
	MenuList::showOverlay(msg, OverlayDismissMode::DismissOnA);
	return NoOp;
}

static MenuList *recovery_final(const char *title, const char *action,
	MenuListCallback go)
{
	std::vector<AbstractMenuItem *> rows;
	rows.push_back(new MenuItem{ListItemType::Button, "Cancel",
		"Leave this confirmation.", recovery_cancel});
	rows.push_back(new MenuItem{ListItemType::Button, action,
		"Final confirmation. Cancel is selected.", go});
	return new MenuList(MenuItemType::Fixed, title, rows);
}

static MenuList *recovery_stage(const char *title, const std::string &desc,
	MenuList *final_page)
{
	std::vector<AbstractMenuItem *> rows;
	rows.push_back(new MenuItem{ListItemType::Button, "Cancel",
		"Leave this confirmation.", recovery_cancel});
	rows.push_back(new MenuItem{ListItemType::Button, "Continue", desc,
		DeferToSubmenu, final_page});
	return new MenuList(MenuItemType::Fixed, title, rows);
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
	const char *restore_desc =
		"Restores the original Miyoo preloader.\n"
		"This is not how you leave MASKROM recovery.";
	const char *arm_desc =
		"A bootable card in the right slot still boots.\n"
		"Without one, startup enters MASKROM.";
	const char *arm_warn =
		"This installs a recovery preloader in internal NAND.\n"
		"A bootable card in the RIGHT slot still boots normally.\n"
		"Without a bootable right-slot card, startup enters Rockchip MASKROM recovery.\n"
		"Internal stock boot stays disabled until recovery is disarmed.";
	const char *disarm_desc =
		"Restores the exact pre-recovery image.\n"
		"This is not a stock restore.";
	const char *disarm_warn =
		"Writes that saved image back to NAND.\n"
		"Only when it is the source of the live recovery image.\n"
		"This does not restore the original Miyoo preloader.";
	std::vector<AbstractMenuItem *> recovery;
	recovery.push_back(new MenuItem{ListItemType::Button, "Preloader status",
		"Internal boot and recovery state.",
		recovery_status});
	recovery.push_back(new MenuItem{ListItemType::Button, "Arm MASKROM recovery",
		arm_desc, DeferToSubmenu,
		recovery_stage("Arm MASKROM recovery", arm_warn,
			recovery_final("Arm MASKROM recovery",
				"ARM MASKROM RECOVERY", recovery_arm_now))});
	recovery.push_back(new MenuItem{ListItemType::Button, "Disarm MASKROM recovery",
		disarm_desc, DeferToSubmenu,
		recovery_stage("Disarm MASKROM recovery", disarm_warn,
			recovery_final("Disarm MASKROM recovery",
				"DISARM MASKROM RECOVERY", recovery_disarm_now))});
	recovery.push_back(new MenuItem{ListItemType::Button, "Restore stock preloader",
		restore_desc, DeferToSubmenu,
		recovery_stage("Restore stock preloader", restore_desc,
			recovery_final("Restore stock preloader",
				"RESTORE STOCK PRELOADER", recovery_restore_now))});
	advanced.push_back(new MenuItem{ListItemType::Generic, "Recovery",
		"Preloader status, MASKROM recovery, and stock restore.",
		{}, {}, nullptr, nullptr, DeferToSubmenu,
		new MenuList(MenuItemType::Fixed, "Recovery", recovery)});
	advanced.push_back(new MenuItem{ListItemType::Button, "Reset Settings",
		"Return product settings to defaults.\nGames, saves, Wi-Fi and paired devices stay.",
		Zlyme_resetSettings});
	advanced.push_back(new MenuItem{ListItemType::Button, "Factory Reset",
		"Restore settings and stock Tools/Emus.\nGames, saves, Wi-Fi and personal content stay.",
		Zlyme_factoryReset});
	items.push_back(new MenuItem{ListItemType::Generic, "Advanced",
		"GPU, power features, logs, reset, and recovery.",
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

	Zlyme_appendGameFolders(storage);

	storage.push_back(new MenuItem{ListItemType::Button, "Format removable storage",
		"Erase a second SD or USB disk. The main card cannot be selected.",
		DeferToSubmenu, new FormatDeviceMenu()});

	if (storage.empty())
		return;
	items.push_back(new MenuItem{ListItemType::Generic, "Storage",
		"Library cards, game folders, PortMaster, and formatting.",
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

static std::string library_label(const std::string &root)
{
	if (root == "/storage")
		return "SD card 1";
	if (root == "/mnt/sd2")
		return "SD card 2";
	const std::string media = "/mnt/media/";
	if (root.compare(0, media.size(), media) == 0) {
		std::string name = root.substr(media.size());
		if (name.empty())
			return "USB";
		return "USB: " + name;
	}
	return root;
}

static std::vector<std::string> active_libraries()
{
	const char *path = getenv("ZLYME_LIBRARIES_FILE");
	if (!path || !path[0])
		path = "/run/zlyme/libraries";
	std::vector<std::string> out;
	std::ifstream in(path);
	std::string line;
	while (std::getline(in, line)) {
		line = trim(line);
		if (!line.empty())
			out.push_back(line);
	}
	return out;
}

static int populate_exec(const std::string &root)
{
	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execl("/usr/sbin/zlyme-library-populate", "zlyme-library-populate",
			root.c_str(), (char *)NULL);
		_exit(127);
	}
	int status = 1;
	if (waitpid(pid, &status, 0) < 0)
		return -1;
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		return 0;
	return -1;
}

class GameFolderMenu : public MenuList
{
public:
	GameFolderMenu()
		: MenuList(MenuItemType::Fixed, "Create game folders", {})
	{
	}

	void onShow() override
	{
		std::unique_lock<std::shared_mutex> lock(itemLock);
		for (AbstractMenuItem *it : items)
			delete it;
		items.clear();
		for (const std::string &root : active_libraries()) {
			std::string label = library_label(root);
			items.push_back(new MenuItem{ListItemType::Button, label, root,
				[root, label](AbstractMenuItem &) {
					if (populate_exec(root) == 0)
						MenuList::showOverlay("Game folders created",
							OverlayDismissMode::DismissOnA);
					else
						MenuList::showOverlay("Library unavailable",
							OverlayDismissMode::DismissOnA);
					return NoOp;
				}});
		}
		if (items.empty()) {
			items.push_back(new MenuItem{ListItemType::Button, "No library", "",
				[](AbstractMenuItem &) {
					MenuList::showOverlay("Library unavailable",
						OverlayDismissMode::DismissOnA);
					return Exit;
				}});
		}
		scope.selected = 0;
		layout_called = false;
	}
};

void Zlyme_appendGameFolders(std::vector<AbstractMenuItem *> &items)
{
	items.push_back(new MenuItem{ListItemType::Button, "Create game folders",
		"Create missing ROM, BIOS and save folders on a library.",
		DeferToSubmenu, new GameFolderMenu()});
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
