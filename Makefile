VERSION_0 = 5
VERSION_1 = 1
VERSION_2 = 10
BRANCH = $(VERSION_0).$(VERSION_1)
VERSION = $(BRANCH).$(VERSION_2)

LANG=

SRC = main.cc queue.cc support.cc \
    logging.cc periodic.cc httpserver.cc xmlserver.cc \
    stream.cc keyboard.cc server.cc client.cc \
    filterwheel.cc \
    tempmon.cc \
    monochromator.cc \
    sr630.cc \
    mm4005.cc \
    klingermc4.cc \
    unidex11.cc \
    unidex511.cc \
    ke617.cc \
    ke6514.cc \
    pm2813.cc \
    prm100.cc \
    labsphere.cc \
    udts370.cc \
    udts470.cc \
    thermalserver.cc \
    sbi4000.cc \
    newmark.cc \
    newportTH.cc

OBJECTS = main.o queue.o support.o \
    logging.o periodic.o httpserver.o xmlserver.o \
    stream.o keyboard.o server.o client.o \
    filterwheel.o \
    tempmon.o \
    monochromator.o \
    sr630.o \
    mm4005.o \
    klingermc4.o \
    unidex11.o \
    unidex511.o \
    ke617.o \
    ke6514.o \
    pm2813.o \
    prm100.o \
    labsphere.o \
    udts370.o \
    udts470.o \
    thermalserver.o \
    sbi4000.o \
    newmark.o \
    newportTH.o

LIBS = -lpthread

CPPFLAGS = -Wall -O3 -Wunused -g # -fno-default-inline
LDFLAGS = -g
RCS = *.cc *.h Makefile .build

all:	checkin .depends hydra

.depends:	$(SRC)
	@ echo "Creating .depends file."
	@ for cc in $(SRC) ; do gcc -MM -MF $$cc.d $$cc ; done
	@ cat *.d > .depends
	@ /bin/rm -f *.d

-include .depends

checkin:
	@- touch .cksum
	@- cat *.cc *.h Makefile | cksum > .cksum_0
	@- if ( ! cmp -s .cksum .cksum_0 ) ; then \
	    date +%F\ %X\ `hostname` > .stamp ; \
	    cat .stamp >> .build ; \
	    export BUILD=`cat .build | wc -l ` ; \
	    cat .build | wc -l > build.h ; \
	    cat *.cc *.h Makefile | cksum > .cksum ; \
	    ci -q -l$(VERSION).$${BUILD} -m${VERSION}.$${BUILD} -t.stamp $(RCS) ; \
	    echo -n "New " ; \
	fi
	@- /bin/rm -f .cksum_0
	@- echo -n "Build #" ; cat build.h ;
	@- touch version.cc

hydra:	$(OBJECTS) build.h
	g++ -O3 -c version.cc -DVERSION=\"$(VERSION)\"
	g++ -o hydra $(LDFLAGS) $(OBJECTS) version.o $(LIBS)

clean:
	/bin/rm -f *.o core core.* hydra .depends

veryclean:
	/bin/rm -f *.o core core.* hydra .depends DEBUG/* Logs/*
	/bin/rm -rf State/*

archive:
	tar -czf ../hydra_$(VERSION).tgz .

rcs: # Do this when adding a new file.
	-ci -f -l$(BRANCH) -m${BRANCH} -t.stamp $(RCS) ;
	-ci -f -l$(VERSION).1 -m${VERSION}.1 -t.stamp $(RCS)
