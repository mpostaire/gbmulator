UI_DEPS += $(ODIR)/favicon.png $(ODIR)/style.css

SRC += $(wildcard $(SDIR)/platform/$(PLATFORM)/*.c)
OBJ := $(SRC:$(SDIR)/%.c=$(ODIR)/%.o)
BIN := $(ODIR)/index.html

CFLAGS += -Wno-unused-command-line-argument
LDLIBS += -lopenal -lidbfs.js -lz

CC = emcc

$(BIN): $(SDIR)/platform/$(PLATFORM)/template.html $(OBJ) $(UI_DEPS)
	$(CC) -o $@ $(OBJ) $(CFLAGS) $(LDLIBS) \
		-sINITIAL_MEMORY=128MB -sUSE_WEBGL2=1 -sUSE_ZLIB=1 -sWASM=1 \
		-sEXPORTED_FUNCTIONS="['_main', '_malloc', '_free']" \
		-sEXPORTED_RUNTIME_METHODS="['ccall', 'HEAPU8']" \
		-sASYNCIFY --shell-file $<

$(ODIR)/favicon.png: $(SDIR)/platform/$(PLATFORM)/favicon.png
	cp $^ $@
$(ODIR)/style.css: $(SDIR)/platform/$(PLATFORM)/style.css
	cp $^ $@
