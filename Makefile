include lvgl/lvgl.mk

TARGET := lvglshowcase 

CURR_DIR = $(shell pwd)

LVGL_SRCFILES := $(ASRCS) $(CSRCS)
# Convert absolute paths to relative paths under obj/
LVGL_OBJFILES := $(patsubst $(CURR_DIR)/%.c,obj/%.o,$(filter %.c,$(CSRCS))) \
                 $(patsubst $(CURR_DIR)/%.S,obj/%.o,$(filter %.S,$(ASRCS)))

$(info    LVGL_OBJFILES is $(LVGL_OBJFILES))


all: $(TARGET)

$(TARGET): $(LVGL_OBJFILES) obj/main.o 
	$(CC) $(CFLAGS) $(AFLAGS) $(LDFLAGS) $^ -o $@ $(SDL) -lSDL2 

obj/main.o: main.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@  

obj/%.o: %.c 
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@  

obj/%.o: %.S 
	@mkdir -p $(@D) 
	$(CC) $(AFLAGS) -c $< -o $@ 

clean:
	rm -f $(TARGET)
	rm -rdf obj

.PHONY: all clean 

