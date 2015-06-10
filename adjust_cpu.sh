#!/bin/sh
 
echo 25 > /sys/devices/system/cpu/cpufreq/ondemand/up_threshold
echo 10 > /sys/devices/system/cpu/cpufreq/ondemand/sampling_down_factor
echo 1 > /sys/devices/system/cpu/cpufreq/ondemand/io_is_busy
echo 408000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq
echo perfomance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo 408000 > /sys/devices/system/cpu/cpu1/cpufreq/scaling_min_freq
echo perfomance > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor

#echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
 
#echo 1008000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq
#echo 912000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq
 
 
#echo performance > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
 
#echo 1008000 > /sys/devices/system/cpu/cpu1/cpufreq/scaling_max_freq
#echo 912000 > /sys/devices/system/cpu/cpu1/cpufreq/scaling_min_freq
