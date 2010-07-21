COMMON_DIR = ../../../common
STUDIOCOMMON_DIR = ../../common
DATABASELIB_DIR = ../src

CC = g++
CFLAGS = -g -Wall -Wundef -W -Wno-unused-parameter -Werror
INCLUDES = -I. -I$(COMMON_DIR) -I$(STUDIOCOMMON_DIR) -I$(DATABASELIB_DIR)

COMPILE = $(CC) $(CFLAGS) $(INCLUDES)

SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


.PHONY: all
all: test


$(DATABASELIB_DIR)/libprodautodb.a:
	@cd $(DATABASELIB_DIR) && $(MAKE) $@


test: $(DATABASELIB_DIR)/libprodautodb.a $(OBJECTS)
	$(CC) $(OBJECTS) -L$(DATABASELIB_DIR) -L$(COMMON_DIR) -L$(STUDIOCOMMON_DIR) -lprodautodb -lcommon -lstudiocommon -luuid -lpqxx -lxerces-c -lpthread -o $@

	
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
	@rm -f *.a *~ test
	@rm -Rf .objs
	@rm -Rf .deps
	

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
