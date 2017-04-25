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

AM_CFLAGS = -Wall -fstack-protector -fPIC -Werror -g -DFORTIFY_SOURCE=2
if DEBUG
AM_CFLAGS += -O0 -DCLIB_DEBUG
vpp_build = vpp_debug-native
else
AM_CFLAGS += -O2
vpp_build = vpp-native
endif

if VPP_DIR_SET
vpp_install = @VPP_DIR@/build-root/install-$(vpp_build)
AM_CFLAGS += -I$(vpp_install)/vpp/include/
else
AM_CFLAGS += -Wall
endif

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
testrtnl_plugin_la_LIBADD = librtnl.la

if VPP_DIR_SET
install: install-am
	mkdir -p $(vpp_install)/vpp/lib64/vpp_plugins/
	cp $(prefix)/lib/testrtnl_plugin.so.*.*.* $(vpp_install)/vpp/lib64/vpp_plugins/testrtnl_plugin.so
	cp $(prefix)/lib/librtnl.so $(vpp_install)/vpp/lib64/librtnl.so
endif
