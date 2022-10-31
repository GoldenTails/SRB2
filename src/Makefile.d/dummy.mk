# Makefile options for a dummy backend.

makedir:=$(makedir)/dummy
sources+=$(call List,dummy/Sourcefile)

# no threading support on the literal dummy backend sorry
#ifndef NOTHREADS
#opts+=-DHAVE_THREADS
#sources+=dummy/i_threads.c
#endif

ifndef CYGWIN32
ifndef NOASM
USEASM=1
endif
endif

ifdef MINGW
opts+=-Umain
libs+=-mconsole
endif
