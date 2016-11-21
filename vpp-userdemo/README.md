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
# INTRO

This is a Vagrant based user demo environment for beginners with VPP

You can run the demo either through the GUI (recommended) or through the CLI

# REQUIREMENTS
- vagrant (1.8)
- virtualbox / vmware fusion

# GETTING STARTED
- clone the repo
- modify env.sh if needed and ```source ./env.sh```
- by default the VM uses 2 x CPUs and 4G RAM
- ```vagrant up```
- ... run the demo

# RUNNING DEMOS VIA THE GUI

- simply open up your favorite browser and point it at ` http://localhost:5000 `
- click on a tutorial from the list appearing on the side navigation bar. 
- Once the selected tutorial is loaded, click the "Next" button, or hit the SPACE bar to go through the steps.
- Each step is executing the shown command against the VM, showing the response on the console that appears at the bottom of the GUI.

# RUNNING DEMOs VIA THE CLI
- From the Host, where you ran ```vagrant up``` run ```./run tutorials/<demoname>```

# DEMOs

## Routing - directly connected routing
- Creates two network namespaces c1, c2
- A gateway interface for each on VPP
- Routes due to directly connected routes inserted into default FIB

## Bridging - directly connected interfaces into a bridge-domain
- Creates two network namespaces c1, c2
- Adds interfaces to VPP and add them to bridge-domain 1
- MAC addresses are automatically learned

## Tracing - how to show a "day in the life of a packet" in VPP
- Same environment as "routing" demo
- How to add a trace
- View a trace
- Interpret a trace

