# A flowtable plugin

Provides a flowtable node to do flow classification, and associate a flow
context that can be enriched as needed by another node or an external
application. The objective is to be adaptable so as to be used for any
statefull use such as load-balancing, firewalling ...

Compared to the classifier, it stores a flow context, which changes the following:
- a flow context (which can be updated with external informations)
- it can offload
- flows have a lifetime

## Build/Install

Patches summary:
 - The flowtable plugin: 0001-Add-flowtable-feature.patch
 - Example nodeusing the flowtable: 0002-Add-Port-Mirroring-feature.patch
 - The est scripts I used: 0003-test-scripts.patch


The tests scripts are used are under ft-test.
They tests the functionality versus the classifier.

Apply the patches to vpp:
```
git am -3 0001-Add-flowtable-feature.patch
git am -3 0002-Add-Port-Mirroring-feature.patch
git am -3 0003-test-scripts.patch
```

After that, you shoud be able to build the plugins as part of the make *plugins*
target as usual:
```
make bootstrap
make build
make build-vpp-api
make plugins
```

## Test

In itself, the flowtable only provides a context and an API.
I added a simple port-mirroring as a test.

I might add some other test examples if needed.

## Administrative

### Current status

Currently :
- Default behavior is to connect transparently to given interface.
- Can reroute packets to given node
- Can receive additional information
- Can offload sessions
- Only support IP packets

I'm still working on this, do not support multithreading, and haven't
gone through performance tests yet.

Planned :
- improve flow statistics, and add a way to get them back at expiration time
- improve timer management (tcp sm state)

### Objective

The objective of this project is to provide a stateful node with flow-level
API, available for consecutive nodes or external applications.

### Main contributors

- Gabriel Ganne - gabriel.ganne@qosmos.com
- Christophe Fontaine - christophe.fontaine@qosmos.com
  (https://github.com/christophefontaine/flowtable-plugin)
