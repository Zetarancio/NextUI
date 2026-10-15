#include "aboutcreds.hpp"

#include <cctype>
#include <string>

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

std::string gui_span(const std::string &xml)
{
	const std::string open = "<gui";
	const std::string close = "</gui>";
	size_t start = xml.find(open);
	if (start == std::string::npos)
		return {};
	size_t end = xml.find(close, start);
	if (end == std::string::npos)
		return {};
	return xml.substr(start, end - start);
}

std::string tag_text(const std::string &span, const char *tag)
{
	const std::string open = std::string("<") + tag + ">";
	const std::string close = std::string("</") + tag + ">";
	size_t start = span.find(open);
	if (start == std::string::npos)
		return {};
	start += open.size();
	size_t end = span.find(close, start);
	if (end == std::string::npos)
		return {};
	return span.substr(start, end - start);
}

bool is_bcrypt(const std::string &s)
{
	if (s.size() < 4 + 1 + 1 + 50)
		return false;
	if (s.compare(0, 4, "$2a$") != 0 && s.compare(0, 4, "$2b$") != 0 &&
	    s.compare(0, 4, "$2y$") != 0)
		return false;
	size_t i = 4;
	if (!std::isdigit(static_cast<unsigned char>(s[i])))
		return false;
	while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])))
		i++;
	if (i >= s.size() || s[i] != '$')
		return false;
	return s.size() - (i + 1) >= 50;
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

std::string zlyme_about_syncthing_account(const std::string &config_xml)
{
	std::string user = tag_text(gui_span(config_xml), "user");
	if (user.empty() || is_bcrypt(user))
		return "(unavailable)";
	if (user.find('\n') != std::string::npos || user.find('\r') != std::string::npos)
		return "(unavailable)";
	return user;
}

std::string zlyme_about_syncthing_password(const std::string &config_xml)
{
	std::string pass = tag_text(gui_span(config_xml), "password");
	// A salted bcrypt value cannot be matched to "zlyme" by comparing
	// the text with a stored hash. Syncthing is not started from About.
	// A present hash is reported as set. Anything else is not shown.
	if (is_bcrypt(pass))
		return "(set)";
	return "(unavailable)";
}
