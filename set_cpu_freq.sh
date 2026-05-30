#!/bin/bash

freq="$1"

sudo cpupower frequency-set -u "$1"MHz
sudo cpupower frequency-set -d "$1"MHz


cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq