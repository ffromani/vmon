
PLUGIN_LDFLAGS = -module -avoid-version -export-symbols-regex '\<module_register\>'

lib_LTLIBRARIES = \
	virt.la \
	monitor.la \
	$(NULL)

monitor_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(NULL)
monitor_la_LIBADD = \
	$(AM_LDFLAGS) \
	$(NULL)
monitor_la_LDFLAGS = \
	$(PLUGIN_LDFLAGS) \
	$(NULL)

virt_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(LIBVIRT_CFLAGS) \
	$(LIBXML2_CFLAGS) \
	$(NULL)
virt_la_LIBADD = \
	$(AM_LDFLAGS) \
	$(LIBVIRT_LIBS) \
	$(LIBXML2_LIBS) \
	$(NULL)
virt_la_LDFLAGS = \
	$(PLUGIN_LDFLAGS) \
	$(NULL)

