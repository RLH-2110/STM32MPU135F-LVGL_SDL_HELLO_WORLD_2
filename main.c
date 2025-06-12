#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/sdl/lv_sdl_mouse.h"

#define LV_FONT_MONTSERRAT_24 1


#define H_RES (480)
#define V_RES (272)

#define LVGL_DESIRED_TICK_INCREASE_MS 1

#define NANOSECOND_TO_MILISECOND_RATE 1000000
#define MICROSECOND_TO_MILISECOND_RATE 1000

extern const lv_font_t lv_font_montserrat_24;
extern const lv_font_t deFontMontserrat14;
extern const lv_image_dsc_t examplePersonImg;

/* lvgl stuff and threats */
static lv_display_t *disp;
static lv_indev_t *lvMouse;

static pthread_t tickThread;
static void* tick_thread(void* data);
volatile sig_atomic_t noStop = 1; /* programms runs as long as this is set */

static pthread_t fakeLoadrThread;
static void* load_main_screen_thread(void* data);

/* screens */
void fake_loading_screen(void);
void main_screen(void);

/* callbacks */
void tab_view_value_changed_cb(lv_event_t *e);
void profile_img_click_cb(lv_event_t *e);
void feedback_send_button_click_cb(lv_event_t *e);

/* structs */

typedef struct two_pointers{ /* for callbacks, if we need to pass 2 thigns */
  void *p1;
  void *p2;
}two_pointers;
/* defines for specific uses of this struct */
#define TWO_PTR_INDEX p1
#define TWO_PTR_LV_OBJ_T p2

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

/* wraps rand(), to avoid divisions by 0 */
unsigned int get_rand(unsigned int max_value_exclusive){
  if (max_value_exclusive == 0)
    return 0;
  return rand() % max_value_exclusive;
}

/* signal handler */
void on_termination(int signal){
  noStop = 0;
}

/* scratch */

/* macro functions */

#define OBJ_POS_HALF_LEFT(obj)  (lv_obj_get_x(obj) - lv_obj_get_width(obj) / 2) /* gets position of the object if it were moved left by half the objects size */
#define OBJ_POS_HALF_RIGHT(obj) (lv_obj_get_x(obj) + lv_obj_get_width(obj) / 2)

#define OBJ_POS_FULL_RIGHT(obj) (lv_obj_get_x(obj) + lv_obj_get_width(obj)) /* gets position of the objecz, if it were moved right by the size of the object */
#define OBJ_POS_FULL_LEFT(obj) (lv_obj_get_x(obj) - lv_obj_get_width(obj))
#define OBJ_POS_FULL_UP(obj) (lv_obj_get_y(obj) - lv_obj_get_height(obj))
#define OBJ_POS_FULL_DOWN(obj) (lv_obj_get_y(obj) + lv_obj_get_height(obj))


int main(){
  srand(time(NULL));

  lv_init();

  signal(SIGINT,on_termination);
  signal(SIGTERM,on_termination);
  if (pthread_create( &tickThread, NULL, tick_thread,NULL) != 0){
    puts("Error: could not create tick thread");
    return 1;
  }

  disp = lv_sdl_window_create(H_RES, V_RES);
  if (disp == NULL){
    puts("Display error!");
    return 1;
  }
  lvMouse = lv_sdl_mouse_create();

  LV_IMAGE_DECLARE(examplePersonImg);  

  /* gui stuff here */

  lv_style_init(&germanFontStyle);
  lv_style_set_text_font(&germanFontStyle, &deFontMontserrat14);

  fake_loading_screen();

  /* automatically call the main screen after a few seconds after the loading screen happens*/
  if(pthread_create( &fakeLoadrThread, NULL, load_main_screen_thread,NULL) != 0){
    main_screen(); /* just go to the main screen if we fail */
  }

  /* end gui stuff*/
  
  int32_t chartInertia = 0;
  int32_t chartCurrPoint = CHART_START_VALUE;
  int i = 0;
  while(noStop){
    if (chart != NULL){
	if (time(NULL) > nextUpdateTarget){
          int32_t newPoint = chart_update(chartCurrPoint, chartInertia);
          chartInertia = (newPoint - chartCurrPoint) / CHART_INTERTIA;
          chartCurrPoint = newPoint;
          lv_chart_set_next_value(chart,chartSeries,chartCurrPoint);
 
          nextUpdateTarget += CHART_UPDATE_RATE_S;
          printf("new point %5d | new target: %d\n",chartCurrPoint,nextUpdateTarget);
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
  lv_sdl_quit();
  puts("quit sdl");
  lv_deinit();
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

    static lv_obj_t *chartScale; 
    /* might needs to be reworked when multiple screens exist */
    if (chart == NULL){
      chart = lv_chart_create(tabHome);
      lv_obj_center(chart);
      lv_obj_set_height(chart, V_RES - 50);
      lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, CHART_MIN_VALUE, CHART_MAX_VALUE);
      lv_obj_update_layout(chart);

      chartSeries = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_DEEP_ORANGE), LV_CHART_AXIS_PRIMARY_Y);
      nextUpdateTarget = time(NULL);

      printf("chart done.      time target: %d\n",nextUpdateTarget);
      
      chartScale = lv_scale_create(chart);
      lv_scale_set_mode(chartScale,LV_SCALE_MODE_VERTICAL_RIGHT);
      lv_scale_set_range(chartScale,CHART_MIN_VALUE,CHART_MAX_VALUE);
      lv_scale_set_total_tick_count(chartScale, 10);
      lv_scale_set_major_tick_every(chartScale, 5);
      lv_obj_set_size(chartScale,25, lv_obj_get_height(chart) - 20);
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

/* other functions */
static void *tick_thread(void* data) {
  (void) data;
  while(noStop) {
    usleep(LVGL_DESIRED_TICK_INCREASE_MS * MICROSECOND_TO_MILISECOND_RATE);
    lv_tick_inc(LVGL_DESIRED_TICK_INCREASE_MS); /*Tell LVGL how many time has eslaped in ms*/
  }
}

static void load_main_screen(void* data){
  main_screen();
}

static void *load_main_screen_thread(void* data) {
  usleep((FAKE_LOADING_TIME_MS + time(NULL) % FAKE_LOADING_TIME_SPEED_VARIATION_MS) * MICROSECOND_TO_MILISECOND_RATE); /* rand is not thread safe. and its not important enough for me to learn and use a good alternative just for this */
  lv_async_call(load_main_screen,NULL);
}

