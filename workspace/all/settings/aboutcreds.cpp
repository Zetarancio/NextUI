#include "aboutcreds.hpp"

#include <cerrno>
#include <cctype>
#include <fcntl.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

bool hex32(const std::string &s)
{
	if (s.size() != 32)
		return false;
	for (char c : s) {
		if (!std::isxdigit(static_cast<unsigned char>(c)))
			return false;
	}
	return true;
}

std::string upper_hex(std::string s)
{
	for (char &c : s)
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	return s;
}

// MD4(UTF-16LE("zlyme")). NT hashes are not salted, so this identifies
// the published initial password. It is not a stored copy of a password
// the user chose later.
const char kSambaDefaultNt[] = "2FCB1348153EAC6843A3DF162CEFD64F";

std::string nth_line(const std::string &text, int which)
{
	size_t start = 0;
	int seen = 0;
	while (seen < which && start <= text.size()) {
		size_t nl = text.find('\n', start);
		if (nl == std::string::npos)
			return {};
		start = nl + 1;
		seen++;
	}
	if (start > text.size())
		return {};
	size_t nl = text.find('\n', start);
	if (nl == std::string::npos)
		return text.substr(start);
	return text.substr(start, nl - start);
}

} // namespace

std::string zlyme_about_ssh_password(bool ok, const std::string &text)
{
	if (!ok)
		return "(unavailable)";
	if (text.empty())
		return "(empty)";
	return text;
}

std::string zlyme_about_samba_password(const std::string &account_file)
{
	int good = 0;
	bool bad = false;
	std::string nt;
	size_t i = 0;
	while (i < account_file.size()) {
		size_t eol = account_file.find('\n', i);
		if (eol == std::string::npos)
			eol = account_file.size();
		std::string line = account_file.substr(i, eol - i);
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		i = eol + 1;
		size_t sp = 0;
		while (sp < line.size() && std::isspace(static_cast<unsigned char>(line[sp])))
			sp++;
		if (sp == line.size() || line[sp] == '#')
			continue;
		std::string fields[4];
		int n = 0;
		size_t col = 0;
		while (n < 4) {
			size_t next = line.find(':', col);
			if (next == std::string::npos) {
				fields[n++] = line.substr(col);
				break;
			}
			fields[n++] = line.substr(col, next - col);
			col = next + 1;
		}
		if (n < 4 || fields[0] != "zlyme" || !hex32(fields[3])) {
			bad = true;
			continue;
		}
		good++;
		nt = upper_hex(fields[3]);
	}
	if (bad || good != 1)
		return "(unavailable)";
	if (nt == kSambaDefaultNt)
		return "zlyme";
	return "(changed)";
}

std::string zlyme_about_syncthing_account_label(const std::string &line)
{
	if (line.empty() || line == "(unavailable)")
		return "(unavailable)";
	if (line.find('\n') != std::string::npos || line.find('\r') != std::string::npos)
		return "(unavailable)";
	if (line.find('<') != std::string::npos || line.find('>') != std::string::npos)
		return "(unavailable)";
	if (line.find("$2a$") != std::string::npos || line.find("$2b$") != std::string::npos ||
	    line.find("$2y$") != std::string::npos)
		return "(unavailable)";
	if (line.size() > 128)
		return "(unavailable)";
	return line;
}

std::string zlyme_about_syncthing_password_label(const std::string &line)
{
	if (line == "(set)")
		return "(set)";
	return "(unavailable)";
}

void zlyme_about_syncthing_read(const std::string &config_path, const std::string &helper,
	std::string &account, std::string &password)
{
	account = "(unavailable)";
	password = "(unavailable)";
	if (helper.empty() || config_path.empty())
		return;
	int fds[2];
	if (pipe(fds) != 0)
		return;
	pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		return;
	}
	if (pid == 0) {
		if (dup2(fds[1], STDOUT_FILENO) < 0)
			_exit(127);
		close(fds[0]);
		close(fds[1]);
		int devnull = open("/dev/null", O_WRONLY);
		if (devnull >= 0) {
			if (dup2(devnull, STDERR_FILENO) < 0)
				_exit(127);
			close(devnull);
		}
		execl(helper.c_str(), helper.c_str(), "about", config_path.c_str(),
			static_cast<char *>(nullptr));
		_exit(127);
	}
	close(fds[1]);
	std::string out;
	char buf[256];
	for (;;) {
		ssize_t n = read(fds[0], buf, sizeof buf);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (n == 0)
			break;
		out.append(buf, static_cast<size_t>(n));
	}
	close(fds[0]);
	int status = 0;
	for (;;) {
		pid_t got = waitpid(pid, &status, 0);
		if (got < 0) {
			if (errno == EINTR)
				continue;
			return;
		}
		break;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return;
	account = zlyme_about_syncthing_account_label(nth_line(out, 0));
	password = zlyme_about_syncthing_password_label(nth_line(out, 1));
}
