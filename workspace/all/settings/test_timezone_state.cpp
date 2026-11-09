// Host stand-in for the Settings timezone memory. It is not a menu draw.
#include "zlymezone.hpp"

#include <iostream>
#include <string>
#include <vector>

static int reads = 0;

static std::string fake_saved()
{
	reads++;
	return "Europe/Rome";
}

static int fail(const char *msg)
{
	std::cerr << "timezone-state: " << msg << "\n";
	return 1;
}

static void align(std::vector<std::string> &common, int &cidx,
	std::vector<std::string> &all, int &aidx, const ZlymeZoneState &state)
{
	zlyme_zone_select(common, cidx, state.label());
	zlyme_zone_select(all, aidx, state.label());
}

int main()
{
	ZlymeZoneState state;
	std::vector<std::string> common = {
		"UTC", "Europe/London", "Europe/Rome", "Asia/Tokyo"};
	std::vector<std::string> all = common;
	all.push_back("Antarctica/Troll");
	all.push_back("Pacific/Kiritimati");
	int cidx = 0;
	int aidx = 0;

	state.adopt(fake_saved());
	align(common, cidx, all, aidx, state);
	if (reads != 1)
		return fail("startup read the saved zone more than once");
	for (int i = 0; i < 40; i++) {
		if (state.label() != "Europe/Rome")
			return fail("the parent label left the memory copy");
		if (common[cidx] != "Europe/Rome" || all[aidx] != "Europe/Rome")
			return fail("a repeated label read changed a row");
	}
	if (reads != 1)
		return fail("drawing read the saved zone");

	reads = 0;
	state.adopt(fake_saved());
	align(common, cidx, all, aidx, state);
	if (reads != 1 || state.label() != "Europe/Rome")
		return fail("opening the picker did not refresh once");

	const int troll = (int)all.size() - 2;
	if (all[troll] != "Antarctica/Troll")
		return fail("the uncommon zone is not in the full list");
	state.adopt(all[zlyme_zone_step(aidx, (int)all.size(), troll - aidx)]);
	align(common, cidx, all, aidx, state);
	if (reads != 1)
		return fail("choosing a zone read the saved zone again");
	if (state.label() != "Antarctica/Troll" || common[cidx] != "Antarctica/Troll"
		|| all[aidx] != "Antarctica/Troll")
		return fail("an uncommon choice did not reach every label");
	if (common.back() != "Antarctica/Troll")
		return fail("the uncommon choice was not appended");

	const int tokyo = 3;
	state.adopt(common[zlyme_zone_step(cidx, (int)common.size(), tokyo - cidx)]);
	align(common, cidx, all, aidx, state);
	if (state.label() != "Asia/Tokyo" || common[cidx] != "Asia/Tokyo" || all[aidx] != "Asia/Tokyo")
		return fail("a common choice did not update every label");

	state.adopt("UTC");
	align(common, cidx, all, aidx, state);
	if (reads != 1)
		return fail("reset read the saved zone");
	if (state.label() != "UTC" || common[cidx] != "UTC" || all[aidx] != "UTC")
		return fail("reset did not return every label to UTC");
	if (zlyme_zone_step(2, 4, 0) != 2)
		return fail("a zero step moved the row");

	std::cout << "timezone state ok\n";
	return 0;
}
