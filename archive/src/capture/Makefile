TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = capture.cpp \
	DigiBetaDropoutDetector.cpp \
	LTCDecoder.cpp \
	FrameWriter.cpp \
	ArchiveMXFFrameWriter.cpp \
	D10MXFFrameWriter.cpp

C_SOURCES = sv_dummy.c


BROWSE_ENCODER_OBJECTS = $(BROWSE_ENCODER_DIR)/.c_objs/browse_encoder.o \
	$(BROWSE_ENCODER_DIR)/.objs/BrowseEncoder.o

PSE_OBJECTS = $(PSE_DIR)/.objs/pse_report_template.o \
	$(PSE_DIR)/.objs/pse_simple.o \
	$(PSE_DIR)/.objs/PSEReport.o
	
GENERAL_OBJECTS = $(GENERAL_DIR)/.objs/Logging.o \
	$(GENERAL_DIR)/.objs/RecorderException.o \
	$(GENERAL_DIR)/.objs/Timing.o \
	$(GENERAL_DIR)/.objs/Threads.o \
	$(GENERAL_DIR)/.objs/Utilities.o

RECMXF_OBJECTS = $(RECMXF_DIR)/.objs/MXFCommon.o \
	$(RECMXF_DIR)/.objs/ArchiveMXFContentPackage.o \
	$(RECMXF_DIR)/.objs/ArchiveMXFFile.o \
	$(RECMXF_DIR)/.objs/D10MXFContentPackage.o \
	$(RECMXF_DIR)/.objs/MXFWriter.o \
	$(RECMXF_DIR)/.objs/ArchiveMXFWriter.o \
	$(RECMXF_DIR)/.objs/D10MXFWriter.o \
	$(RECMXF_DIR)/.objs/MXFMetadataHelper.o \
	$(RECMXF_DIR)/.objs/MXFFileReader.o \
	$(RECMXF_DIR)/.objs/MXFEvents.o \
	$(RECMXF_DIR)/.objs/MXFEventFileWriter.o \
	$(RECMXF_DIR)/.objs/TimecodeBreakHelper.o 

ENCODER_OBJECTS = $(ENCODER_DIR)/.objs/FFmpegEncoder.o \
	$(INGEX_STUDIO_FFMPEG_OBJ)


MXF_INC = $(LIBMXFPP_INC) $(LIBMXFPP_D10MXFOP1AWRITER_INC) $(LIBMXFPP_COMMON_INC) $(LIBMXF_INC) $(LIBMXF_ARCHIVE_INC) $(LIBMXF_ARCHIVE_WRITE_INC) $(LIBMXF_READER_INC)
MXF_LIB = $(LIBMXFPP_D10MXFOP1AWRITER_LIB) $(LIBMXFPP_LIB) $(LIBMXF_ARCHIVE_INFO_LIB) $(LIBMXF_READER_LIB) $(LIBMXF_ARCHIVE_WRITE_LIB) $(LIBMXF_LIB)

INCLUDES += $(BROWSE_ENCODER_INC) $(PSE_INC) $(ENCODER_INC) $(DATABASE_INC) $(GENERAL_INC) $(INGEX_COMMON_INC) $(INGEX_YUVLIB_INC) $(MXF_INC) $(DVS_INC) $(FPA_INC) $(RECMXF_INC) $(INGEX_STUDIO_FFMPEG_INC) $(LTCSMPTE_INC)

LIBS = $(INGEX_COMMON_LIB) $(INGEX_YUVLIB_LIB) $(MXF_LIB) $(FFMPEG_LIB) $(LTCSMPTE_LIB)


.PHONY: all
all: $(OBJECTS) $(C_OBJECTS) test_capture dummy_capture test_ltc_decoder


test_capture: .objs/test_capture.o $(OBJECTS) $(BROWSE_ENCODER_OBJECTS) $(GENERAL_OBJECTS) $(PSE_OBJECTS) $(RECMXF_OBJECTS) $(ENCODER_OBJECTS)
	$(COMPILE) -o test_capture .objs/test_capture.o $(OBJECTS) $(BROWSE_ENCODER_OBJECTS) $(GENERAL_OBJECTS) $(PSE_OBJECTS) $(RECMXF_OBJECTS) $(ENCODER_OBJECTS) $(LIBS) -lncurses -lpthread -lrt $(FPA_LIB) $(DVS_LIB)

dummy_capture: .objs/test_capture.o $(OBJECTS) $(C_OBJECTS) $(BROWSE_ENCODER_OBJECTS) $(PSE_OBJECTS) $(GENERAL_OBJECTS) $(RECMXF_OBJECTS) $(ENCODER_OBJECTS)
	$(COMPILE) -o dummy_capture .objs/test_capture.o $(OBJECTS) $(C_OBJECTS) $(BROWSE_ENCODER_OBJECTS) $(PSE_OBJECTS) $(GENERAL_OBJECTS) $(RECMXF_OBJECTS) $(ENCODER_OBJECTS) $(LIBS) -lncurses -lpthread -lrt $(FPA_LIB)

test_ltc_decoder: .objs/LTCDecoder.o .objs/test_ltc_decoder.o $(GENERAL_OBJECTS) $(RECMXF_OBJECTS)
	$(COMPILE) .objs/test_ltc_decoder.o .objs/LTCDecoder.o $(GENERAL_OBJECTS) $(RECMXF_OBJECTS) $(INGEX_COMMON_LIB) $(MXF_LIB) $(LTCSMPTE_LIB) -lrt -o $@
	
	

.PHONY: clean
clean: cmn-clean
	@rm -f test_capture dummy_capture test_ltc_decoder

include $(TOPLEVEL)/rules.mk


