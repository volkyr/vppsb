# Copyright (c) 2016 Cisco and/or its affiliates.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

AUTOMAKE_OPTIONS = foreign subdir-objects

AM_CFLAGS = -Wall -I@TOOLKIT_INCLUDE@

lib_LTLIBRARIES = librtnl.la

librtnl_la_SOURCES =     \
  librtnl/netns.c        \
  librtnl/rtnl.c         \
  librtnl/mapper.c

nobase_include_HEADERS =	\
  librtnl/netns.h           \
  librtnl/rtnl.h            \
  librtnl/mapper.h


lib_LTLIBRARIES += testrtnl_plugin.la

testrtnl_plugin_la_SOURCES = test/test.c
testrtnl_plugin_la_LDFLAGS = -module


if WITH_PLUGIN_TOOLKIT
install-data-hook:
	mkdir /usr/lib/vpp_plugins || true
	cp $(prefix)/lib/testnl_plugin.so.*.*.* /usr/lib/vpp_plugins
endif
