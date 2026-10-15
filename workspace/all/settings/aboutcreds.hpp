#pragma once

#include <string>

// Labels for Settings → About. Callers read the files when the page
// opens. These functions do not keep a password and do not return a hash.

std::string zlyme_about_ssh_password(bool ok, const std::string &text);
std::string zlyme_about_samba_password(const std::string &account_file);
std::string zlyme_about_syncthing_account(const std::string &config_xml);
std::string zlyme_about_syncthing_password(const std::string &config_xml);
