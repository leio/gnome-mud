SUBDIRS = handlers zmp

bin_PROGRAMS = gnome-mud

localedir = $(datadir)/locale

gnome_mud_CPPFLAGS =							\
	-DLOCALEDIR=\"$(localedir)\"					\
	-DPIXMAPSDIR=\""$(datadir)/pixmaps"\"				\
	-DGMPIXMAPSDIR=\""$(datadir)/icons/hicolor/scalable/apps"\"	\
	-DPREFIX=\""$(prefix)"\"					\
	-DSYSCONFDIR=\""$(sysconfdir)"\"				\
	-DDATADIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DUIDIR=\""$(pkgdatadir)"\"					\
	-DPKGDATADIR=\""$(pkgdatadir)"\"

gnome_mud_CFLAGS = \
	$(GMUD_CFLAGS) \
	$(MSP_CFLAGS) \
	-g -Wall -O0

gnome_mud_LDADD = \
	handlers/libtelnethandlers.la \
	zmp/libzmpcommands.la \
	$(GMUD_LIBS)	\
	$(Z_LIBS)	\
	$(MSP_LIBS)

gnome_mud_SOURCES =				\
	debug-logger.c				\
	debug-logger.h				\
	ecma48.h				\
	gnome-mud.c				\
	gnome-mud.h				\
	gnome-mud-builtins.c			\
	gnome-mud-builtins.h			\
	gnome-mud-icons.h			\
	mud-connection-view.c			\
	mud-connections.c			\
	mud-connections.h			\
	mud-connection-view.h			\
	mud-connection.c			\
	mud-connection.h			\
	mud-line-buffer.c			\
	mud-line-buffer.h			\
	mud-log.c				\
	mud-log.h				\
	mud-parse-alias.c			\
	mud-parse-alias.h			\
	mud-parse-base.c			\
	mud-parse-base.h			\
	mud-parse-trigger.c			\
	mud-parse-trigger.h			\
	mud-profile.c				\
	mud-profile.h				\
	mud-profile-manager.c			\
	mud-profile-manager.h			\
	mud-regex.c				\
	mud-regex.h				\
	mud-subwindow.c				\
	mud-subwindow.h				\
	mud-telnet.c				\
	mud-telnet.h				\
	mud-trigger.c				\
	mud-trigger.h				\
	mud-tray.c				\
	mud-tray.h				\
	mud-window.c				\
	mud-window.h				\
	mud-window-prefs.c			\
	mud-window-prefs.h			\
	mud-window-profile.c			\
	mud-window-profile.h			\
	utils.c					\
	utils.h

BUILT_SOURCES = \
	gnome-mud-builtins.c \
	gnome-mud-builtins.h

enumheaders = \
	mud-trigger.h

EXTRA_DIST = \
	gnome-mud-builtins.h.template \
	gnome-mud-builtins.c.template

CLEANFILES = \
	$(BUILT_SOURCES)

gnome-mud-builtins.h: $(enumheaders) gnome-mud-builtins.h.template
	( $(GLIB_MKENUMS) --template gnome-mud-builtins.h.template \
	    $(enumheaders) > gnome-mud-builtins.h.tmp \
	    && mv gnome-mud-builtins.h.tmp gnome-mud-builtins.h ) \
	    || ( rm -f gnome-mud-builtins.h.tmp && exit 1 )

gnome-mud-builtins.c: $(enumheaders) gnome-mud-builtins.c.template
	( $(GLIB_MKENUMS) --template gnome-mud-builtins.c.template \
	    $(enumheaders) > gnome-mud-builtins.c.tmp \
	    && mv gnome-mud-builtins.c.tmp gnome-mud-builtins.c ) \
	    || ( rm -f gnome-mud-builtins.c.tmp && exit 1 )


-include $(top_srcdir)/git.mk
