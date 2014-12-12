# vmon

## Rationale
[VDSM](http://gerrit.ovirt.org/p/vdsm)  ([github mirror](http://github.com/oVirt/vdsm)) is the node management daemon
for the [oVirt](http://www.oVirt.org) project, which is an excellent virtualization platform manager.

Among other task, VDSM is in charge to check the health of the VM running on a node, and to report
their status and health indicators to the oVirt central management entity.
To do that, and to manage VMs in general, VDSM makes great use of [libvirt](http://libvirt.org).

This task must be done efficiently and by consuming the less feasible amount of resources.
VDSM used to do this in python, and attempts to improve the performances and to reduce the consumption of resources
of this task while keeping the code in pure python have been made, and they have proven to be unsatisfactory.

vmon provides an efficient, fast and low-consumption way to gather statistics, tailored
to fit VDSM's needs and purposes.

## Goals
- Small memory footprint
- Quickest execution
- Very easy and seamless integration with [oVirt](http://www.ovirt.org)

## License
GPL v2+, same as VDSM

## Dependencies
- libuuid
- glib2 (>= 2.32)
- libvirt (>= 1.2.11)
