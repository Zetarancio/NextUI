#!/bin/sh
# Lid close requests mem. Screen timeout stays hybrid. No hardware run.
# shellcheck disable=SC1007
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
api=$ROOT/workspace/all/common/api.c
plat=$ROOT/workspace/my355/platform/platform.c

grep -q 'int PLAT_lidIsOpen' "$plat" || {
	echo "platform does not read the current lid state" >&2
	exit 1
}
if grep -n 'wakealarm' "$api" "$plat"; then
	echo "lid change touches the RTC wakealarm" >&2
	exit 1
fi

awk '
	/if \(pwr.requested_lid_sleep\)/ && !lid { lid = NR }
	lid && !lid_mem && /PWR_sleepNow\(\)/ { lid_mem = NR }
	/screenOffDelay > 0 && now - last_input_at/ && !timeout { timeout = NR }
	timeout && !hybrid && /PWR_sleep\(\)/ { hybrid = NR }
	/PAD_justReleased\(BTN_SLEEP\)/ && !pwr { pwr = NR }
	pwr && !pwr_mem && /PWR_sleepNow\(\)/ { pwr_mem = NR }
	END {
		if (!(lid && lid_mem && timeout && hybrid && pwr && pwr_mem))
			exit 1
		if (!(lid_mem < hybrid && hybrid < pwr_mem))
			exit 1
	}
' "$api" || {
	echo "lid, timeout, and power no longer have separate sleep calls" >&2
	exit 1
}

echo "nextui lid mem path ok"
