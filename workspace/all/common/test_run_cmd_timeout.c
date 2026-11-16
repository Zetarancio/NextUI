/* Host exercise of runCmdTimeout. Not a Flip run.
 * The child is a shell the helper itself starts. */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include "utils.h"

static pid_t tracked;
static int failed;

static void expire(int sig)
{
	(void)sig;
	if (tracked > 1) {
		kill(-tracked, SIGKILL);
		kill(tracked, SIGKILL);
	}
	_exit(2);
}

static int alive(pid_t pid)
{
	if (pid <= 1)
		return 0;
	if (kill(pid, 0) == 0)
		return 1;
	return errno != ESRCH;
}

static int open_fds(void)
{
	int fd;
	int n = 0;

	for (fd = 0; fd < 256; fd++) {
		if (fcntl(fd, F_GETFD) >= 0)
			n++;
	}
	return n;
}

static int wait_for_pid(const char *path, pid_t *out)
{
	struct timespec start, now;

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (;;) {
		FILE *f = fopen(path, "r");
		long value = 0;

		clock_gettime(CLOCK_MONOTONIC, &now);
		if ((now.tv_sec - start.tv_sec) > 3)
			return -1;
		if (f) {
			if (fscanf(f, "%ld", &value) == 1 && value > 1) {
				fclose(f);
				if (!alive((pid_t)value))
					continue;
				*out = (pid_t)value;
				return 0;
			}
			fclose(f);
		}
	}
}

static void expect(int cond, const char *msg)
{
	if (!cond) {
		fprintf(stderr, "run-cmd: %s\n", msg);
		failed = 1;
	}
}

static void check_normal(void)
{
	char buf[64];
	int before = open_fds();
	int rc = runCmdTimeout("printf 'hello\\n'", buf, sizeof buf, 2000);

	expect(rc == 0, "echo did not exit 0");
	expect(strcmp(buf, "hello\n") == 0, "echo output mismatch");
	expect(open_fds() == before, "normal path leaked a descriptor");
}

static void check_bound(void)
{
	char buf[8];
	int before = open_fds();
	int rc = runCmdTimeout("printf 'abcdefghij'", buf, sizeof buf, 2000);

	expect(rc == 0, "bounded command failed");
	expect(buf[sizeof buf - 1] == '\0', "output was not terminated");
	expect(strlen(buf) < sizeof buf, "output overran the buffer");
	expect(open_fds() == before, "bounded path leaked a descriptor");
}

static void check_timeout(void)
{
	char path[] = "/tmp/zlyme-cmd-timeout-XXXXXX";
	int fd = mkstemp(path);
	char cmd[256];
	int before;
	int rc;
	pid_t pid = -1;

	expect(fd >= 0, "mkstemp failed");
	if (fd >= 0)
		close(fd);
	snprintf(cmd, sizeof cmd, "echo $$ > '%s'; exec sleep 30", path);
	before = open_fds();
	rc = runCmdTimeout(cmd, NULL, 0, 300);
	expect(rc == 124, "timeout did not return 124");
	{
		FILE *f = fopen(path, "r");
		long value = 0;
		if (f && fscanf(f, "%ld", &value) == 1)
			pid = (pid_t)value;
		if (f)
			fclose(f);
	}
	expect(pid > 1 && !alive(pid), "timeout left the child");
	if (alive(pid)) {
		kill(-pid, SIGKILL);
		kill(pid, SIGKILL);
		waitpid(pid, NULL, 0);
	}
	expect(open_fds() == before, "timeout leaked a descriptor");
	unlink(path);
}

struct cancel_job {
	char path[64];
	int rc;
};

static void *worker(void *arg)
{
	struct cancel_job *job = arg;
	char cmd[256];

	snprintf(cmd, sizeof cmd, "echo $$ > '%s'; exec sleep 30", job->path);
	job->rc = runCmdTimeout(cmd, NULL, 0, 20000);
	return NULL;
}

static void check_cancel(void)
{
	struct cancel_job job;
	pthread_t thread;
	int file;
	pid_t pid = -1;
	int before = open_fds();

	snprintf(job.path, sizeof job.path, "/tmp/zlyme-cmd-cancel-XXXXXX");
	file = mkstemp(job.path);
	expect(file >= 0, "cancel mkstemp failed");
	if (file >= 0)
		close(file);
	job.rc = -99;
	expect(pthread_create(&thread, NULL, worker, &job) == 0, "thread create");
	if (wait_for_pid(job.path, &pid) != 0) {
		pthread_cancel(thread);
		pthread_join(thread, NULL);
		expect(0, "child did not start");
		unlink(job.path);
		return;
	}
	tracked = pid;
	pthread_cancel(thread);
	pthread_join(thread, NULL);
	tracked = -1;
	expect(!alive(pid), "cancel left the direct child");
	expect(kill(-pid, 0) < 0 && errno == ESRCH, "cancel left the process group");
	if (alive(pid)) {
		kill(-pid, SIGKILL);
		kill(pid, SIGKILL);
		waitpid(pid, NULL, 0);
	}
	expect(open_fds() == before, "cancel leaked a descriptor");
	unlink(job.path);
}

int main(void)
{
	int i;

	signal(SIGALRM, expire);
	alarm(20);
	for (i = 0; i < 5; i++) {
		check_normal();
		check_bound();
		check_timeout();
		check_cancel();
	}
	alarm(0);
	if (failed)
		return 1;
	printf("run cmd timeout ok\n");
	return 0;
}
