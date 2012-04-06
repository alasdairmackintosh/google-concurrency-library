# Put the user's flags after the defaults, so the user can override
# the defaults.
CppFlags := $(CPPFLAGS)
AllCFlags := -pthread -Wall -Werror -g $(ALLCFLAGS)
CxxFlags := $(AllCFlags) $(CXXFLAGS)
CFlags := $(AllCFlags) $(CFLAGS)
LdFlags := $(LDFLAGS)
LdLibs := -pthread $(LDLIBS)


all: blackscholes

clean:
	find * -name '*.a' -o -name '*.o' -o -name '*.d' | xargs $(RM)

blackscholes: blackscholes.o


blackscholes: CppFlags += -I../../include -I.
blackscholes: blackscholes.o ../../std_thread.a ../../std_mutex.a ../../std_atomic.a
	$(CXX) -o $@ $(LdFlags) $^ $(LOADLIBES) $(LdLibs)


# Automatically rebuild when header files change:
DEPEND_OPTIONS = -MMD -MP -MF "$*.d.tmp"
MOVE_DEPENDFILE = then mv -f "$*.d.tmp" "$*.d"; \
                  else $(RM) "$*.d.tmp"; exit 1; fi

%.o: %.c
	if $(CC) -o $@ -c $(CppFlags) $(DEPEND_OPTIONS) $(CFlags) $< ; \
		$(MOVE_DEPENDFILE)

%.o: %.cc
	if $(CXX) -o $@ -c $(CppFlags) $(DEPEND_OPTIONS) $(CxxFlags) $< ; \
		$(MOVE_DEPENDFILE)

# Include the generated dependency makefiles.
ALL_SOURCE_BASENAMES := $(basename $(shell find * -name "*.c" -o -name "*.cc"))
-include $(ALL_SOURCE_BASENAMES:%=%.d)
