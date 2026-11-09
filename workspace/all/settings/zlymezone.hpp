#pragma once

#include <algorithm>
#include <string>
#include <vector>

// The name Settings shows for the clock. Callers adopt a value when
// Settings starts, when the picker opens, after a choice, or on reset.
// label() does not read the saved zone.
struct ZlymeZoneState {
	std::string name{"UTC"};

	void adopt(std::string zone)
	{
		if (zone.empty())
			zone = "UTC";
		name = std::move(zone);
	}

	const std::string &label() const { return name; }
};

// Keep idx on `now`, appending it when the short list did not have it.
inline void zlyme_zone_select(std::vector<std::string> &names, int &idx, const std::string &now)
{
	const std::string saved = now.empty() ? std::string("UTC") : now;
	if (std::find(names.begin(), names.end(), saved) == names.end())
		names.push_back(saved);
	for (int i = 0; i < (int)names.size(); i++) {
		if (names[i] == saved) {
			idx = i;
			return;
		}
	}
	idx = 0;
}

inline int zlyme_zone_step(int idx, int n, int step)
{
	if (n <= 0)
		return idx;
	const int hop = step > 0 ? step % n : (-step) % n;
	if (hop == 0)
		return idx;
	return step > 0 ? (idx + hop) % n : (idx + n - hop) % n;
}
