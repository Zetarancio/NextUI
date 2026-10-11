#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

// Recent throughput of newly downloaded bytes.
// now_ms is monotonic milliseconds. fresh is bytes received in the
// current attempt and does not include a resumed prefix. The result is
// -1 until the first window completes, then bytes per second. A full
// window with no new bytes is 0, so a stall does not keep a high rate.
// The last rate is held until the window elapses.
struct DlRate {
	static constexpr long long kWindowMs = 2000;
	long long window_start_ms = -1;
	long long window_start_fresh = 0;
	long long shown_bps = -1;

	void reset()
	{
		window_start_ms = -1;
		window_start_fresh = 0;
		shown_bps = -1;
	}

	long long sample(long long fresh, long long now_ms)
	{
		if (fresh < 0)
			fresh = 0;
		if (window_start_ms < 0 || fresh < window_start_fresh) {
			window_start_ms = now_ms;
			window_start_fresh = fresh;
			shown_bps = -1;
			return shown_bps;
		}
		long long dt = now_ms - window_start_ms;
		if (dt < kWindowMs)
			return shown_bps;
		long long db = fresh - window_start_fresh;
		if (dt <= 0 || db <= 0)
			shown_bps = 0;
		else
			shown_bps = (db * 1000) / dt;
		window_start_ms = now_ms;
		window_start_fresh = fresh;
		return shown_bps;
	}
};

inline void zlyme_fmt_rate(long long bps, char *buf, size_t n)
{
	if (!buf || n == 0)
		return;
	if (bps < 0) {
		buf[0] = '\0';
		return;
	}
	if (bps >= 1024LL * 1024LL) {
		double mib = (double)bps / (1024.0 * 1024.0);
		std::snprintf(buf, n, "%.1f MiB/s", mib);
	} else {
		long long kib = bps / 1024;
		std::snprintf(buf, n, "%lld KiB/s", kib);
	}
}
