TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = Recorder.cpp \
	RecordingSession.cpp \
	RecordingItems.cpp \
	HTTPRecorder.cpp \
	ConfidenceReplay.cpp \
	PlayerState.cpp \
	Chunking.cpp \
	JogShuttleControl.cpp

GENERAL_OBJECTS=$(GENERAL_DIR)/.objs/Threads.o \
	$(GENERAL_DIR)/.objs/Logging.o \
	$(GENERAL_DIR)/.objs/RecorderException.o \
	$(GENERAL_DIR)/.objs/Utilities.o \
	$(GENERAL_DIR)/.objs/DirectoryWatch.o \
	$(GENERAL_DIR)/.objs/Config.o \
	$(GENERAL_DIR)/.objs/Profile.o \
	$(GENERAL_DIR)/.objs/Timing.o \
	$(GENERAL_DIR)/.objs/FileLock.o

BARCODE_SCANNER_OBJECTS = $(BARCODE_SCANNER_DIR)/.objs/BarcodeScanner.o \
	$(BARCODE_SCANNER_DIR)/.objs/BarcodeScannerFIFO.o
	
INFAX_ACCESS_OBJECTS = $(INFAX_ACCESS_DIR)/.objs/InfaxAccess.o \
	$(INFAX_ACCESS_DIR)/.objs/LocalInfaxAccess.o \
	$(INFAX_ACCESS_DIR)/.objs/LocalInfaxDatabase.o
	
DATABASE_OBJECTS = $(DATABASE_DIR)/.objs/PostgresDatabase.o \
	$(DATABASE_DIR)/.objs/DatabaseThings.o \
	$(DATABASE_DIR)/.objs/RecorderDatabase.o
	
CACHE_OBJECTS =	$(CACHE_DIR)/.objs/Cache.o

HTTP_OBJECTS = $(HTTP_DIR)/.objs/JSONObject.o \
	$(HTTP_DIR)/.objs/HTTPServer.o
	
VTR_OBJECTS = $(VTR_DIR)/.objs/SerialPort.o \
	$(VTR_DIR)/.objs/SonyProtocol.o \
	$(VTR_DIR)/.objs/VTRControl.o \
	$(VTR_DIR)/.objs/HTTPVTRControl.o \
	$(VTR_DIR)/.objs/DummyVTRControl.o

CAPTURE_OBJECTS = $(CAPTURE_DIR)/.objs/capture.o \
	$(CAPTURE_DIR)/.objs/DigiBetaDropoutDetector.o \
	$(CAPTURE_DIR)/.objs/LTCDecoder.o \
	$(CAPTURE_DIR)/.objs/FrameWriter.o \
	$(CAPTURE_DIR)/.objs/ArchiveMXFFrameWriter.o \
	$(CAPTURE_DIR)/.objs/D10MXFFrameWriter.o
	
BROWSE_ENCODER_OBJECTS = $(BROWSE_ENCODER_DIR)/.objs/BrowseEncoder.o \
	$(BROWSE_ENCODER_DIR)/.c_objs/browse_encoder.o

PSE_OBJECTS = $(PSE_DIR)/.objs/pse_report_template.o \
	$(PSE_DIR)/.objs/pse_simple.o \
	$(PSE_DIR)/.objs/PSEReport.o
	
RECMXF_OBJECTS = $(RECMXF_DIR)/.objs/MXFCommon.o \
	$(RECMXF_DIR)/.objs/ArchiveMXFContentPackage.o \
	$(RECMXF_DIR)/.objs/ArchiveMXFFile.o \
	$(RECMXF_DIR)/.objs/D10MXFContentPackage.o \
	$(RECMXF_DIR)/.objs/MXFWriter.o \
	$(RECMXF_DIR)/.objs/ArchiveMXFWriter.o \
	$(RECMXF_DIR)/.objs/D10MXFWriter.o \
	$(RECMXF_DIR)/.objs/MXFMetadataHelper.o \
	$(RECMXF_DIR)/.objs/D10MXFFile.o \
	$(RECMXF_DIR)/.objs/MXFFileReader.o \
	$(RECMXF_DIR)/.objs/MXFEvents.o \
	$(RECMXF_DIR)/.objs/MXFEventFileWriter.o \
	$(RECMXF_DIR)/.objs/MXFEventFile.o \
	$(RECMXF_DIR)/.objs/TimecodeBreakHelper.o 

ENCODER_OBJECTS = $(ENCODER_DIR)/.objs/FFmpegEncoder.o \
	$(ENCODER_DIR)/.objs/FFmpegDecoder.o \
	$(INGEX_STUDIO_FFMPEG_OBJ)
	
	
MXF_INC = $(LIBMXF_INC) $(LIBMXFPP_D10MXFOP1AWRITER_INC) $(LIBMXFPP_COMMON_INC) $(LIBMXF_ARCHIVE_INC) $(LIBMXF_ARCHIVE_WRITE_INC) $(LIBMXF_ARCHIVE_INFO_INC) $(LIBMXF_READER_INC)
MXF_LIB = $(LIBMXFPP_D10MXFOP1AWRITER_LIB) $(LIBMXFPP_LIB) $(LIBMXF_ARCHIVE_WRITE_LIB) $(LIBMXF_ARCHIVE_INFO_LIB) $(LIBMXF_READER_LIB) $(LIBMXF_LIB)


INCLUDES += $(GENERAL_INC) $(BARCODE_SCANNER_INC) $(INFAX_ACCESS_INC) $(DATABASE_INC) $(CACHE_INC) $(HTTP_INC) $(VTR_INC) $(BROWSE_ENCODER_INC) $(PSE_INC) $(CAPTURE_INC) $(MXF_INC) $(DVS_INC) $(FPA_INC) $(INGEX_COMMON_INC) $(INGEX_YUVLIB_INC) $(JOGSHUTTLE_INC) $(RECMXF_INC) $(LTCSMPTE_INC) $(INGEX_STUDIO_FFMPEG_INC) $(ENCODER_INC)


ALL_OBJECTS = $(OBJECTS) $(GENERAL_OBJECTS) $(RECMXF_OBJECTS) $(BARCODE_SCANNER_OBJECTS) $(INFAX_ACCESS_OBJECTS) $(DATABASE_OBJECTS) $(CACHE_OBJECTS) $(HTTP_OBJECTS) $(VTR_OBJECTS) $(CAPTURE_OBJECTS) $(BROWSE_ENCODER_OBJECTS) $(PSE_OBJECTS) $(ENCODER_OBJECTS)


.PHONY: all
all: ingex_recorder


ingex_recorder: .objs/main.o $(ALL_OBJECTS)
	$(COMPILE) .objs/main.o $(ALL_OBJECTS) $(INGEX_COMMON_LIB) $(INGEX_YUVLIB_LIB) $(DVS_LIB) $(MXF_LIB) $(FPA_LIB) $(FFMPEG_LIB) $(SHTTPD_LIB) $(LIBPQXX_LIB) $(JOGSHUTTLE_LIB) $(LTCSMPTE_LIB) -lcurl -lpthread -lrt -o $@


.PHONY: clean
clean: cmn-clean
	@rm -f ingex_recorder


include $(TOPLEVEL)/rules.mk

