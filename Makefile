include lvgl/lvgl.mk

TARGET := lvglshowcase 

CURR_DIR = $(shell pwd)

LVGL_SRCFILES := $(ASRCS) $(CSRCS)
# Convert absolute paths to relative paths under obj/
LVGL_OBJFILES := $(patsubst $(CURR_DIR)/%.c,obj/%.o,$(filter %.c,$(CSRCS))) \
                 $(patsubst $(CURR_DIR)/%.S,obj/%.o,$(filter %.S,$(ASRCS)))


all: $(TARGET)

$(TARGET): obj/main.o obj/de_font_montserrat_14.o example-person_small.o lvgl/src/font/lv_font_montserrat_24.c $(LVGL_OBJFILES) 
	$(CC) $(CFLAGS) $(AFLAGS) $(LDFLAGS) $^ -o $@ $(SDL) -lSDL2 

obj/%.o: %.c 
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@ $(SDL) 

obj/%.o: %.S 
	@mkdir -p $(@D) 
	$(CC) $(AFLAGS) -c $< -o $@ $(SDL)

clean:
	rm -f $(TARGET)
	rm -rdf obj

.PHONY: all clean 

