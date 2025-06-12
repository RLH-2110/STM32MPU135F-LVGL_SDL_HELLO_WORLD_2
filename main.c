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

/* signal handler */
void on_termination(int signal){
  noStop = 0;
}

/* scratch */

/* macro functions */

#define OBJ_POS_HALF_LEFT(obj)  (lv_obj_get_x(obj) - lv_obj_get_width(obj) / 2) /* gets position of the object if it was moved left by half the objects size */
#define OBJ_POS_HALF_RIGHT(obj) (lv_obj_get_x(obj) + lv_obj_get_width(obj) / 2)

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


  int i = 0;
  while(noStop){
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

/* screens */

void fake_loading_screen(void)
{
  static lv_obj_t *screen; screen = lv_obj_create(NULL);
  lv_obj_add_style(screen, &germanFontStyle, 0);

  lv_obj_t * spinner = lv_spinner_create(screen);
  lv_obj_set_size(spinner, 100, 100);
  lv_spinner_set_anim_params(spinner,FAKE_LOADING_TIME_SPEED_MS + rand() % FAKE_LOADING_TIME_SPEED_VARIATION_MS ,250); /* 250 is just a random angle. 0 looks ugly*/
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

  lv_obj_t * tab_buttons = lv_tabview_get_tab_bar(tabView);
  lv_obj_set_style_bg_color(tab_buttons, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
  lv_obj_set_style_text_color(tab_buttons, lv_palette_lighten(LV_PALETTE_GREY, 5), 0);


  lv_obj_t *tabHome      = lv_tabview_add_tab(tabView, "Start");
  lv_obj_t *tabBio       = lv_tabview_add_tab(tabView, "Bio");
  lv_obj_t *tabFeedback  = lv_tabview_add_tab(tabView, "Feedback");
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
  

  /* bio tab */

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

  /* feedbcak tab */
    
    lv_obj_t *fbLabel = lv_label_create(tabFeedback);
    lv_label_set_text(fbLabel,"Feedback:");

    

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

