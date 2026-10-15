#include "aboutcreds.hpp"
#include "sambasettings.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static int fails = 0;

static void expect(bool ok, const char *name)
{
	if (ok)
		return;
	std::cerr << name << " failed\n";
	fails++;
}

static void expect_eq(const std::string &got, const std::string &want, const char *name)
{
	if (got == want)
		return;
	std::cerr << name << ": got [" << got << "] want [" << want << "]\n";
	fails++;
}

static std::string slurp(const std::string &path)
{
	std::ifstream in(path);
	std::string text;
	std::string line;
	while (std::getline(in, line)) {
		text += line;
		text += "\n";
	}
	return text;
}

int main()
{
	expect_eq(zlyme_samba_password_label(false), "Set", "label set");
	expect_eq(zlyme_samba_password_label(true), "Change", "label change");

	std::vector<std::string> args{"leftover"};
	std::string stdin_text = "leftover";
	std::string error;
	expect(!zlyme_samba_password_command("", &args, &stdin_text, &error), "empty rejected");
	expect(args.size() == 1 && args[0] == "leftover", "empty left the command alone");
	expect_eq(stdin_text, "leftover", "empty left stdin alone");
	expect_eq(error, "Enter a Samba password.", "empty message");

	const std::string password = "s3cret-token";
	expect(zlyme_samba_password_command(password, &args, &stdin_text, &error), "set accepted");
	expect(args.size() == 1 && args[0] == "set", "args are only set");
	expect_eq(stdin_text, password, "password is stdin");
	expect(error.empty(), "set has no error");

	expect_eq(zlyme_samba_service_message(true, true, "Starting smbd: OK\n"), "", "start success is quiet");
	expect_eq(zlyme_samba_service_message(true, false, "Samba: authentication failed\n"),
		"Samba: authentication failed", "start failure text");
	expect_eq(zlyme_samba_service_message(true, false, ""), "Samba did not start.", "start failure fallback");
	expect_eq(zlyme_samba_service_message(false, false, ""), "Samba did not stop.", "stop failure fallback");
	expect(!zlyme_samba_flag_after(true, false), "failed start is off");
	expect(zlyme_samba_flag_after(true, true), "started stays on");
	expect(!zlyme_samba_flag_after(false, true), "stop is off");
	expect(!zlyme_samba_flag_after(false, false), "failed stop stays off");

	char dir[] = "/tmp/zlyme-samba-ui-XXXXXX";
	if (!mkdtemp(dir)) {
		std::cerr << "mkdtemp failed\n";
		return 1;
	}
	std::string auth = std::string(dir) + "/auth";
	std::string account = std::string(dir) + "/samba.account";
	const char *def =
		"zlyme:980:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX:2FCB1348153EAC6843A3DF162CEFD64F:[U          ]:LCT-00000001\n";
	{
		std::ofstream out(auth);
		out << "#!/bin/sh\n"
		    << "dir=${ZLYME_TEST_DIR:?}\n"
		    << "printf '%s\\n' \"$0\" \"$@\" > \"$dir/argv\"\n"
		    << "cat > \"$dir/stdin\"\n"
		    << "env > \"$dir/env\"\n"
		    << "if [ \"${ZLYME_TEST_FAIL:-0}\" = 1 ]; then exit 1; fi\n"
		    << "printf '%s\\n' 'zlyme:980:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX:8846F7EAEE8FB117AD06BDD830B7586C:[U          ]:LCT-00000001' > \"$ZLYME_TEST_ACCOUNT\"\n"
		    << "exit 0\n";
	}
	if (chmod(auth.c_str(), 0755) != 0) {
		std::cerr << "chmod auth failed\n";
		return 1;
	}
	{
		std::ofstream out(account);
		out << def;
	}
	setenv("ZLYME_TEST_DIR", dir, 1);
	setenv("ZLYME_TEST_ACCOUNT", account.c_str(), 1);
	setenv("ZLYME_TEST_FAIL", "1", 1);
	std::string out_text;
	std::string err_text;
	int rc = zlyme_tool_exec(auth.c_str(), args, &stdin_text, &out_text, &err_text);
	expect(rc != 0, "backend failure");
	expect_eq(slurp(account), def, "failed update kept the account");
	expect_eq(zlyme_about_samba_password(slurp(account)), "zlyme", "about unchanged after failure");
	std::string argv = slurp(std::string(dir) + "/argv");
	std::string env = slurp(std::string(dir) + "/env");
	std::ifstream stdin_in(std::string(dir) + "/stdin", std::ios::binary);
	std::string got_stdin((std::istreambuf_iterator<char>(stdin_in)), std::istreambuf_iterator<char>());
	expect(argv.find(password) == std::string::npos, "password not in argv");
	expect(env.find(password) == std::string::npos, "password not in environment");
	expect_eq(got_stdin, password, "password arrived on stdin");
	expect(argv.find("\nset\n") != std::string::npos || argv.size() >= 4, "set was the argument");

	unsetenv("ZLYME_TEST_FAIL");
	rc = zlyme_tool_exec(auth.c_str(), args, &stdin_text, &out_text, &err_text);
	expect(rc == 0, "backend success");
	expect_eq(zlyme_about_samba_password(slurp(account)), "(changed)", "about after change");
	expect(slurp(account).find(password) == std::string::npos, "account file has no plaintext");

	std::string init = std::string(dir) + "/init";
	{
		std::ofstream out(init);
		out << "#!/bin/sh\n"
		    << "if [ \"$1\" = start ] && [ \"${ZLYME_TEST_INIT_FAIL:-0}\" = 1 ]; then\n"
		    << "  printf '%s\\n' 'Samba: authentication failed'\n"
		    << "  exit 1\n"
		    << "fi\n"
		    << "exit 0\n";
	}
	if (chmod(init.c_str(), 0755) != 0) {
		std::cerr << "chmod init failed\n";
		return 1;
	}
	setenv("ZLYME_TEST_INIT_FAIL", "1", 1);
	rc = zlyme_tool_exec(init.c_str(), {"start"}, nullptr, &out_text, &err_text);
	expect(rc != 0, "init failure");
	expect_eq(zlyme_samba_service_message(true, false, out_text),
		"Samba: authentication failed", "init failure is visible");
	expect(!zlyme_samba_flag_after(true, false), "flag off after init failure");

	if (fails) {
		std::cerr << fails << " samba settings checks failed\n";
		return 1;
	}
	std::cout << "samba settings ok\n";
	return 0;
}
