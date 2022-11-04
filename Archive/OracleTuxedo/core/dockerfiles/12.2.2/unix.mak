##############################################################################
#
#    $Id: //db/ed8/tests/xa/tuxedo/makefile#1 $
#
#    $$RW_INSERT_HEADER "dbyrs.str"
#
#
# Skleton makefile for the Tuxedo application used to test DB XA Module
# functionality.
#
# Fill out the parameters in angle brackets <...>.
# Most of the fields can be filled out by referring in the makefile generated
# in the examples directory.
#
##############################################################################

# Tuxedo macros
BUILDSERVER=buildserver
BUILDCLIENT=buildclient

# Common macros
# The path to the root of the buildspace
BUILDSPACE= ../../..
# Use the source files in from here
SOURCEBUILDSPACE= ../../..
# The path to where the actual source is.
SRCDIR= ../../../examples/dbcore

# build configuration
TAG= 12s
CONFIGDEF= -D_RWCONFIG=$(TAG)

# C++ macros
CPPINVOKE=g++ -m64

# Compiling and Linking flags and linking libraries.
INCLUDES= -I${TUXDIR}/include -I"$(INFORMIXDIR)/incl/cli" -I$(BUILDSPACE) -I$(SRCDIR) -I../../../examples/informix/$(TAG)
COMPILEFLAGS= -O2 -pthread --pedantic -Wall -W -Wno-long-long \
-DRWDB_SERVERTYPE=\"libinf7012d.so\" \
-DRWDB_SERVERNAME=\"GRUNT\" -DRWDB_USER=\"dbtest1\" \
-DRWDB_PASSWORD=\"zebco5\" -DRWDB_DATABASE=\"qe1\" -DRWDB_PSTRING=\"\" \
-DRWDB_INFORMIX290 -DINFORMIX -DRWDBQE_TESTS_IN_FILENAME=\"tests.in\" \
-DRWDBQE_FACTORY_TEST_MANAGER_NAME=\"InformixTestManager\" \
-DRWDBQE_TEST_OUTPUT=\"manager.out\" -DRW_LOCALE=\"en_US\" \
-DRW_FRLOCALE=\"french\" -DRW_DELOCALE=\"german\" \
-DRWDBQE_TEST_COMPILER=\"gcc-4.1.1\" -DRWDBQE_TEST_MACHINE=\"redhat_as-5.0-em64t\" \
-DAUTOTEST
LINKER= g++ -m64
LINKFLAGS= -pthread -L$(BUILDSPACE)/lib -L"$(INFORMIXDIR)/lib" \
-L"$(INFORMIXDIR)/lib/cli" 
LINKLIBS= -lm -ldl -ltlt1012d -linf7012d -ldbt7012d -ltls8012d -lnsl 



# Build everything.
all: tututil.o testxaclient testxaserver deploy

# Adjust the following rule per the OS/Compiler/Build requirement.
# Refer to a compile line in the generated examples makefile.
tututil.o: $(SRCDIR)/tututil.cpp
	$(CPPINVOKE) $(CONFIGDEF) $(INCLUDES) $(COMPILEFLAGS) \
 -c $(SRCDIR)/tututil.cpp -o $(@)




# Adjust the following rule per the OS/Compiler/Build requirement.
# Refer to a compile line in the generated examples makefile.
testxaclient.o: testxaclient.cpp
	$(CPPINVOKE) $(CONFIGDEF) $(INCLUDES) $(COMPILEFLAGS) \
 -c testxaclient.cpp -o $(@)

# Adjust the following rule per the OS/Compiler/Build requirement.
# Refer to a link line in the generated examples makefile.
# $(COMPILEFLAGS) and $(INCLUDES) are required, as a Tuxedo generated file
# is compiled in this step.
# RM name should be the same as entered in $TUXDIR/udataobj/RM file
# In case of a static build, also include the
# <buildSpace>/lib/rw<database><build>.o file. e.g.- ../../../lib/rwinf15s.o
testxaclient: testxaclient.o
	$(BUILDCLIENT)  -v -r INFORMIX-OnLine -f " $(COMPILEFLAGS) $(LINKFLAGS) \
 $(INCLUDES) tututil.o testxaclient.o  " -l " $(LINKLIBS) " -o $(@)




# Adjust the following rule per the OS/Compiler/Build requirement.
# Refer to a compile line in the generated examples makefile.
testxaserver.o: testxaserver.cpp
	$(CPPINVOKE) $(CONFIGDEF) $(INCLUDES) $(COMPILEFLAGS) \
 -c testxaserver.cpp -o $(@)

# Adjust the following rule per the OS/Compiler/Build requirement.
# Refer to a link line in the generated examples makefile.
# $(COMPILEFLAGS) and $(INCLUDES) are required, as a Tuxedo generated file
# is compiled in this step.
# RM name should be the same as entered in $TUXDIR/udataobj/RM file
# In case of a static build, also include the
# <buildSpace>/lib/rw<database><build>.o file. e.g.- ../../../lib/rwinf15s.o
testxaserver: testxaserver.o
	$(BUILDSERVER) -v -r INFORMIX-OnLine -k \
 -s SERV_INS_DATA,SERV_UPD_DATA,SERV_DEL_DATA,SERV_CHK_DATA \
 -f " $(COMPILEFLAGS) $(LINKFLAGS) $(INCLUDES) tututil.o testxaserver.o  "\
 -l " $(LINKLIBS) " -o $(@)


# Deploy rule
deploy:
	cp testxaclient /u01/oracle/user_projects
	cp testxaserver /u01/oracle/user_projects


# Cleanup rule
clean:
	rm -f *.o testxaclient testxaserver
