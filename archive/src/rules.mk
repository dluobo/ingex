# dependency and object file targets. $(COMPILE) must be defined

.deps/%.d: %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $< -o $@	
	
.deps/main.d: main.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.c_deps/%.d: %.c
	@echo Generating dependency file for $<; \
	mkdir -p .c_deps; \
	$(C_COMPILE) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.c_objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.c_objs/%.o: %.c
	@mkdir -p .c_objs
	$(C_COMPILE) -c $< -o $@




# common clean target

.PHONY: cmn-clean
cmn-clean:
	@rm -f *~ *.a
	@rm -Rf .objs .deps
	@rm -Rf .c_objs .c_deps



# include the dependency targets 

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
-include $(C_DEPENDENCIES)
-include .deps/main.d
endif


