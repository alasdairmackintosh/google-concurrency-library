#!/bin/sh

# Copyright 2011 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if test -r /proc/cpuinfo
then
	# Linux
	echo '-j' `grep -c processor /proc/cpuinfo`
elif test -x /usr/sbin/system_profiler
then
	# Mac OS X
	# from http://tech.serbinn.net/2010/
	# detect-number-of-cpu-cores-in-shell-script-mac-os-x-snow-leopard/
	/usr/sbin/system_profiler -detailLevel full SPHardwareDataType |
		awk '/Total Number Of Cores/ {printf("-j %d\n", $5)};'
#else unknown
	# assume one job, so no -j flag
fi
