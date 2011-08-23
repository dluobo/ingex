DATABASELIB_DIR = ../../database/src
COMMON_DIR = ../../../common
STUDIOCOMMON_DIR = ../../common
INSTALL_WEB_DIR = /srv/ingex/deletepkg

CC = g++
CFLAGS = -g -Wall -Wundef -W -Wno-unused-parameter -Wno-non-virtual-dtor -Werror
INCLUDES = -I. -I$(DATABASELIB_DIR) -I$(COMMON_DIR)

COMPILE = $(CC) $(CFLAGS) $(INCLUDES)
LD = $(CC) -ldl -rdynamic

SOURCES = $(wildcard *.cpp)
OBJECTS = $(subst .cpp,.o,$(SOURCES))
DEPENDENCIES = $(subst .cpp,.d,$(SOURCES))


.PHONY: all
all: deletepkg


$(DATABASELIB_DIR)/libprodautodb.a:
	@cd $(DATABASELIB_DIR) && $(MAKE) libprodautodb.a

deletepkg: $(DATABASELIB_DIR)/libprodautodb.a $(OBJECTS)
	$(LD) $(OBJECTS) -Xlinker -rpath -Xlinker . -L$(DATABASELIB_DIR) -L$(COMMON_DIR) -L$(STUDIOCOMMON_DIR) -lprodautodb -lcommon -lstudiocommon -lpqxx -luuid -lpthread -lxerces-c -o $@



%.d : %.cpp
	@echo Generating dependency file for $<; \
	$(CC) $(CFLAGS) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o: %.cpp
	$(COMPILE) -c $< -o $@	
	

.PHONY: install-web
install-web: deletepkg
	mkdir -p $(INSTALL_WEB_DIR); cp deletepkg $(INSTALL_WEB_DIR)

.PHONY: uninstall-web
uninstall-web:
	@rm -f $(INSTALL_WEB_DIR)/deletepkg

.PHONY: clean
clean:
	@rm -f *.d *.o *.a *~ deletepkg
	
	

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
	


