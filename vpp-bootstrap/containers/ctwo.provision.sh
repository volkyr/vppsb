#!/usr/bin/env bash
# Copyright (c) 2016 Intel Corporation
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

VPP_VERSION=v17.04
VPP_DIR=~/vpp
VPP_GIT="https://git.fd.io/vpp"

echo Cloning $VPP_GIT
git clone $VPP_GIT $VPP_DIR

# Install dependencies
echo Building $VPP_DIR
cd $VPP_DIR
git checkout -b $VPP_VERSION $VPP_VERSION
make UNATTENDED=yes install-dep

make wipe
(cd build-root/; make distclean)
rm -f build-root/.bootstrap.ok

# Build and install packaging
make bootstrap
make build
