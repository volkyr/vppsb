/*
 *
 * Copyright (c) 2016 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
# INTRO:

This is a Vagrant based user demo environment for beginners with VPP

It walks a user through
- each of the commands,
- the expected output,
- gives a file of the commands used to try themselves

# REQUIREMENTS
- vagrant (1.8)
- virtualbox / vmware fusion

# GETTING STARTED
- clone the repo
- modify env.sh if needed and ```source ./env.sh```
- by default the VM uses 2 x CPUs and 4G RAM
- ```vagrant up```
- ... run the demo

# RUNNING DEMOs
- From the Host, where you ran ```vagrant up``` run ```./run <demoname>```

```./run <demoname>```

# DEMOs

## routing - directly connected routing
- Creates two network namespaces c1, c2
- A gateway interface for each on VPP
- Routes due to directly connected routes inserted into default FIB

## bridging - directly connected interfaces into a bridge-domain
- Creates two network namespaces c1, c2
- Adds interfaces to VPP and add them to bridge-domain 1
- MAC addresses are automatically learned

## tracing - how to show a "day in the life of a packet" in VPP
- Same environment as "routing" demo
- How to add a trace
- View a trace
- Interpret a trace

