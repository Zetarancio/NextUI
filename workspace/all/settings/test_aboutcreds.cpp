#include "aboutcreds.hpp"

#include <iostream>
#include <string>

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

	const char *xml =
		"<gui enabled=\"true\" tls=\"false\">\n"
		"    <user>zlyme</user>\n"
		"    <password>$2a$10$DEz.z2B2OovL1pa.dQl4bOTttxkPtRpq3timKQXpfpLDdcwxLCFyW</password>\n"
		"</gui>\n";
	expect(zlyme_about_syncthing_account(xml), "zlyme", "syncthing user");
	expect(zlyme_about_syncthing_password(xml), "(set)", "syncthing hash");
	reject_hash(zlyme_about_syncthing_password(xml), "syncthing hash display");

	const char *plain =
		"<gui><user>zlyme</user><password>zlyme</password></gui>";
	expect(zlyme_about_syncthing_password(plain), "(unavailable)", "syncthing plaintext");
	expect(zlyme_about_syncthing_account(""), "(unavailable)", "syncthing missing");

	const char *hash_user =
		"<gui><user>$2a$10$DEz.z2B2OovL1pa.dQl4bOTttxkPtRpq3timKQXpfpLDdcwxLCFyW</user>"
		"<password>$2a$10$DEz.z2B2OovL1pa.dQl4bOTttxkPtRpq3timKQXpfpLDdcwxLCFyW</password></gui>";
	expect(zlyme_about_syncthing_account(hash_user), "(unavailable)", "syncthing user hash");
	reject_hash(zlyme_about_syncthing_account(hash_user), "syncthing user display");

	const char *renamed =
		"<gui><user>alice</user>"
		"<password>$2b$10$DEz.z2B2OovL1pa.dQl4bOTttxkPtRpq3timKQXpfpLDdcwxLCFyW</password></gui>";
	expect(zlyme_about_syncthing_account(renamed), "alice", "syncthing renamed");
	expect(zlyme_about_syncthing_password(renamed), "(set)", "syncthing renamed hash");

	if (fails) {
		std::cerr << fails << " about credential checks failed\n";
		return 1;
	}
	std::cout << "about creds ok\n";
	return 0;
}
