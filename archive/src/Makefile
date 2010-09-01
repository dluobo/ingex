TOPLEVEL = .
include $(TOPLEVEL)/vars.mk

.PHONY: all
all:
	cd general && $(MAKE) $@
	cd mxf && $(MAKE) $@
	cd encoder && $(MAKE) $@
	cd browse-encoder && $(MAKE) $@
	cd generate-browse && $(MAKE) $@
	cd barcode-scanner && $(MAKE) $@
	cd libltcsmpte && $(MAKE) $@
	cd cache && $(MAKE) $@
	cd infax-access && $(MAKE) $@
	cd database && $(MAKE) $@
	cd http && $(MAKE) $@
	cd pse && $(MAKE) $@
	cd capture && $(MAKE) $@
	cd vtr && $(MAKE) $@
	cd recorder && $(MAKE) $@
	cd tape-io && $(MAKE) $@
	cd tape-export && $(MAKE) $@
	cd qc-report && $(MAKE) $@

.PHONY: clean
clean: cmn-clean
	cd general && $(MAKE) $@
	cd mxf && $(MAKE) $@
	cd encoder && $(MAKE) $@
	cd browse-encoder && $(MAKE) $@
	cd generate-browse && $(MAKE) $@
	cd barcode-scanner && $(MAKE) $@
	cd libltcsmpte && $(MAKE) $@
	cd cache && $(MAKE) $@
	cd infax-access && $(MAKE) $@
	cd database && $(MAKE) $@
	cd http && $(MAKE) $@
	cd pse && $(MAKE) $@
	cd capture && $(MAKE) $@
	cd vtr && $(MAKE) $@
	cd recorder && $(MAKE) $@
	cd tape-io && $(MAKE) $@
	cd tape-export && $(MAKE) $@
	cd qc-report && $(MAKE) $@



.PHONY: ingex-deps
ingex-deps:
	cd $(LIBMXF_DIR) && $(MAKE) all
	cd $(LIBMXFPP_DIR) && $(MAKE) all
	cd $(INGEX_COMMON_DIR) && $(MAKE) libcommon.a
	cd $(INGEX_COMMON_DIR)/YUVlib && $(MAKE) all
	cd $(INGEX_PLAYER_DIR) && $(MAKE) all
	cd $(JOGSHUTTLE_DIR) && $(MAKE) all
	cd $(INGEX_STUDIO_COMMON_DIR) && $(MAKE) ffmpeg_encoder.o
	cd $(INGEX_STUDIO_COMMON_DIR) && $(MAKE) ffmpeg_resolutions.o
	cd $(INGEX_STUDIO_COMMON_DIR) && $(MAKE) MaterialResolution.o
ifdef BBC_ARCHIVE_DIR
	cd $(BBC_ARCHIVE_DIR)/pse && $(MAKE) all
endif


.PHONY: clean-ingex-deps
clean-ingex-deps:
	cd $(LIBMXF_DIR) && $(MAKE) clean
	cd $(LIBMXFPP_DIR) && $(MAKE) clean
	cd $(INGEX_COMMON_DIR) && $(MAKE) clean
	cd $(INGEX_PLAYER_DIR) && $(MAKE) clean
	cd $(JOGSHUTTLE_DIR) && $(MAKE) clean
	cd $(INGEX_STUDIO_COMMON_DIR) && $(MAKE) clean
ifdef BBC_ARCHIVE_DIR
	cd $(BBC_ARCHIVE_DIR)/pse && $(MAKE) clean
endif



include $(TOPLEVEL)/rules.mk


