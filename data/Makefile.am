desktopdir 	 = $(datadir)/applications
desktop_in_files = lapiz.desktop.in
desktop_DATA 	 = $(desktop_in_files:.desktop.in=.desktop)
$(desktop_DATA): $(desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

appdatadir = $(datadir)/metainfo
appdata_in_files = lapiz.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
$(appdata_DATA): $(appdata_in_files)
	$(AM_V_GEN) GETTEXTDATADIRS=$(top_srcdir) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

gsettings_SCHEMAS = org.cafe.lapiz.gschema.xml
@GSETTINGS_RULES@

man_MANS = lapiz.1

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lapiz.pc

bugreportdir = $(libexecdir)/lapiz
bugreport_SCRIPTS = lapiz-bugreport.sh

EXTRA_DIST = 			\
	$(appdata_in_files)	\
	$(desktop_in_files)	\
	$(man_MANS)			\
	lapiz.pc.in			\
	lapiz-bugreport.sh.in

CLEANFILES =	 			\
	$(desktop_DATA)			\
	$(gsettings_SCHEMAS)	\
	$(pkgconfig_DATA)		\
	$(appdata_DATA)




-include $(top_srcdir)/git.mk
