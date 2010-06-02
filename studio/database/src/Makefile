CC = g++
CFLAGS = -g -Wall -Wundef -W -Wno-unused-parameter -Werror
INCLUDES = -I. -I../../common -I../../../common

COMPILE = $(CC) $(CFLAGS)
AR = ar cr


SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))



.PHONY: all
all: libprodautodb.a generate_resolution_sql

libprodautodb.a: $(OBJECTS)
	$(AR) $@ $(OBJECTS)


.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@
	
	
generate_resolution_sql: libprodautodb.a .objs/generate_resolution_sql.o 
	$(CC) .objs/generate_resolution_sql.o -L. -lprodautodb -L../../common -lstudiocommon -o $@


.PHONY: clean
clean:
	@rm -f *.a *~
	@rm -Rf .objs
	@rm -Rf .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif
