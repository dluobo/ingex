include ../../common/libmxf.mk

COMMON_DIR = ../../common
STUDIOCOMMON_DIR = ../common
DATABASELIB_DIR = ../database/src

CC = g++
CFLAGS = -g -Wall -Wundef -W -Wno-unused-parameter -Werror -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
INCLUDES = -I. -I$(COMMON_DIR) -I$(STUDIOCOMMON_DIR) -I$(DATABASELIB_DIR) \
	$(LIBMXF_INCLUDE) $(WRITEAVIDMXF_INCLUDE) $(LIBMXFPP_INCLUDE) \
	$(D10MXFOP1AWRITER_INCLUDE) $(LIBMXFPPEXAMPLESCOMMON_INCLUDE)
LIBS = -L$(DATABASELIB_DIR) -lprodautodb -lpqxx \
	-L$(COMMON_DIR) -lcommon -L$(STUDIOCOMMON_DIR) -lstudiocommon \
	$(WRITEAVIDMXF_LIB) $(D10MXFOP1AWRITER_LIB) $(LIBMXFPP_LIB) $(LIBMXF_LIB)
	
COMPILE = $(CC) $(CFLAGS) $(INCLUDES)
AR = ar cr

SOURCES = MXFWriterException.cpp \
	MXFUtils.cpp \
	MXFWriter.cpp \
	MXFOPAtomWriter.cpp \
	MXFOP1AWriter.cpp \
	test_mxfwriter.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))

LIBOBJECTS = $(OBJECTS)
LIBOBJECTS -= .objs/test_mxfwriter.o


.PHONY: all
all: libmxfwriter.a test_mxfwriter


.PHONY: libprodautodb.a
libprodautodb.a:
	@cd $(DATABASELIB_DIR) && $(MAKE) $@


libmxfwriter.a: $(LIBOBJECTS)
	$(AR) $@ $(LIBOBJECTS)
	
	
test_mxfwriter: libprodautodb.a libmxfwriter.a .objs/test_mxfwriter.o 
	$(CC) .objs/test_mxfwriter.o -L. -lmxfwriter $(LIBS) -o $@

	
.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@


.PHONY: clean
clean:
	@rm -f *.a *~ test_mxfwriter
	@rm -Rf .objs
	@rm -Rf .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
