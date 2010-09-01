TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk

QC_REPORT_INSTALL_PREFIX = /usr/local


SOURCES = QCSessionFile.cpp \
	LTODirectory.cpp \
	QCReport.cpp \
	PSEReportWrapper.cpp \
	qc_report_template.cpp

GENERAL_OBJECTS = $(GENERAL_DIR)/.objs/Logging.o \
	$(GENERAL_DIR)/.objs/RecorderException.o

PSE_OBJECTS = $(PSE_DIR)/.objs/PSEReport.o \
	$(PSE_DIR)/.objs/pse_report_template.o
	
RECMXF_OBJECTS = $(RECMXF_DIR)/.objs/ArchiveMXFContentPackage.o \
	$(RECMXF_DIR)/.objs/ArchiveMXFFile.o \
	$(RECMXF_DIR)/.objs/MXFFileReader.o


MXF_INC = $(LIBMXF_INC) $(LIBMXF_ARCHIVE_INC) $(LIBMXF_ARCHIVE_INFO_INC) $(LIBMXF_READER_INC)
MXF_LIB = $(LIBMXF_ARCHIVE_INFO_LIB) $(LIBMXF_READER_LIB) $(LIBMXF_LIB)

INCLUDES += $(GENERAL_INC) $(MXF_INC) $(PSE_INC) $(RECMXF_INC) $(INGEX_COMMON_INC)



.PHONY: all
all: qc_report


qc_report_template.cpp: qc_report_template.html QCReport.cpp
	$(COMPILE) -DGEN_QC_REPORT_TEMPLATE=1 QCReport.cpp -o gen_qc_report_template
	./gen_qc_report_template qc_report_template.html $@


qc_report: .objs/main.o $(OBJECTS) $(GENERAL_OBJECTS) $(RECMXF_OBJECTS) $(PSE_OBJECTS)
	$(COMPILE) .objs/main.o $(OBJECTS) $(GENERAL_OBJECTS) $(RECMXF_OBJECTS) $(PSE_OBJECTS) $(INGEX_COMMON_LIB) $(MXF_LIB) -o $@



.PHONY: install
install: qc_report
	cp qc_report $(QC_REPORT_INSTALL_PREFIX)/bin
	cp qc_delete_cache_dir.sh $(QC_REPORT_INSTALL_PREFIX)/bin
	cp qc_report_script.sh $(QC_REPORT_INSTALL_PREFIX)/bin

.PHONY: uninstall
uninstall:
	rm -f $(QC_REPORT_INSTALL_PREFIX)/bin/qc_report
	rm -f $(QC_REPORT_INSTALL_PREFIX)/bin/qc_delete_cache_dir.sh
	rm -f $(QC_REPORT_INSTALL_PREFIX)/bin/qc_report_script.sh

	
.PHONY: clean
clean: cmn-clean
	@rm -f qc_report gen_qc_report_template qc_report_template.cpp
	



include $(TOPLEVEL)/rules.mk



