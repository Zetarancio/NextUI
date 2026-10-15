#include "sambasettings.hpp"

#include <cerrno>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

std::string zlyme_samba_password_label(bool have_account)
{
	return have_account ? "Change" : "Set";
}

bool zlyme_samba_password_command(const std::string &typed,
	std::vector<std::string> *args, std::string *stdin_text, std::string *error)
{
	if (typed.empty()) {
		if (error)
			*error = "Enter a Samba password.";
		return false;
	}
	if (args) {
		args->clear();
		args->push_back("set");
	}
	if (stdin_text)
		*stdin_text = typed;
	if (error)
		error->clear();
	return true;
}

static std::string samba_trim(std::string text)
{
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ' || text.back() == '\t'))
		text.pop_back();
	return text;
}

std::string zlyme_samba_service_message(bool want_on, bool init_ok, const std::string &output)
{
	if (init_ok)
		return "";
	std::string text = samba_trim(output);
	if (!text.empty())
		return text;
	return want_on ? "Samba did not start." : "Samba did not stop.";
}

bool zlyme_samba_flag_after(bool want_on, bool init_ok)
{
	return want_on && init_ok;
}

int zlyme_tool_exec(const char *program, const std::vector<std::string> &args,
	const std::string *input, std::string *out_text, std::string *err_text)
{
	int inp[2] = {-1, -1};
	int outp[2] = {-1, -1};
	int errp[2] = {-1, -1};
	if (!program || !program[0])
		return -1;
	if (pipe(outp) != 0)
		return -1;
	if (pipe(errp) != 0) {
		close(outp[0]);
		close(outp[1]);
		return -1;
	}
	if (input && pipe(inp) != 0) {
		close(outp[0]);
		close(outp[1]);
		close(errp[0]);
		close(errp[1]);
		return -1;
	}
	pid_t pid = fork();
	if (pid < 0) {
		close(outp[0]);
		close(outp[1]);
		close(errp[0]);
		close(errp[1]);
		if (input) {
			close(inp[0]);
			close(inp[1]);
		}
		return -1;
	}
	if (pid == 0) {
		if (input)
			dup2(inp[0], STDIN_FILENO);
		dup2(outp[1], STDOUT_FILENO);
		dup2(errp[1], STDERR_FILENO);
		if (input) {
			close(inp[0]);
			close(inp[1]);
		}
		close(outp[0]);
		close(outp[1]);
		close(errp[0]);
		close(errp[1]);
		std::vector<char *> argv;
		argv.push_back(const_cast<char *>(program));
		for (const auto &arg : args)
			argv.push_back(const_cast<char *>(arg.c_str()));
		argv.push_back(nullptr);
		execvp(program, argv.data());
		_exit(127);
	}
	if (input) {
		close(inp[0]);
		const char *p = input->data();
		size_t left = input->size();
		while (left > 0) {
			ssize_t n = write(inp[1], p, left);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			p += n;
			left -= (size_t)n;
		}
		close(inp[1]);
	}
	close(outp[1]);
	close(errp[1]);
	auto read_fd = [](int fd) {
		std::string text;
		char buf[256];
		for (;;) {
			ssize_t n = read(fd, buf, sizeof(buf));
			if (n < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			if (n == 0)
				break;
			text.append(buf, buf + n);
		}
		return text;
	};
	if (out_text)
		*out_text = read_fd(outp[0]);
	else
		read_fd(outp[0]);
	close(outp[0]);
	if (err_text)
		*err_text = read_fd(errp[0]);
	else
		read_fd(errp[0]);
	close(errp[0]);
	int status = 0;
	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR)
			return -1;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;
	return 0;
}
