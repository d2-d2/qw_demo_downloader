CC=gcc -static
STRIP=strip

CFLAGS=-O2 -Wall -fno-strict-aliasing -DBUILD_STRL
LDFLAGS= `glib-config --libs` -static-libgcc -static-libstdc++
ASFLAGS= -DELF -x assembler-with-cpp -c
AS=$(CC)

ifneq ($(MAKECMDGOALS),python_only)
PERL_CFLAGS=	$(shell perl -MExtUtils::Embed -e ccopts) -DWITH_PERL
PERL_LDFLAGS=	$(shell perl -MExtUtils::Embed -e ldopts)
endif


ifneq ($(MAKECMDGOALS),perl_only)
PYTHON_CFLAGS=	-DWITH_PYTHON 
PYTHON_LDFLAGS=	-lpython2.7
endif



STRIPFLAGS=--strip-uneeded --remove-section.comment

OBJS=	\
	cl_cmd.o	\
	cl_ents.o	\
	cl_input.o	\
	cl_main.o	\
	cl_parse.o	\
	cl_tent.o	\
	cmd.o		\
	common.o	\
	com_msg.o	\
	crc.o		\
	cvar.o		\
	host.o		\
	net_chan.o	\
	net_udp.o	\
	sys_linux.o	\
	utils.o		\
	version.o	\
	zone.o		\

#	sys_x86.o	
PERL_OBJ = \
		perl.o\

PYTHON_OBJ = \
	     	python.o\

clean:
		rm *.o

all:		
		mkdir -p objects/
		(cd objects;make -f ../Makefile perl_only)
		
		

perl_only:	$(OBJS) $(PERL_OBJ)
		$(CC) $(CFLAGS) $^ $(LDFLAGS) -lm -o thin_client

python_only:	$(OBJS) $(PYTHON_OBJ)
		$(CC) $(CFLAGS) $^ $(LDFLAGS) -lm -o thin_client

both:		$(OBJS) $(PYTHON_OBJ) $(PERL_OBJ)
		$(CC) $(CFLAGS) $^ $(LDFLAGS) -lm -o thin_client

d2:		$(OBJS)
		$(CC) $(CFLAGS) $^ $(LDFLAGS) -lm -o thin_client

