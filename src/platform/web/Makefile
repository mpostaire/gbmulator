SRC:=$(wildcard *.c)
OBJ:=$(SRC:%.c=$(ODIR)/web/%.o)
BIN:=$(ODIR)/index.html

ODIR_STRUCTURE:=$(ODIR)/web

all: $(ODIR_STRUCTURE) $(BIN) $(ODIR)/favicon.png $(ODIR)/style.css

$(ODIR_STRUCTURE):
	mkdir -p $@

$(ODIR)/favicon.png: favicon.png
	cp $^ $@
$(ODIR)/style.css: style.css
	cp $^ $@

$(ODIR)/index.html: template.html $(OBJ)
	$(CC) -o $@ $(OBJ) $(ODIR)/common/common.a $(ODIR)/core/core.a $(CFLAGS) -lopenal -lidbfs.js -sINITIAL_MEMORY=128MB -sUSE_WEBGL2=1 -sWASM=1 -sEXPORTED_FUNCTIONS="['_main', '_malloc', '_free']" -sEXPORTED_RUNTIME_METHODS="['ccall', 'HEAPU8']" -sASYNCIFY --shell-file $<

$(ODIR)/web/%.o: ./%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD -MP $(LDLIBS)

-include $(OBJ:.o=.d)
