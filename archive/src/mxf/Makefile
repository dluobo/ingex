TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = MXFCommon.cpp \
	MXFWriter.cpp \
	MXFFileReader.cpp \
	ArchiveMXFContentPackage.cpp \
	ArchiveMXFFile.cpp \
	ArchiveMXFWriter.cpp \
	D10MXFContentPackage.cpp \
	D10MXFFile.cpp \
	D10MXFWriter.cpp \
	MXFMetadataHelper.cpp \
	MXFEventFileWriter.cpp \
	MXFEvents.cpp \
	MXFEventFile.cpp \
	TimecodeBreakHelper.cpp

MXF_INC = $(LIBMXFPP_INC) $(LIBMXFPP_D10MXFOP1AWRITER_INC) $(LIBMXFPP_COMMON_INC) $(LIBMXF_INC) $(LIBMXF_ARCHIVE_INC) $(LIBMXF_ARCHIVE_INFO_INC) $(LIBMXF_READER_INC) $(LIBMXF_ARCHIVE_WRITE_INC)

INCLUDES += $(INGEX_COMMON_INC) $(GENERAL_INC) $(MXF_INC) $(INGEX_STUDIO_FFMPEG_INC) $(ENCODER_INC)



.PHONY: all
all: $(OBJECTS)


.PHONY: clean
clean: cmn-clean



include $(TOPLEVEL)/rules.mk

