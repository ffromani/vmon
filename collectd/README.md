# the virt2 collectd plugin

`virt2` is an up-to-date plugin for collectd to monitor
VMs running on a host, using [libvirt](http://libvirt.org).

It is designed and implemented using the lessons learned and
the requirements provided by the [oVirt](http://www.ovirt.org)
project.

`virt2` requires libvirt >= 2.0.0

## resiliency against slow reads or slow storage

The virt2 plugin must cope with very long response time from libvirt.
There are well known causes for this slowness of libvirt (see below),
and this is actually a libvirt bug which will ultimately be fixed.
For the meantime, it is highly desiderable that the monitoring
application can cope with unresponsive libvirtd without getting stuck
in the same place.

### The problem

The single biggest cause for a libvirt API to be unresponsive (e.g.
return after a long period -even hours) is unresponsive or unreachable
storage.

I/O operations can block forever inside the kernel if shared storage
becomes unreachable. The process state will change to 'D' (dead).
QEMU is no exception to this. On data centers, is standard practice
to have shared storage, either file-based (NFS) or block-based
(ISCSI SANs, fiber-channel SANs).

To get the VM disk stats when the VM uses block storage, the management
application -libvirt- *must* use the QEMU monitor, which uses a simple
request/response JSON protocol.
Such call can very much become unresponsive if QEMU is attempting one
I/O on blocked storage. I/O are triggered by the guest, so they
are pretty much unpredictable for the monitoring application.

Being the QEMU monitor protocol so simple (request/response, without
asynchronous responses or pipelining), the monitoring application
must protect the monitor access with locks, or in general enforce
sequencing. Libvirtd uses locks, and this means that if one QEMU
monitor blocks, the calling thread is blocked as well.

Libvirtd also uses a worker thread pool. The libvirt client application
must be careful to not exaust the worker pools issuing calls which
can block, otherwise all libvirtd becomes unresponsive.

### Mitigations and solutions

A well-behaving monitoring application must unfortunately take in
account all the layers in the description of this problem. This
is a blatant layering violation, still it is the only way that leads
to some improvement, besides waiting for the lower layers to be fixed.

Unlike the I/O POSIX API, the libvirt API is blocking and cannot be made
unblocking. A monitoring application could (and almost always) use a worker
thread pool. To avoid to lose all the worker threads, the application may
want to enlarge the pool when one thread is stuck calling libvirt, and
reduce again the pool once those calls eventually unlock.

This works for the monitoring application, which keeps the pool running,
but depletes the libvirtd worker pool with no hopes of replenishing it,
so it just moves the problem down the stack.
[Vdsm](https://gerrit.ovirt.org/#/q/status%3Aopen+project%3Avdsm),
the node management daemon of oVirt, used this approach in versions 3.6
and 4.0, with limited success.

The key issue in the above approach is to avoid to deplenish the libvirt
worker pool. One way to do this could be to query libvirt for the available
worker pool size, using its
[admin API](http://events.linuxfoundation.org/sites/events/files/slides/libvirt-admin-api-kvm-forum_0.pdf).



BRAINDUMP
=========

- instances configurable via config dropin
  -- either implicitely or explicitely
  -- query libvirt to autotune?!?
- scan metadata for storage domain/affinity tag
- when listing domains, group domains by tag
- each instance will pick one subset of the tag set
  -- if possible one
  -- if possible always the same
- the above fall backs graciously to one big set if
  no hints are given
- optionally pre-scan each VM in the set for availability (ReadyForCommands)
