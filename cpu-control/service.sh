#!/system/bin/sh
# cpu-control: optional CPU max-frequency cap
#
# This is NOT reverting an overclock - investigation confirmed shas-noc/
# shas-susfs run this chip at its genuine stock ceiling (2000000 kHz big
# cluster, 1700000 kHz little cluster), selected by the SoC's own hardware
# efuse, not something our kernel config or code inflates (see
# _mt_cpufreq_get_cpu_level() - the boosted "PRO" OPP table is unreachable
# dead code in this driver, regardless of what the efuse reports).
#
# This module exists purely as an OPTIONAL, reversible way to run cooler /
# save battery by capping below stock, if you want that tradeoff. Defaults
# below are the stock values, i.e. a no-op out of the box. Uninstalling this
# module fully reverts to kernel-default governor behavior immediately.

MAX_FREQ_BIG=2000000
MAX_FREQ_LITTLE=1700000

for policy in /sys/devices/system/cpu/cpufreq/policy*; do
  [ -d "$policy" ] || continue
  max_avail=$(cat "$policy/cpuinfo_max_freq" 2>/dev/null)
  [ -z "$max_avail" ] && continue

  if [ "$max_avail" -gt 1800000 ]; then
    target=$MAX_FREQ_BIG
  else
    target=$MAX_FREQ_LITTLE
  fi

  echo "$target" > "$policy/scaling_max_freq" 2>/dev/null
done
