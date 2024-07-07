NULL =
TESTS = \
	passarg$(EXEEXT) \
	condition$(EXEEXT) \
	mutex1$(EXEEXT) \
	dlist$(EXEEXT) \
	bytes$(EXEEXT) \
	smp$(EXEEXT) \
	path$(EXEEXT) \
	$(NULL)
check_PROGRAMS = $(TESTS)

passarg_SOURCES = passarg.c common.h
condition_SOURCES = condition.c common.h
mutex1_SOURCES = mutex1.c common.h
dlist_SOURCES = dlist.c common.h
smp_SOURCES = smp.c common.h
path_SOURCES = path.c common.h
if !HAVE_SSPI
TESTS += challenge$(EXEEXT)
challenge_SOURCES= challenge.c common.h
endif

AM_CPPFLAGS = -I$(top_srcdir)/include
LDADD = ../libtdsutils.la @NETWORK_LIBS@
EXTRA_DIST = CMakeLists.txt challenge.c
