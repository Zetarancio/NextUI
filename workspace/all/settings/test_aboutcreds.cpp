#include "aboutcreds.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static int fails = 0;

static void expect(const std::string &got, const std::string &want, const char *name)
{
	if (got == want)
		return;
	std::cerr << name << ": got [" << got << "] want [" << want << "]\n";
	fails++;
}

static void reject_hash(const std::string &got, const char *name)
{
	if (got.find("$2") != std::string::npos || got.size() == 32) {
		std::cerr << name << " displayed a hash: " << got << "\n";
		fails++;
	}
}

int main()
{
	expect(zlyme_about_ssh_password(true, "secret"), "secret", "ssh set");
	expect(zlyme_about_ssh_password(true, ""), "(empty)", "ssh empty");
	expect(zlyme_about_ssh_password(false, "secret"), "(unavailable)", "ssh fail");

	const char *def =
		"zlyme:980:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX:2FCB1348153EAC6843A3DF162CEFD64F:[U          ]:LCT-00000001\n";
	const char *changed =
		"zlyme:980:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX:8846F7EAEE8FB117AD06BDD830B7586C:[U          ]:LCT-00000001\n";
	expect(zlyme_about_samba_password(def), "zlyme", "samba default");
	expect(zlyme_about_samba_password(changed), "(changed)", "samba changed");
	expect(zlyme_about_samba_password(""), "(unavailable)", "samba missing");
	expect(zlyme_about_samba_password("not-an-account\n"), "(unavailable)", "samba corrupt");
	expect(zlyme_about_samba_password(std::string(def) + changed), "(unavailable)", "samba two lines");
	reject_hash(zlyme_about_samba_password(def), "samba default");
	reject_hash(zlyme_about_samba_password(changed), "samba changed");

	const char *hash = "$2a$10$DEz.z2B2OovL1pa.dQl4bOTttxkPtRpq3timKQXpfpLDdcwxLCFyW";
	expect(zlyme_about_syncthing_account_label("zlyme"), "zlyme", "syncthing user");
	expect(zlyme_about_syncthing_password_label("(set)"), "(set)", "syncthing hash");
	expect(zlyme_about_syncthing_password_label(hash), "(unavailable)", "syncthing hash display");
	reject_hash(zlyme_about_syncthing_password_label("(set)"), "syncthing hash display");
	expect(zlyme_about_syncthing_password_label("zlyme"), "(unavailable)", "syncthing plaintext");
	expect(zlyme_about_syncthing_account_label(""), "(unavailable)", "syncthing missing");
	expect(zlyme_about_syncthing_account_label(hash), "(unavailable)", "syncthing user hash");
	reject_hash(zlyme_about_syncthing_account_label("zlyme"), "syncthing user display");
	expect(zlyme_about_syncthing_account_label("alice"), "alice", "syncthing renamed");
	expect(zlyme_about_syncthing_account_label("<!-- <user>zlyme</user> -->"), "(unavailable)",
		"syncthing comment text");

	char dir[] = "/tmp/zlyme-about-XXXXXX";
	if (!mkdtemp(dir)) {
		std::cerr << "mkdtemp failed\n";
		return 1;
	}
	std::string helper = std::string(dir) + "/helper";
	{
		std::ofstream out(helper);
		out << "#!/bin/sh\nprintf '%s\\n' \"zlyme\" \"(set)\"\n";
	}
	if (chmod(helper.c_str(), 0755) != 0) {
		std::cerr << "chmod helper failed\n";
		return 1;
	}
	std::string account, password;
	zlyme_about_syncthing_read("/tmp/unused-config.xml", helper, account, password);
	expect(account, "zlyme", "helper account");
	expect(password, "(set)", "helper password");
	reject_hash(password, "helper password");

	std::string leak = std::string(dir) + "/leak";
	{
		std::ofstream out(leak);
		out << "#!/bin/sh\nprintf '%s\\n' '" << hash << "' '" << hash << "'\n";
	}
	chmod(leak.c_str(), 0755);
	zlyme_about_syncthing_read("/tmp/unused-config.xml", leak, account, password);
	expect(account, "(unavailable)", "helper hash account");
	expect(password, "(unavailable)", "helper hash password");
	reject_hash(account, "helper hash account display");
	reject_hash(password, "helper hash password display");

	const char *gui = std::getenv("ZLYME_SYNCTHING_GUI");
	if (gui && *gui) {
		std::string xml = std::string(dir) + "/config.xml";
		{
			std::ofstream out(xml);
			out << "<configuration>\n"
			    << "  <!--\n"
			    << "  <gui><user>zlyme</user><password>" << hash << "</password></gui>\n"
			    << "  -->\n"
			    << "  <gui enabled=\"true\"><address>127.0.0.1:8384</address></gui>\n"
			    << "</configuration>\n";
		}
		zlyme_about_syncthing_read(xml, gui, account, password);
		expect(account, "(unavailable)", "commented xml account");
		expect(password, "(unavailable)", "commented xml password");
		reject_hash(account, "commented xml account");
		reject_hash(password, "commented xml password");

		std::string real = std::string(dir) + "/real.xml";
		{
			std::ofstream out(real);
			out << "<configuration><gui><user>alice</user><password>" << hash
			    << "</password></gui></configuration>\n";
		}
		zlyme_about_syncthing_read(real, gui, account, password);
		expect(account, "alice", "parsed xml account");
		expect(password, "(set)", "parsed xml password");
		reject_hash(password, "parsed xml password");
	}
	unlink(helper.c_str());
	unlink(leak.c_str());
	if (gui && *gui) {
		unlink((std::string(dir) + "/config.xml").c_str());
		unlink((std::string(dir) + "/real.xml").c_str());
	}
	rmdir(dir);

	if (fails) {
		std::cerr << fails << " about credential checks failed\n";
		return 1;
	}
	std::cout << "about creds ok\n";
	return 0;
}
