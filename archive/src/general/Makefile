TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = RecorderException.cpp \
	Logging.cpp \
	Threads.cpp \
	Utilities.cpp \
	DirectoryWatch.cpp \
	Config.cpp \
	Profile.cpp \
	Timing.cpp \
	FileLock.cpp \
	ByteArray.cpp


INCLUDES += $(DATABASE_INC) $(VTR_INC) $(DVS_INC)


.PHONY: all
all: $(OBJECTS)


.PHONY: clean
clean: cmn-clean
	
	

include $(TOPLEVEL)/rules.mk


