TOPLEVEL = ../..
include $(TOPLEVEL)/vars.mk

COMMON_DIR = ../Common
INCLUDES += -I$(COMMON_DIR)

SOURCES = OPAtomContentPackage.cpp \
	FrameOffsetIndexTable.cpp \
	RawEssenceParser.cpp \
	FixedSizeEssenceParser.cpp \
	VariableSizeEssenceParser.cpp \
	PCMEssenceParser.cpp \
	OPAtomClipReader.cpp \
	OPAtomTrackReader.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))

COMMON_SOURCES = DynamicByteArray.cpp
COMMON_OBJECTS = $(patsubst %.cpp,$(COMMON_DIR)/.objs/%.o,$(COMMON_SOURCES))
DEPENDENCIES += $(patsubst %.cpp,.deps/%.d,$(COMMON_SOURCES))



.PHONY: all
all: libOPAtomReader.a test_opatomreader



libOPAtomReader.a: $(OBJECTS) $(COMMON_OBJECTS)
	$(AR) $@ $(OBJECTS) $(COMMON_OBJECTS)



$(COMMON_OBJECTS):
	@cd $(COMMON_DIR) && $(MAKE) all

.deps/%.d : $(COMMON_DIR)/%.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(COMMON_DIR)/\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$


.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@



test_opatomreader: .objs/test_opatomreader.o libOPAtomReader.a
	$(COMPILE) .objs/test_opatomreader.o -L. -L$(LIBMXFPP_DIR)/libMXF++ -L$(LIBMXF_DIR)/lib -lOPAtomReader -lMXF++ -lMXF $(UUIDLIB) -o $@


.PHONY: install
install: all
	mkdir -p $(INSTALL_PREFIX)/lib
	cp libOPAtomReader.a $(INSTALL_PREFIX)/lib/
	mkdir -p $(INSTALL_PREFIX)/include/libMXF++/examples/OPAtomReader
	cp FrameOffsetIndexTable.h $(INSTALL_PREFIX)/include/libMXF++/examples/OPAtomReader/
	cp OPAtomClipReader.h $(INSTALL_PREFIX)/include/libMXF++/examples/OPAtomReader/
	cp OPAtomContentPackage.h $(INSTALL_PREFIX)/include/libMXF++/examples/OPAtomReader/
	cp OPAtomTrackReader.h $(INSTALL_PREFIX)/include/libMXF++/examples/OPAtomReader/
	cp PCMEssenceParser.h $(INSTALL_PREFIX)/include/libMXF++/examples/OPAtomReader/
	cp RawEssenceParser.h $(INSTALL_PREFIX)/include/libMXF++/examples/OPAtomReader/
	cp FixedSizeEssenceParser.h $(INSTALL_PREFIX)/include/libMXF++/examples/OPAtomReader/
	cp VariableSizeEssenceParser.h $(INSTALL_PREFIX)/include/libMXF++/examples/OPAtomReader/

.PHONY: uninstall
uninstall:
	rm -f $(INSTALL_PREFIX)/lib/libOPAtomReader.a
	rm -Rf $(INSTALL_PREFIX)/include/libMXF++/examples/OPAtomReader

.PHONY: clean
clean:
	@rm -f *~ *.a test_opatomreader
	@rm -Rf .objs
	@rm -Rf .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
-include .deps/test_opatomreader.d
endif
