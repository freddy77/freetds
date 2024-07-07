NULL =
TESTS =	\
	strings$(EXEEXT) \
	strtok_r$(EXEEXT) \
	strsep$(EXEEXT) \
	$(NULL)
check_PROGRAMS = $(TESTS)

strings_SOURCES = strings.c common.h
strtok_r_SOURCES = strtok_r.c common.h
strsep_SOURCES = strsep.c common.h

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/.. -I../
LDADD = ../../replacements/libreplacements.la $(NETWORK_LIBS)
EXTRA_DIST = CMakeLists.txt

