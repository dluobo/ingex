DATABASELIB_DIR = ../database/src
LIBMXF_DIR = ../../libMXF
WRITEAVIDMXF_DIR = $(LIBMXF_DIR)/examples/writeavidmxf
LIBMXFPP_DIR = ../../libMXF++
D10WRITER_DIR = $(LIBMXFPP_DIR)/examples/D10MXFOP1AWriter
LIBMXFPP_COMMON_DIR = $(LIBMXFPP_DIR)/examples/Common

CC = g++
CFLAGS = -g -Wall -Wundef -W -Wno-unused-parameter -Werror -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
INCLUDES = -I. -I$(DATABASELIB_DIR) -I$(LIBMXF_DIR)/lib/include -I$(WRITEAVIDMXF_DIR) -I$(LIBMXFPP_DIR) -I$(D10WRITER_DIR) -I$(LIBMXFPP_COMMON_DIR)

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


.PHONY: libMXF.a
libMXF.a:
	@cd $(LIBMXF_DIR)/lib && $(MAKE) $@

.PHONY: libwriteavidmxf.a
libwriteavidmxf.a:
	@cd $(WRITEAVIDMXF_DIR) && $(MAKE) $@

.PHONY: libMXF++.a
libMXF++.a:
	@cd $(LIBMXFPP_DIR)/libMXF++ && $(MAKE) $@

.PHONY: libD10MXFOP1AWriter.a
libD10MXFOP1AWriter.a:
	@cd $(D10WRITER_DIR) && $(MAKE) $@

.PHONY: libprodautodb.a
libprodautodb.a:
	@cd $(DATABASELIB_DIR) && $(MAKE) $@


libmxfwriter.a: $(LIBOBJECTS)
	$(AR) $@ $(LIBOBJECTS)
	
	
test_mxfwriter: libprodautodb.a libwriteavidmxf.a libMXF++.a libD10MXFOP1AWriter.a libMXF.a libmxfwriter.a .objs/test_mxfwriter.o 
	$(CC) .objs/test_mxfwriter.o -L. -L$(DATABASELIB_DIR) -L$(WRITEAVIDMXF_DIR) -L$(LIBMXF_DIR)/lib -L$(LIBMXFPP_DIR)/libMXF++ -L$(D10WRITER_DIR) -lprodautodb -lmxfwriter -lwriteavidmxf -lD10MXFOP1AWriter -lMXF++ -lMXF -lpq -lpqxx -luuid -lpthread -o $@

	
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
