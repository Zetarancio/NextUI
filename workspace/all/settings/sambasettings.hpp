#pragma once

#include <string>
#include <vector>

// "Set" until an account file exists, then "Change". The file contents
// are not read here.
std::string zlyme_samba_password_label(bool have_account);

// An empty submission does not build a command and does not clear the
// account. A password is carried only in stdin_text. args is {"set"}.
bool zlyme_samba_password_command(const std::string &typed,
	std::vector<std::string> *args, std::string *stdin_text, std::string *error);

// A failed init script stays visible. Success is an empty message.
std::string zlyme_samba_service_message(bool want_on, bool init_ok, const std::string &output);

// A failed start puts the flag back off. A stop leaves it off.
bool zlyme_samba_flag_after(bool want_on, bool init_ok);

// argv is the program name plus args. input, when set, is written to
// stdin and is not placed in argv or the environment.
int zlyme_tool_exec(const char *program, const std::vector<std::string> &args,
	const std::string *input, std::string *out_text, std::string *err_text);
