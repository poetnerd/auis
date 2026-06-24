
#		Bison and Parser Tests

# where to install 
DESTDIR = /usr/local

#where to make tests
TESTDIR = /tmp/bisontests

all:
	@echo usage:
	@echo 	to install all three subdirs:  	make Install
	@echo  	to test in two subdirs:      	make Test
	@echo	to clean all three subdirs:  	make Clean


Install:
	-mkdir ${DESTDIR}/bin
	-mkdir ${DESTDIR}/doc
	-mkdir ${DESTDIR}/doc/syntax 
	-mkdir ${DESTDIR}/etc
	-mkdir ${DESTDIR}/lib
	-mkdir ${DESTDIR}/include
	-mkdir ${DESTDIR}/help
	-mkdir ${DESTDIR}/share 
	-mkdir ${DESTDIR}/info 
	-mkdir ${DESTDIR}/man
	-mkdir ${DESTDIR}/man/man1
	(cd bison*; ./configure --prefix=${DESTDIR}; make install)
	(cd mkparser; make install DESTDIR=${DESTDIR})
	(cd C++parser; make install DESTDIR=${DESTDIR})


TESTFLAGS = BISON=${TESTDIR}/bin/bison  \
		YYFLAG=-DYYDEBUG   CDEBUGFLAGS=-g

Test:
	-mkdir ${TESTDIR}
	(make Install DESTDIR=${TESTDIR})
	(cd mkparser;  make testparse ${TESTFLAGS};  ./testparse)
	(cd C++parser; make testparser ${TESTFLAGS};  ./testparser)

Clean:
	(cd C++parser; make clean)
	(cd mkparser; make clean)
	(cd bison*; make clean)
	rm -f \#* ,* *~ *.CKP *.BAK *.bas errs core
	rm -f *.ln *.o *.dog tags TAGS make.log
	rm -f *.do *.eh *.ih *.a *.fwm *.snf *.pcf *.rtx *.fb
	