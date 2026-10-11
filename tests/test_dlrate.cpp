#include "../workspace/all/settings/dlrate.hpp"

#include <cstdio>
#include <cstring>

static int fails = 0;

static void expect_eq(const char *name, long long got, long long want)
{
	if (got == want)
		return;
	std::fprintf(stderr, "FAIL %s got %lld want %lld\n", name, got, want);
	fails++;
}

static void expect_str(const char *name, const char *got, const char *want)
{
	if (std::strcmp(got, want) == 0)
		return;
	std::fprintf(stderr, "FAIL %s got '%s' want '%s'\n", name, got, want);
	fails++;
}

int main()
{
	// Constant 200000 bytes / 2s = 100000 B/s. Held inside the window.
	{
		DlRate rate;
		expect_eq("const start", rate.sample(0, 0), -1);
		expect_eq("const early", rate.sample(100000, 1000), -1);
		expect_eq("const window", rate.sample(200000, 2000), 100000);
		long long held = rate.sample(250000, 2500);
		expect_eq("const held", held, 100000);
		for (int i = 0; i < 8; i++)
			expect_eq("const flicker", rate.sample(250000 + i, 2500 + i), held);
		expect_eq("const next", rate.sample(400000, 4000), 100000);
	}

	// Increasing, then decreasing. Each window replaces the shown rate.
	{
		DlRate rate;
		rate.sample(0, 0);
		expect_eq("up 1", rate.sample(100000, 2000), 50000);
		expect_eq("up hold", rate.sample(200000, 3000), 50000);
		expect_eq("up 2", rate.sample(400000, 4000), 150000);
		expect_eq("down", rate.sample(450000, 6000), 25000);
	}

	// A full window with no new bytes is a stall, not the previous rate.
	{
		DlRate rate;
		rate.sample(0, 0);
		expect_eq("stall high", rate.sample(200000, 2000), 100000);
		expect_eq("stall zero", rate.sample(200000, 4000), 0);
		expect_eq("stall held", rate.sample(200000, 5000), 0);
		expect_eq("stall still", rate.sample(200000, 6000), 0);
	}

	// Resume: fresh starts at 0. Five mebibytes already on disk are not
	// part of fresh, so they cannot inflate the rate. Total size is not
	// an input.
	{
		DlRate rate;
		const long long resumed_prefix = 5LL * 1024 * 1024;
		(void)resumed_prefix;
		expect_eq("resume start", rate.sample(0, 0), -1);
		expect_eq("resume early", rate.sample(0, 1000), -1);
		expect_eq("resume rate", rate.sample(2LL * 1024 * 1024, 2000), 1024LL * 1024);
	}

	// Retry from zero drops fresh and clears the previous rate.
	{
		DlRate rate;
		rate.sample(0, 0);
		expect_eq("retry before", rate.sample(200000, 2000), 100000);
		expect_eq("retry reset", rate.sample(0, 2500), -1);
		expect_eq("retry early", rate.sample(50000, 3500), -1);
		expect_eq("retry after", rate.sample(100000, 4500), 50000);
	}

	// Shorter than one window: no rate.
	{
		DlRate rate;
		expect_eq("short 0", rate.sample(0, 0), -1);
		expect_eq("short mid", rate.sample(50000, 500), -1);
		expect_eq("short end", rate.sample(80000, 1999), -1);
	}

	// Cancel drops the measurement.
	{
		DlRate rate;
		rate.sample(0, 0);
		rate.sample(200000, 2000);
		rate.reset();
		expect_eq("cancel", rate.sample(10, 10), -1);
	}

	char buf[32];
	zlyme_fmt_rate(-1, buf, sizeof(buf));
	expect_str("fmt hidden", buf, "");
	zlyme_fmt_rate(0, buf, sizeof(buf));
	expect_str("fmt zero", buf, "0 KiB/s");
	zlyme_fmt_rate(512 * 1024, buf, sizeof(buf));
	expect_str("fmt kib", buf, "512 KiB/s");
	zlyme_fmt_rate(1024LL * 1024 - 1, buf, sizeof(buf));
	expect_str("fmt below mib", buf, "1023 KiB/s");
	zlyme_fmt_rate(1024LL * 1024, buf, sizeof(buf));
	expect_str("fmt mib", buf, "1.0 MiB/s");
	zlyme_fmt_rate(3LL * 1024 * 1024 / 2, buf, sizeof(buf));
	expect_str("fmt 1.5", buf, "1.5 MiB/s");

	if (fails) {
		std::fprintf(stderr, "%d failed\n", fails);
		return 1;
	}
	std::printf("ok\n");
	return 0;
}
