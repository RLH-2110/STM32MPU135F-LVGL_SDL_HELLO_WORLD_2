include lvgl/lvgl.mk

TARGET := lvglshowcase 

CURR_DIR = $(shell pwd)

LVGL_SRCFILES := $(ASRCS) $(CSRCS)
# Convert absolute paths to relative paths under obj/
LVGL_OBJFILES := $(patsubst $(CURR_DIR)/%.c,obj/%.o,$(filter %.c,$(CSRCS))) \
                 $(patsubst $(CURR_DIR)/%.S,obj/%.o,$(filter %.S,$(ASRCS)))

CFLAGS += -Werror

OTHER_OBJ := obj/main.o obj/de_font_montserrat_14.o obj/example-person_small.o obj/lvgl/src/font/lv_font_montserrat_24.o 

all: $(TARGET)

$(TARGET): $(OTHER_OBJ) $(LVGL_OBJFILES) 
	$(CC) $(CFLAGS) $(AFLAGS) $(LDFLAGS) $^ -o $@ $(SDL) -lSDL2 

obj/%.o: %.c 
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@ $(SDL) 

obj/%.o: %.S 
	@mkdir -p $(@D) 
	$(CC) $(AFLAGS) -c $< -o $@ $(SDL)


obj/lvgl/src/font/lv_font_montserrat_24.o: lvgl/src/font/lv_font_montserrat_24.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -f $(TARGET)
	rm -rdf obj

.PHONY: all clean 

