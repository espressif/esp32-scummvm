bundle_name = esp32-dist/scummvm

libs: $(DETECT_OBJS) $(OBJS)
	ar rcsP libdetect.a $(DETECT_OBJS)
	ranlib libdetect.a

libscummvm.a: $(DETECT_OBJS) $(OBJS)
	ar r libscummvm.a $(DETECT_OBJS) $(OBJS)

esp32dist: all
	$(MKDIR) esp32dist/scummvm
	for i in $(DIST_FILES_DOCS); do sed 's/$$/\r/' < $$i > esp32dist/scummvm/`basename $$i`.txt; done
	$(CP) $(DIST_FILES_THEMES) esp32dist/scummvm/
ifneq ($(DIST_FILES_ENGINEDATA),)
	$(CP) $(DIST_FILES_ENGINEDATA) esp32dist/scummvm/
endif
ifneq ($(DIST_FILES_NETWORKING),)
	$(CP) $(DIST_FILES_NETWORKING) esp32dist/scummvm/
endif
ifneq ($(DIST_FILES_VKEYBD),)
	$(CP) $(DIST_FILES_VKEYBD) esp32dist/scummvm/
endif
