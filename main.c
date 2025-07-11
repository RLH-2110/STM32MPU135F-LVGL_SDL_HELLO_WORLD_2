#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <poll.h>
#include <errno.h>

#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/evdev/lv_evdev.h"


#define LV_FONT_MONTSERRAT_24 1


#define H_RES (480)
#define V_RES (272)

#define LVGL_DESIRED_TICK_INCREASE_MS 1

#define NANOSECOND_TO_MILISECOND_RATE 1000000
#define MICROSECOND_TO_MILISECOND_RATE 1000

#define DROP_FRAMES 1 /* set to 1 to drop frames that would be send while the screen is drawing, instead of busy waiting for them*/
#define LOG_DROPPED_FRAMES 1

extern const lv_font_t lv_font_montserrat_24;
extern const lv_font_t de_font_montserrat_14;
extern const lv_image_dsc_t examplePersonImg;

/* lvgl stuff and threats */
lv_display_t *disp;
lv_indev_t *lvMouse;

pthread_t tickThread;
static void *tick_thread(void* data);
volatile sig_atomic_t noStop = 1; /* programms runs as long as this is set */

pthread_t fakeLoadrThread;
static void *load_main_screen_thread(void* data);

pthread_t pollDrmThread;
static void *poll_drm_thread(void *data);

/* screens */
void fake_loading_screen(void);
void main_screen(void);

/* callbacks */
void tab_view_value_changed_cb(lv_event_t *e);
void profile_img_click_cb(lv_event_t *e);
void feedback_send_button_click_cb(lv_event_t *e);
void contact_page_switch_cb(lv_event_t *e);
void contact_page_0_options_changed_cb(lv_event_t *e);

/* structs */

typedef struct two_pointers{ /* for callbacks, if we need to pass 2 thigns */
  void *p1;
  void *p2;
}two_pointers;
/* defines for specific uses of this struct */
#define TWO_PTR_INDEX p1
#define TWO_PTR_LV_OBJ_T p2

                   /* used to pass contact tab data to callbacks*/
typedef struct tabContact_data{
  lv_obj_t *contactRightButton;
  lv_obj_t *contactLeftButton;
  int page;
  lv_obj_t *page0;
  lv_obj_t *page1;
  bool getDateError;
} tabContact_data;



/* other */
lv_style_t germanFontStyle; /* style that includes german symbols */

            /* note: the defines that end in `VARIATION_MS` should have values lower than RAND_MAX */
#define FAKE_LOADING_TIME_MS 1500
#define FAKE_LOADING_TIME_VARIATION_MS 800
#define FAKE_LOADING_TIME_SPEED_MS 10000 /* time till it loops */
#define FAKE_LOADING_TIME_SPEED_VARIATION_MS 1200


lv_obj_t *chart = NULL;
lv_chart_series_t *chartSeries;
time_t nextUpdateTarget;
#define CHART_UPDATE_RATE_S 1 /* can take longer than written here*/
#define CHART_MIN_VALUE 0
#define CHART_MAX_VALUE 2000
#define CHART_START_VALUE 1000
#define CHART_RANDOM_RANGE 51          /* add 1, because this one counts 0 too*/
#define CHART_RANDOM_NEGATIVE_RANGE 50 /* this one does not count 0*/
#define CHART_INTERTIA 2.5 /* bigger values = less intertia. can't be 0! */
uint32_t chart_update(uint32_t currPoint, uint32_t inertia);
#define CHART_SCALE_TICKS 9
#define CHART_SCALE_TICK_EVERY 2
const char * chartScaleLabels[6] = {"0 ¢", "500 ¢", "1000 ¢", "1500 ¢", "2000 ¢", NULL};


const char *emailPrivat = "max.musterman@com.gmail";
const char *email = "geschäftlich.musterman@gmail.su";
const char *telPrivat = "+850 193 944 1526";
#define TEL_COUNTRIES 3
const char *tel[TEL_COUNTRIES] = {"+850 193 755 7086","+1 684-733-7310","+49 15647 144756"};

const char *countries = "Korea\nSamoa\nDeutchland";
#define EMAIL_BUFF_SIZE 50
char emailBuff[EMAIL_BUFF_SIZE] = { 0 };
#define TEL_BUFF_SIZE 40
char telBuff[TEL_BUFF_SIZE] = { 0 };

#define CONCAT(str1,str2,buff,buffSize) {if (concat(str1,str2,buff,buffSize) == NULL) { puts("contat had an error!"); noStop = 0; }}

/* sets random dates in the current month based on deterministic rng. returns false if the current date cant be determined */
extern bool set_calandar_dates(lv_obj_t *calendar);

/* wraps rand(), to avoid divisions by 0 */
unsigned int get_rand(unsigned int max_value_exclusive){
  if (max_value_exclusive == 0)
    return 0;
  return rand() % max_value_exclusive;
}


/* concatinates 2 strings without
  s1: first string
  s2: second string
  buffer: buffer where the combined string is safed
  bufferSize: size of the bufer

  returns: pointer to buffer on success, NULL on failire
*/
char* concat(const char *s1, const char *s2, char* buffer, size_t bufferSize);


/* signal handler */
void on_termination(int signal){
  noStop = 0;
}

/* scratch */

/* macro functions */

#define OBJ_POS_HALF_LEFT(obj)  (lv_obj_get_x(obj) - lv_obj_get_width(obj) / 2) /* gets position of the object if it were moved left by half the objects size */
#define OBJ_POS_HALF_RIGHT(obj) (lv_obj_get_x(obj) + lv_obj_get_width(obj) / 2)

#define OBJ_POS_FULL_RIGHT(obj) (lv_obj_get_x(obj) + lv_obj_get_width(obj)) /* gets position of the object, if it were moved right by the size of the object */
#define OBJ_POS_FULL_LEFT(obj) (lv_obj_get_x(obj) - lv_obj_get_width(obj))
#define OBJ_POS_FULL_UP(obj) (lv_obj_get_y(obj) - lv_obj_get_height(obj))
#define OBJ_POS_FULL_DOWN(obj) (lv_obj_get_y(obj) + lv_obj_get_height(obj))

#include <drm_fourcc.h>
#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdint.h>

/* drm */

# define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
# define DISPLAY_BUFF_SIZE H_RES * V_RES * BYTES_PER_PIXEL

bool setup_drm(void);
void cleanup_drm(void);
static uint32_t find_crtc(int drm_fd, drmModeRes *res, drmModeConnector *conn, uint32_t *taken_crtcs);

drmModeConnector *drm_conn = NULL;
uint32_t crtc = 0;
int drm_fd = -1;


uint8_t *fb0_data = NULL;
uint8_t *fb1_data = NULL;
uint8_t *back_fb_data = NULL;
uint8_t *front_fb_data = NULL;

struct drm_mode_create_dumb fb0;
struct drm_mode_create_dumb fb1;
uint32_t fb0_id;
uint32_t fb1_id;
uint32_t back_fb;
uint32_t front_fb;

volatile sig_atomic_t waitForFlip = 0;
drmModeCrtc *saved = NULL;

static void page_flip_handler(int drm_fd, unsigned sequence, unsigned tv_sec,	unsigned tv_usec, void *data)
{
  uint32_t tmp_fb = front_fb;
  front_fb = back_fb;
  back_fb = tmp_fb;

  uint8_t *tmp_fb_data = front_fb_data;
  front_fb_data = back_fb_data;
  back_fb_data = tmp_fb_data;

  waitForFlip = 0;
}

void flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map)
{

#if DROP_FRAMES
    if (waitForFlip != 0)
    {
#if LOG_DROPPED_FRAMES
      printf("dropped frame at %d\n",time(NULL));
#endif
      lv_display_flush_ready(display);
      return;
    }
#else
  while(waitForFlip)
    ;
#endif

    uint8_t * buff8 = px_map; /* 16 bit (RGB565) */
    int32_t area_width = (area->x2 - area->x1 + 1) * BYTES_PER_PIXEL;
    int32_t x,y;

    for(y = area->y1; y <= area->y2; y++) {
        uint8_t *dest = back_fb_data + (fb0.pitch * y) + (area->x1 * BYTES_PER_PIXEL);
        memcpy( dest , buff8, area_width);
        buff8 += area_width;
    }
    waitForFlip = 1;

    if (drmModePageFlip( 	drm_fd,	crtc, back_fb, DRM_MODE_PAGE_FLIP_EVENT, NULL) != 0){
      perror("PageFLip in flush_cb");
      waitForFlip = 0;
    } 	

    lv_display_flush_ready(display);

}

int main(){
  srand(time(NULL));

  puts("killing weston...");
  int code = system("pkill weston");
  if (code != 0){
    printf("could not kill weston | code: %d, aborting\n",code);
    _Exit(1); /* exit without atexit */
  }

  int i;
  for (i = 0; i < 50; ++i) { /* Wait up to ~5 seconds */
    if (system("pidof weston > /dev/null") != 0) {
      break; /* weston is gone */
    }
    usleep(100 * 1000); /* sleep 100ms */
  }
  if (i == 50) {
    printf("weston did not terminate in time, aborting\n");
    _Exit(1);
  }

  if (setup_drm() == false){
    goto exit_early;
  }

  lv_init();

  signal(SIGINT,on_termination);
  signal(SIGTERM,on_termination);
  if (pthread_create( &tickThread, NULL, tick_thread,NULL) != 0){
    puts("Error: could not create tick thread");
    return 1;
  }

  disp = lv_display_create(H_RES, V_RES);
  if (disp == NULL){
    puts("Display error!");
    return 1;
  }

  lv_display_set_flush_cb(disp, flush_cb);


  static uint8_t displayBuff1[DISPLAY_BUFF_SIZE];
  static uint8_t displayBuff2[DISPLAY_BUFF_SIZE];
  lv_display_set_buffers(disp, displayBuff1, displayBuff2, DISPLAY_BUFF_SIZE, LV_DISPLAY_RENDER_MODE_FULL);


  lvMouse = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event0");

  LV_IMAGE_DECLARE(examplePersonImg);  

  /* gui stuff here */

  lv_style_init(&germanFontStyle);
  lv_style_set_text_font(&germanFontStyle, &de_font_montserrat_14);

  fake_loading_screen();

  /* automatically call the main screen after a few seconds after the loading screen happens*/
  if(pthread_create( &fakeLoadrThread, NULL, load_main_screen_thread,NULL) != 0){
    main_screen(); /* just go to the main screen if we fail */
  }

  /* end gui stuff*/
  
  int32_t chartInertia = 0;
  int32_t chartCurrPoint = CHART_START_VALUE;
  i = 0;
  while(noStop){
    if (chart != NULL){
	if (time(NULL) > nextUpdateTarget){
          int32_t newPoint = chart_update(chartCurrPoint, chartInertia);
          chartInertia = (newPoint - chartCurrPoint) / CHART_INTERTIA;
          chartCurrPoint = newPoint;
          lv_chart_set_next_value(chart,chartSeries,chartCurrPoint);
 
          nextUpdateTarget += CHART_UPDATE_RATE_S;
        }
    }
    uint32_t timeTillNext = lv_timer_handler();
    usleep(timeTillNext*MICROSECOND_TO_MILISECOND_RATE ); 
  }
  
  puts("Preparing to exit...");
  pthread_join(fakeLoadrThread,NULL);
  pthread_join(tickThread,NULL);
  puts("counter joined");
  lv_indev_delete(lvMouse);
  puts("\"mouse\" deleted");
  lv_disp_remove(disp);
  puts("display removed");
  lv_deinit();
  puts("deinitalized lvgl");

  puts("cleaning up drm stuff...");
  cleanup_drm();
  puts("drm cleanup done");

  drmDropMaster(drm_fd);
  puts("dropped master");

exit_early:
  system("mkdir -p $XDG_RUNTIME_DIR");
  system("weston &");

  puts("restarted weston");
  puts("Exiting...");

  return 0;

}

uint32_t chart_update(uint32_t currPoint, uint32_t inertia)
{
  int randChange = get_rand(CHART_RANDOM_RANGE + CHART_RANDOM_NEGATIVE_RANGE) - CHART_RANDOM_NEGATIVE_RANGE + inertia;
  currPoint += randChange;
  
  if (currPoint < CHART_MIN_VALUE)
    currPoint = CHART_MIN_VALUE + get_rand(CHART_RANDOM_RANGE);
  else if (currPoint > CHART_MAX_VALUE)
    currPoint = CHART_MAX_VALUE - get_rand(CHART_RANDOM_NEGATIVE_RANGE);

  return currPoint;
}

/* screens */

void fake_loading_screen(void)
{
  static lv_obj_t *screen; screen = lv_obj_create(NULL);
  lv_obj_add_style(screen, &germanFontStyle, 0);

  lv_obj_t * spinner = lv_spinner_create(screen);
  lv_obj_set_size(spinner, 100, 100);
  lv_spinner_set_anim_params(spinner,FAKE_LOADING_TIME_SPEED_MS + get_rand(FAKE_LOADING_TIME_SPEED_VARIATION_MS) ,250); /* 250 is just a random angle. 0 looks ugly*/
  lv_obj_center(spinner);

  lv_screen_load_anim(screen,LV_SCR_LOAD_ANIM_NONE,0,0,true);
}

void main_screen(void)
{
  
  static lv_obj_t *screen; screen = lv_obj_create(NULL);
  lv_obj_add_style(screen, &germanFontStyle, 0);


  static lv_obj_t *tabView; tabView = lv_tabview_create(screen);
  lv_tabview_set_tab_bar_position(tabView, LV_DIR_LEFT);
  lv_tabview_set_tab_bar_size(tabView, 80);
  lv_obj_remove_flag(lv_tabview_get_content(tabView), LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * tab_buttons = lv_tabview_get_tab_bar(tabView);
  lv_obj_set_style_bg_color(tab_buttons, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
  lv_obj_set_style_text_color(tab_buttons, lv_palette_lighten(LV_PALETTE_GREY, 5), 0);


  lv_obj_t *tabHome      = lv_tabview_add_tab(tabView, "Kurs");
  lv_obj_t *tabBio       = lv_tabview_add_tab(tabView, "Bio");
  lv_obj_t *tabFeedback  = lv_tabview_add_tab(tabView, "Feedback");
  lv_obj_t *tabContact   = lv_tabview_add_tab(tabView, "Kontakt");
  lv_obj_t *tabExit      = lv_tabview_add_tab(tabView, "Beenden");


  static lv_obj_t *profileImg; profileImg = lv_imagebutton_create(tabView);
  lv_imagebutton_set_src(profileImg, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &examplePersonImg, NULL);
  //lv_obj_center(profileImg);
  lv_obj_align(profileImg, LV_ALIGN_TOP_RIGHT, -80, -80);


  /* events tab and profile image */
  {
    static uint32_t tabExitId; tabExitId = lv_obj_get_index(tabExit);
    lv_obj_add_event_cb(tabView,tab_view_value_changed_cb,LV_EVENT_ALL, &tabExitId); /* value changed event does not work, and this one is clsoe enough*/

    static uint32_t tabBioId; tabBioId = lv_obj_get_index(tabBio);
    static two_pointers pfpClickPtrUserDataStruct;
    pfpClickPtrUserDataStruct.TWO_PTR_INDEX = (void*)&tabBioId;
    pfpClickPtrUserDataStruct.TWO_PTR_LV_OBJ_T = (void*)tabView;
    lv_obj_add_event_cb(profileImg,profile_img_click_cb,LV_EVENT_CLICKED, &pfpClickPtrUserDataStruct); 
  }
  
  /* home tab */

    lv_obj_set_scrollbar_mode(tabHome, LV_SCROLLBAR_MODE_OFF);

    static lv_obj_t *chartLabel; chartLabel = lv_label_create(tabHome);
    if (CHART_UPDATE_RATE_S == 1)
      lv_label_set_text(chartLabel,"Geldverlust in cent jede Sekunde:");
    else
      lv_label_set_text_fmt(chartLabel,"Geldverlust in cent jede %d Sekunden:",CHART_UPDATE_RATE_S);
    //lv_obj_align_to(chartLabel,chart, LV_ALIGN_OUT_TOP_LEFT,0,-5);
    lv_obj_align(chartLabel, LV_ALIGN_OUT_TOP_LEFT,0,0);
 
    static lv_obj_t *chartScale; 

    /* might needs to be reworked when multiple screens exist */
    if (chart == NULL){
      chart = lv_chart_create(tabHome);
      lv_obj_align(chart, LV_ALIGN_CENTER, 0,15);
      lv_obj_set_height(chart, V_RES - 50);
      lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, CHART_MIN_VALUE, CHART_MAX_VALUE);
      lv_obj_set_scrollbar_mode(chart, LV_SCROLLBAR_MODE_OFF);
      lv_chart_set_div_line_count(chart,5,5);
      lv_obj_update_layout(chart);

      chartSeries = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_DEEP_ORANGE), LV_CHART_AXIS_PRIMARY_Y);
      nextUpdateTarget = time(NULL);

      chartScale = lv_scale_create(chart);
      lv_scale_set_mode(chartScale,LV_SCALE_MODE_VERTICAL_RIGHT);
      lv_scale_set_range(chartScale,CHART_MIN_VALUE,CHART_MAX_VALUE);
      lv_scale_set_total_tick_count(chartScale, CHART_SCALE_TICKS);
      lv_scale_set_major_tick_every(chartScale, CHART_SCALE_TICK_EVERY);
      lv_obj_set_style_pad_all(chartScale, 0, 0);
      lv_obj_set_size(chartScale,25, lv_obj_get_height(chart) - 20);
      lv_scale_set_text_src(chartScale, chartScaleLabels);
      lv_obj_add_style(chartScale, &germanFontStyle, 0);
    }
    
          

  /* bio tab */

    lv_obj_set_scrollbar_mode(tabBio, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *bio = lv_spangroup_create(tabBio);
    lv_obj_set_width(bio, H_RES - 135);
    lv_obj_set_height(bio, LV_SIZE_CONTENT);
    //lv_obj_center(bio);
    lv_obj_align(bio, LV_ALIGN_LEFT_MID,0,0);
  
    lv_span_t *bio1 = lv_spangroup_new_span(bio);
    lv_span_set_text(bio1, "Lorum Ipsum ");
    lv_style_set_text_color(lv_span_get_style(bio1), lv_palette_main(LV_PALETTE_RED));
    lv_style_set_text_font(lv_span_get_style(bio1),  &lv_font_montserrat_24);

    lv_span_t *bio2 = lv_spangroup_new_span(bio);
    lv_span_set_text(bio2, "dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.");

  /* feedback tab */

    lv_obj_set_scrollbar_mode(tabFeedback, LV_SCROLLBAR_MODE_OFF);

    static lv_obj_t *fbSliderSatisfaction; fbSliderSatisfaction = lv_slider_create(tabFeedback);
    lv_slider_set_range(fbSliderSatisfaction,1,10);
    lv_slider_set_value(fbSliderSatisfaction, 5 , LV_ANIM_OFF);    
    lv_obj_align(fbSliderSatisfaction, LV_ALIGN_TOP_MID,0,25);    

    static lv_obj_t *fbScaleSatisfaction; fbScaleSatisfaction = lv_scale_create(tabFeedback);
    lv_scale_set_mode(fbScaleSatisfaction, LV_SCALE_MODE_HORIZONTAL_TOP);
    lv_scale_set_range(fbScaleSatisfaction, 1,10);
    lv_scale_set_total_tick_count(fbScaleSatisfaction,10);
    lv_scale_set_major_tick_every(fbScaleSatisfaction, 1);
    lv_obj_align_to(fbScaleSatisfaction, fbSliderSatisfaction, LV_ALIGN_BOTTOM_LEFT, 0, 25);
    lv_obj_clear_flag(fbScaleSatisfaction, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_update_layout(fbSliderSatisfaction);
    lv_obj_set_width(fbScaleSatisfaction,    lv_obj_get_width(fbSliderSatisfaction));
    lv_obj_set_height(fbSliderSatisfaction , 10); 
    lv_obj_move_background(fbSliderSatisfaction);

    static lv_obj_t *fbLblSatisfaction; fbLblSatisfaction = lv_label_create(tabFeedback);
    lv_label_set_text(fbLblSatisfaction,"Zufridenheit:");
    lv_obj_align_to(fbLblSatisfaction, fbSliderSatisfaction, LV_ALIGN_OUT_TOP_LEFT, 0,-10);


    static lv_obj_t *fbTextarea; fbTextarea = lv_textarea_create(tabFeedback);
    lv_obj_align_to(fbTextarea, fbScaleSatisfaction, LV_ALIGN_OUT_BOTTOM_LEFT,0,10);
    lv_textarea_set_placeholder_text(fbTextarea, "Schreiben sie ihr Feedback hier!");

    static lv_obj_t *fbSendButton; fbSendButton = lv_button_create(tabFeedback);
    static lv_obj_t *fbSendBtnLabel; fbSendBtnLabel = lv_label_create(fbSendButton);
    lv_label_set_text(fbSendBtnLabel,"Absenden");
    lv_obj_align_to(fbSendButton, fbTextarea, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 10);

    lv_obj_add_event_cb(fbSendButton, feedback_send_button_click_cb, LV_EVENT_CLICKED, fbSliderSatisfaction);


  /* Contact tab */

    lv_obj_set_scrollbar_mode(tabContact, LV_SCROLLBAR_MODE_OFF);
    
    static tabContact_data contactTabD = { 0 }; /* initalize to null pointer, in case we forget to set something */
    contactTabD.page = 0;
    
    contactTabD.page0 = lv_obj_create(tabContact);   
    lv_obj_remove_style_all(contactTabD.page0);
    lv_obj_set_size(contactTabD.page0, lv_pct(100) , lv_pct(85));
    lv_obj_align(contactTabD.page0, LV_ALIGN_TOP_MID, 0, 0);

    contactTabD.page1 = lv_obj_create(tabContact);   
    lv_obj_remove_style_all(contactTabD.page1);
    lv_obj_set_size(contactTabD.page1, lv_pct(100) , lv_pct(85));
    lv_obj_align(contactTabD.page1, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_move_background( contactTabD.page1 );
    lv_obj_move_foreground( contactTabD.page0 );

    /* page 1 */   
    static lv_obj_t *contactCalandar; contactCalandar = lv_calendar_create(contactTabD.page1);
    lv_obj_align(contactCalandar, LV_ALIGN_CENTER, 0 ,15);
    lv_obj_add_flag(contactCalandar, LV_OBJ_FLAG_HIDDEN);

    static lv_obj_t *contactCalandarLbl; contactCalandarLbl = lv_label_create(contactTabD.page1);
    lv_obj_align_to(contactCalandarLbl, contactCalandar, LV_ALIGN_OUT_TOP_LEFT, 0, 0);
    lv_label_set_text(contactCalandarLbl, "Erreichbare Tage");
    lv_obj_add_flag(contactCalandarLbl, LV_OBJ_FLAG_HIDDEN);

    
    contactTabD.getDateError = !(set_calandar_dates(contactCalandar));
    

    /* all pages */

    contactTabD.contactRightButton = lv_button_create(tabContact);
    lv_obj_update_layout(contactTabD.contactRightButton);    
    lv_obj_align(contactTabD.contactRightButton, LV_ALIGN_BOTTOM_MID,  5 + (lv_obj_get_width(contactTabD.contactRightButton) / 2) ,0);

    static lv_obj_t *contactRightButtonLbl; contactRightButtonLbl = lv_label_create( contactTabD.contactRightButton);
    lv_label_set_text(contactRightButtonLbl, ">");

    contactTabD.contactLeftButton= lv_button_create(tabContact);
    lv_obj_update_layout(contactTabD.contactLeftButton);    
    lv_obj_align(contactTabD.contactLeftButton, LV_ALIGN_BOTTOM_MID,  -5 - (lv_obj_get_width(contactTabD.contactLeftButton) / 2) ,0);

    static lv_obj_t *contactLeftButtonLbl; contactLeftButtonLbl = lv_label_create( contactTabD.contactLeftButton);
    lv_label_set_text(contactLeftButtonLbl, "<"); 

    lv_obj_add_event_cb(contactTabD.contactRightButton, contact_page_switch_cb, LV_EVENT_CLICKED, &contactTabD);
    lv_obj_add_event_cb(contactTabD.contactLeftButton, contact_page_switch_cb, LV_EVENT_CLICKED, &contactTabD);
    lv_obj_set_style_bg_color(contactTabD.contactRightButton, lv_palette_main(LV_PALETTE_BLUE), LV_STATE_DEFAULT | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(contactTabD.contactRightButton, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_bg_color(contactTabD.contactLeftButton, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_STATE_DEFAULT | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(contactTabD.contactLeftButton, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);

    /* page 0 */
    
    static lv_obj_t *contactContactInfoLbl; contactContactInfoLbl = lv_label_create(contactTabD.page0);
    lv_obj_align(contactContactInfoLbl, LV_ALIGN_TOP_MID, 0 ,10);
    lv_label_set_text(contactContactInfoLbl,"Kontaktinfo: ");

     
    static lv_obj_t *contactCountryDropDown; contactCountryDropDown = lv_dropdown_create(contactTabD.page0);
    lv_obj_align_to(contactCountryDropDown, contactContactInfoLbl, LV_ALIGN_OUT_BOTTOM_LEFT, 0 ,10);
    lv_dropdown_set_options(contactCountryDropDown,countries);
    lv_obj_add_event_cb(contactCountryDropDown,contact_page_0_options_changed_cb, LV_EVENT_VALUE_CHANGED, &contactTabD);

    static lv_obj_t *contactPrivateCheckb; contactPrivateCheckb = lv_checkbox_create(contactTabD.page0);
    lv_obj_update_layout(contactPrivateCheckb);
    lv_obj_align_to(contactPrivateCheckb,contactCountryDropDown, LV_ALIGN_OUT_LEFT_MID, 0 ,0); 
    lv_obj_set_x(contactPrivateCheckb,lv_obj_get_x(contactPrivateCheckb)); /* position update did not happen yet, so we are aligning this to the y position of the dropdown, but we keep your x position */
    lv_checkbox_set_text(contactPrivateCheckb, "privat");
    lv_obj_add_event_cb(contactPrivateCheckb,contact_page_0_options_changed_cb, LV_EVENT_VALUE_CHANGED, &contactTabD);

#   define CONTACT_EMAIL_LABEL_INDEX 1
    static lv_obj_t *contactEmailLbl; contactEmailLbl = lv_label_create(contactTabD.page0);
    lv_obj_align(contactEmailLbl, LV_ALIGN_LEFT_MID, 10, 10);
    CONCAT("Email: ",email,emailBuff,EMAIL_BUFF_SIZE);
    lv_label_set_text(contactEmailLbl,emailBuff);
    lv_obj_add_style(contactEmailLbl, &germanFontStyle, 0);

#   define CONTACT_TEL_LABEL_INDEX 2
    static lv_obj_t *contactTelLbl; contactTelLbl = lv_label_create(contactTabD.page0);
    lv_obj_align_to(contactTelLbl,contactEmailLbl, LV_ALIGN_OUT_LEFT_BOTTOM, 0 , 20);
    CONCAT("Tel: ",tel[0],telBuff,TEL_BUFF_SIZE);
    lv_label_set_text(contactTelLbl,telBuff);
    lv_obj_update_layout(contactEmailLbl);
    lv_obj_set_x(contactTelLbl, lv_obj_get_x(contactEmailLbl)); /* set to same x as email (should be done with the align_to, but that does not like to rn) */  
 
  /* tabs end*/

  lv_screen_load_anim(screen,LV_SCR_LOAD_ANIM_NONE,0,0,true);
}


/* callbacks */
void tab_view_value_changed_cb(lv_event_t *e){
  
  lv_obj_t *tabView = lv_event_get_target_obj(e);
  int32_t exitTabId = *((int32_t*)lv_event_get_user_data(e));
  
  if (lv_tabview_get_tab_active(tabView) == exitTabId)
    noStop = 0;
}

void profile_img_click_cb(lv_event_t *e){
  two_pointers *data = (two_pointers*)lv_event_get_user_data(e);
  if (data->p1 == NULL || data->p2 == NULL){
    puts("ERROR: PROFILE_IMG_CLICK_CB GOT NULL PARAMETERS!!");
    return;
  }
  
  lv_obj_t *tabView = (lv_obj_t*)(data->TWO_PTR_LV_OBJ_T);
  int32_t tabBioIndex = *((uint32_t*)data->TWO_PTR_INDEX);

  lv_tabview_set_active(tabView, tabBioIndex, LV_ANIM_ON);
}

void feedback_send_button_click_cb(lv_event_t *e)
{

  lv_obj_t *slider = (lv_obj_t*)lv_event_get_user_data(e);

  static lv_obj_t *fbMessagebox; fbMessagebox = lv_msgbox_create(NULL);
  lv_msgbox_add_title(fbMessagebox,"Feedback");
  lv_obj_add_style(fbMessagebox, &germanFontStyle, 0);
  lv_msgbox_add_close_button(fbMessagebox);


  if (lv_slider_get_value(slider) <= 3)
    lv_msgbox_add_text(fbMessagebox, "Danke für Ihr Negatives Feedback, Sie erhalten eine Briefbombe inerhab von 3 Tagen.");
  else
    lv_msgbox_add_text(fbMessagebox, "Danke für Ihr Feedback, es wird nun direkt in den Müll gesendet.");
  
}

void contact_page_switch_cb(lv_event_t *e)
{
  lv_obj_t *callerBtn = lv_event_get_target(e);
  tabContact_data *contactTabD = lv_event_get_user_data(e);  

  #define page contactTabD->page

  printf("page cb | page %d | btn: %p | left %p\n",page,callerBtn,contactTabD->contactLeftButton);
  /* filter out button presses that would do nothing */
  if(page == 0 && callerBtn == contactTabD->contactLeftButton)
    return; 
  if(page == 1 && callerBtn == contactTabD->contactRightButton)
    return; 
  puts ("hi");

  if (page == 0){
    page = 1;
    lv_obj_move_background( contactTabD->page0 );
    lv_obj_move_foreground( contactTabD->page1 );
    lv_obj_set_style_bg_color(contactTabD->contactRightButton, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_STATE_DEFAULT | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(contactTabD->contactRightButton, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);
    lv_obj_set_style_bg_color(contactTabD->contactLeftButton, lv_palette_main(LV_PALETTE_BLUE), LV_STATE_DEFAULT | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(contactTabD->contactLeftButton, lv_palette_main(LV_PALETTE_BLUE), 0);

    for (int i = 0; i < lv_obj_get_child_count(contactTabD->page0); i++)
      lv_obj_add_flag(  lv_obj_get_child(contactTabD->page0,i), LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < lv_obj_get_child_count(contactTabD->page1); i++)
      lv_obj_clear_flag(  lv_obj_get_child(contactTabD->page1,i), LV_OBJ_FLAG_HIDDEN);
    
    if (contactTabD->getDateError){
       static lv_obj_t *getDateErrorMsg; getDateErrorMsg = lv_msgbox_create(NULL);
       lv_msgbox_add_title(getDateErrorMsg,"Fehler");
       lv_msgbox_add_close_button(getDateErrorMsg);
       lv_msgbox_add_text(getDateErrorMsg,"Datum konnte nicht herausgefunden werden!\nKalender ist deaktiviert.");
    }

  }else /* page == 1 */ {
    page = 0;
    lv_obj_move_background( contactTabD->page1 );
    lv_obj_move_foreground( contactTabD->page0 );
    lv_obj_set_style_bg_color(contactTabD->contactRightButton, lv_palette_main(LV_PALETTE_BLUE), LV_STATE_DEFAULT | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(contactTabD->contactRightButton, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_bg_color(contactTabD->contactLeftButton, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_STATE_DEFAULT | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(contactTabD->contactLeftButton, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);

    for (int i = 0; i < lv_obj_get_child_count(contactTabD->page0); i++)
      lv_obj_clear_flag(  lv_obj_get_child(contactTabD->page0,i), LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < lv_obj_get_child_count(contactTabD->page1); i++)
      lv_obj_add_flag(  lv_obj_get_child(contactTabD->page1,i), LV_OBJ_FLAG_HIDDEN);
  }
  #undef page
}

void contact_page_0_options_changed_cb(lv_event_t *e){

  lv_obj_t *page0 = ((tabContact_data*)lv_event_get_user_data(e))->page0;
  if (page0 == NULL) { 
    puts("contact_page_0_options_changed_cb: no page0!"); 
    return;
  }

  lv_obj_t *dropdownCountry = lv_obj_get_child_by_type(page0, 0, &lv_dropdown_class); 
  lv_obj_t *checkboxPrivate = lv_obj_get_child_by_type(page0, 0, &lv_checkbox_class); 

  lv_obj_t *emailLbl = lv_obj_get_child_by_type(page0, CONTACT_EMAIL_LABEL_INDEX, &lv_label_class); 
  lv_obj_t *telLbl = lv_obj_get_child_by_type(page0, CONTACT_TEL_LABEL_INDEX, &lv_label_class); 

 if (lv_obj_get_state(checkboxPrivate) & LV_STATE_CHECKED){
   lv_obj_add_state(dropdownCountry, LV_STATE_DISABLED);

   CONCAT("Email: ",emailPrivat,emailBuff,EMAIL_BUFF_SIZE);
   CONCAT("Tel: ",telPrivat,telBuff,EMAIL_BUFF_SIZE); 

   lv_label_set_text(emailLbl,emailBuff);
   lv_label_set_text(telLbl,telBuff);

   return;
 }
 lv_obj_clear_state(dropdownCountry, LV_STATE_DISABLED); 

 int countryIndex = lv_dropdown_get_selected(dropdownCountry); 

 if (countryIndex > TEL_COUNTRIES){
   printf("tel id %d is out of scope!\n",countryIndex);
   countryIndex = 0;
 }
   
 CONCAT("Email: ",email,emailBuff,EMAIL_BUFF_SIZE);
 CONCAT("Tel: ",tel[countryIndex],telBuff,EMAIL_BUFF_SIZE);
 
 lv_label_set_text(emailLbl,emailBuff);
 lv_label_set_text(telLbl,telBuff);
}


/* other functions */
static void *tick_thread(void* data) {
  (void) data;
  while(noStop) {
    usleep(LVGL_DESIRED_TICK_INCREASE_MS * MICROSECOND_TO_MILISECOND_RATE);
    lv_tick_inc(LVGL_DESIRED_TICK_INCREASE_MS); /*Tell LVGL how many time has eslaped in ms*/
  }
}

/* sets random dates in the current month based on deterministic rng. returns false if the current date cant be determined */
bool set_calandar_dates(lv_obj_t *calendar){
  time_t now = time(NULL);
  struct tm *date = localtime(&now);
  if (date == NULL){
    puts("could not get date!");
    return false;
  }

  /* tm_year + 1900, because tm_year is years since 1900. tm_month + 1 because tm_month is 0-11 and not 1-12*/
  lv_calendar_set_today_date(calendar,date->tm_year + 1900,date->tm_mon + 1, date->tm_mday);
  lv_calendar_set_month_shown(calendar,date->tm_year + 1900,date->tm_mon + 1);

  /*deterministic randomness*/
  srand(date->tm_year * 100 + date->tm_mon);
 
  #define CALANDER_RANDOM_DAYS 8
  static lv_calendar_date_t days[CALANDER_RANDOM_DAYS+1] = { 0 }; 
  #define CALANDER_RANDOM_MAX_DAY 27
  bool dayTaken[CALANDER_RANDOM_MAX_DAY+2] = { 0 };

  size_t i = 0;
  while(i < CALANDER_RANDOM_DAYS){
     int random = get_rand(CALANDER_RANDOM_MAX_DAY) + 1;    
     if (dayTaken[random])
       continue;

     dayTaken[random] = true;
     days[i].year = date->tm_year + 1900;
     days[i].month = date->tm_mon + 1;
     days[i].day = random;
     i++;
  }

  lv_calendar_set_highlighted_dates(calendar,days,i);

  /*get back to pseudo randomness*/
  srand(time(NULL));
  return true;
}


static void load_main_screen(void* data){
  main_screen();
}

static void *load_main_screen_thread(void* data) {
  usleep((FAKE_LOADING_TIME_MS + time(NULL) % FAKE_LOADING_TIME_SPEED_VARIATION_MS) * MICROSECOND_TO_MILISECOND_RATE); /* rand is not thread safe. and its not important enough for me to learn and use a good alternative just for this */
  lv_async_call(load_main_screen,NULL);
}

/* concatinates 2 strings without 
  s1: first string
  s2: second string
  buffer: buffer where the combined string is safed
  bufferSize: size of the bufer

  returns: pointer to buffer on success, NULL on failire
*/
char* concat(const char *s1, const char *s2, char* buffer, size_t bufferSize)
{
    const size_t len1 = strlen(s1);
    const size_t len2 = strlen(s2);
    if (len1 + len2 + 1 > bufferSize || buffer == NULL)
      return NULL;

    memcpy(buffer, s1, len1);
    memcpy(buffer + len1, s2, len2 + 1); // +1 to copy the null-terminator
    return buffer;
}






/* drm */

/* false = failed | true = success */
bool setup_drm(void){


  drm_fd = open("/dev/dri/card0", O_RDWR | O_NONBLOCK);
  if (drm_fd < 0){
    perror("/dev/dri/card0 error");
    return false;
  }

  /* get drm resources */

  drmModeRes *resources = drmModeGetResources(drm_fd);
        if (resources == NULL) {
                perror("drmModeGetResources error");
                return false;
        }
  if (resources->count_connectors == 0){
    puts("no connectors!");
    return false;
  }

  if (resources->count_connectors > 1){
    puts("more connectors than expected! programm will only try to use the first one.");
  }


  /* get connector */

  //struct connector *conn = NULL;
  drm_conn = drmModeGetConnector(drm_fd, resources->connectors[0]);
  if (drm_conn == NULL){
    puts("error: connector was NULL!");
    goto drm_resources_cleanup;
  }

  if (drm_conn->count_modes == 0){
    puts("error: no valid display modes");
    goto drm_resources_conn_cleanup;
  }

  if (drm_conn->connection != DRM_MODE_CONNECTED){
    puts("error connector is not connected!");
    goto drm_resources_conn_cleanup;
  }

  /* check resolution */
  if (drm_conn->modes[0].hdisplay != H_RES || drm_conn->modes[0].vdisplay != V_RES){
    printf("ERROR: resulution is not what is expected.\n\tExpected: %d x %d\n\tGot     : %d x %x\n",H_RES, V_RES, drm_conn->modes[0].hdisplay, drm_conn->modes[0].vdisplay);
    goto drm_resources_conn_cleanup;
  }


  /* get crtc */
  uint32_t taken_crtcs = 0;
  crtc = find_crtc(drm_fd,resources,drm_conn,&taken_crtcs);
  if (crtc == 0){
    puts("no crtc found!");
    goto drm_resources_conn_cleanup;
  }


  /* framebuffer 0 */
  fb0.width = H_RES;
  fb0.height = V_RES;
  fb0.bpp = BYTES_PER_PIXEL * 8;

  drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &fb0);
  
  {
    uint32_t handles[4] = { fb0.handle };
    uint32_t strides[4] = { fb0.pitch };
    uint32_t offsets[4] = { 0 };
    int ret = drmModeAddFB2(drm_fd, H_RES, V_RES, DRM_FORMAT_RGB565, handles, strides, offsets, &fb0_id, 0);
    if (ret != 0){
      perror("drmModeAddFB2 failed to create failbuffer0");
      goto drm_resources_conn_fb0_dumb_cleanup;
    }
  }

 /* prepare mapping fb0*/

  {
    struct drm_mode_map_dumb map = { .handle = fb0.handle };
    int ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
    if (ret < 0) {
      perror("DRM_IOCTL_MODE_MAP_DUMB failed to preapare map0");
      goto drm_resources_conn_fb0_cleanup;
    }

    fb0_data = mmap(0, fb0.size, PROT_READ | PROT_WRITE, MAP_SHARED,
            drm_fd, map.offset);
    if (fb0_data == 0) {
      perror("mmap0 failed");
      goto drm_resources_conn_fb0_cleanup;
    }

    memset(fb0_data, 0xff, fb0.size);
  }

  /* framebuffer 1 */
  fb1.width = H_RES;
  fb1.height = V_RES;
  fb1.bpp = BYTES_PER_PIXEL * 8;

  drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &fb1);
  {
    uint32_t handles[4] = { fb1.handle };
    uint32_t strides[4] = { fb1.pitch };
    uint32_t offsets[4] = { 0 };
    int ret = drmModeAddFB2(drm_fd, H_RES, V_RES, DRM_FORMAT_RGB565, handles, strides, offsets, &fb1_id, 0);
    if (ret != 0){
      perror("drmModeAddFB2 failed to create failbuffer1");
      goto drm_resources_conn_fb0_fb1_dumb_cleanup;
    }
  }

  /* prepare mapping fb1*/
  
  {
    struct drm_mode_map_dumb map = { .handle = fb1.handle };
    int ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
    if (ret < 0) {
      perror("DRM_IOCTL_MODE_MAP_DUMB failed to preapare map1");
      goto drm_resources_conn_fb0_fb1_cleanup;
    }

    fb1_data = mmap(0, fb1.size, PROT_READ | PROT_WRITE, MAP_SHARED,
      drm_fd, map.offset);
    if (fb1_data == 0) {
      perror("mmap1 failed");
      goto drm_resources_conn_fb0_fb1_cleanup;
    }

    memset(fb1_data, 0xff, fb1.size);
  }

  /* front / back set */
  front_fb_data = fb1_data;
  back_fb_data = fb0_data;
  front_fb = fb1_id;
  back_fb = fb0_id;

  /* save old connection and perfom the modeset*/
  saved = drmModeGetCrtc(drm_fd, crtc);

  int ret = drmModeSetCrtc(drm_fd, crtc, fb1_id, 0, 0, &drm_conn->connector_id, 1, &drm_conn->modes[0]);
  if (ret < 0) {
    perror("drmModeSetCrtc could net set the mode");
  }


  /* clean the mode resources */
  drmModeFreeResources(resources);

  /* polling for Flip handler*/

    if(pthread_create( &pollDrmThread, NULL, poll_drm_thread,NULL) != 0){
      cleanup_drm();
      return false; 
    }

  /* done, now clean and return */

  return true;

  /**** cleanup only reachable with goto: *******/

drm_resources_conn_fb0_fb1_cleanup:
  drmModeRmFB(drm_fd, fb1_id);
drm_resources_conn_fb0_fb1_dumb_cleanup:
  {  
    struct drm_mode_destroy_dumb destroy = { .handle = fb1.handle };
    drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
  }
drm_resources_conn_fb0_cleanup:
  drmModeRmFB(drm_fd, fb0_id);
drm_resources_conn_fb0_dumb_cleanup:
  {
    struct drm_mode_destroy_dumb destroy = { .handle = fb0.handle };
    drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
  }
drm_resources_conn_cleanup:
  drmModeFreeConnector(drm_conn);
drm_resources_cleanup:
  drmModeFreeResources(resources);

  return false;
}

void cleanup_drm(void){

  /* polling */
  pthread_join(pollDrmThread,NULL);

  /* fb0 */

  if (fb0_data != 0){
    munmap(fb0_data, fb0.size); fb0_data = NULL;
    drmModeRmFB(drm_fd, fb0_id);
    struct drm_mode_destroy_dumb destroy = { .handle = fb0.handle };
    drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    puts("\tcleaned up fb0");
  }

  /* fb1 */

  if (fb1_data != 0){
    munmap(fb1_data, fb1.size); fb1_data = NULL;
    drmModeRmFB(drm_fd, fb1_id);
    struct drm_mode_destroy_dumb destroy = { .handle = fb1.handle };
    drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    puts("\tcleaned up fb1");
  }

  /* restore saved mode */

  if (saved != NULL){
    drmModeSetCrtc(drm_fd, saved->crtc_id, saved->buffer_id,saved->x, saved->y, &drm_conn->connector_id, 1, &saved->mode);
    drmModeFreeCrtc(saved); saved = NULL;
    puts("\trestored saved mode");
  }

  /* connector */

  if (drm_conn != NULL){
    drmModeFreeConnector(drm_conn); drm_conn = NULL;
    puts("\tFreed connector");
  }

}


/* borrowed code from: https://github.com/ascent12/drm_doc/blob/master/02_modesetting/src/main.c#L46
 see LICENSE_DRM_DOC
*/

static uint32_t find_crtc(int drm_fd, drmModeRes *res, drmModeConnector *conn,
                uint32_t *taken_crtcs)
{
        for (int i = 0; i < conn->count_encoders; ++i) {
                drmModeEncoder *enc = drmModeGetEncoder(drm_fd, conn->encoders[i]);
                if (!enc)
                        continue;

                for (int i = 0; i < res->count_crtcs; ++i) {
                        uint32_t bit = 1 << i;
                        // Not compatible
                        if ((enc->possible_crtcs & bit) == 0)
                                continue;

                        // Already taken
                        if (*taken_crtcs & bit)
                                continue;

                        drmModeFreeEncoder(enc);
                        *taken_crtcs |= bit;
                        return res->crtcs[i];
                }

                drmModeFreeEncoder(enc);
        }

        return 0;
}


static void *poll_drm_thread(void *data){

  struct pollfd pollfd = {
		.fd = drm_fd,
		.events = POLLIN,
	};

  while(noStop){
    int ret = poll(&pollfd, 1, 250);
    if (ret < 0 && errno != EAGAIN) {
      perror("poll");
    }

    if (pollfd.revents & POLLIN) {
      drmEventContext context = {
        .version = DRM_EVENT_CONTEXT_VERSION,
        .page_flip_handler = page_flip_handler,
      };

      if (drmHandleEvent(drm_fd, &context) < 0) {
        perror("drmHandleEvent");
      }
    }
  }
}


