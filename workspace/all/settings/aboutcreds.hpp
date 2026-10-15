#pragma once

#include <string>

// Labels for Settings → About. Callers read the files when the page
// opens. These functions do not keep a password and do not return a hash.
// Syncthing's text comes from zlyme-syncthing-gui. This file does not
// parse config.xml.

std::string zlyme_about_ssh_password(bool ok, const std::string &text);
std::string zlyme_about_samba_password(const std::string &account_file);
std::string zlyme_about_syncthing_account_label(const std::string &line);
std::string zlyme_about_syncthing_password_label(const std::string &line);
void zlyme_about_syncthing_read(const std::string &config_path, const std::string &helper,
	std::string &account, std::string &password);
