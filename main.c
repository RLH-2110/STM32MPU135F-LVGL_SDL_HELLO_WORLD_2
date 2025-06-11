#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#include "lv_drv_conf.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/sdl/lv_sdl_mouse.h"

#define H_RES (480)
#define V_RES (272)

#define NANOSECOND_TO_MILISECOND_RATE 1000000
#define MICROSECOND_TO_MILISECOND_RATE 1000

#include "font.c"


/* lvgl stuff and threats */
static lv_display_t *disp;
static lv_indev_t *lvMouse;

static pthread_t thread1;
static void* tick_thread(void* data);
volatile sig_atomic_t noStop = 1; /* programms runs as long as this is set */

/* screens */
void main_screen(void);

/* callbacks */

/* other */
lv_style_t german_font_style; /* style that includes german symbols */
  

/* signal handler */
void on_termination(int signal){
  noStop = 0;
}

/* macro functions */

#define obj_pos_half_left(obj)  (lv_obj_get_x(obj) - lv_obj_get_width(obj) / 2) /* gets position of the object if it was moved left by half the objects size */
#define obj_pos_half_right(obj) (lv_obj_get_x(obj) + lv_obj_get_width(obj) / 2)

int main(){

  lv_init();

  signal(SIGINT,on_termination);
  signal(SIGTERM,on_termination);
  pthread_create( &thread1, NULL, tick_thread,NULL);

  disp = lv_sdl_window_create(H_RES, V_RES);
  if (disp == NULL){
    puts("Display error!");
    return 1;
  }
  lvMouse = lv_sdl_mouse_create();


  /* gui stuff here */

  lv_style_init(&german_font_style);
  lv_style_set_text_font(&german_font_style, &Monsterat);

  main_screen();

  /* end gui stuff*/


  int i = 0;
  while(noStop){
    uint32_t time_till_next = lv_timer_handler();
    usleep(time_till_next*MICROSECOND_TO_MILISECOND_RATE ); 
  }

  pthread_join(thread1,NULL);
  lv_indev_delete(lvMouse);
  lv_disp_remove(disp);
  lv_sdl_quit();
  lv_deinit();

  return 0;

}

/* screens */
void main_screen(void)
{
  
  static lv_obj_t *screen; screen = lv_obj_create(NULL);

  /* hello world button */
  static lv_obj_t* hello_world_button; hello_world_button = lv_button_create(screen);

  /* hello world button label*/
  static lv_obj_t *hello_world_button_label; hello_world_button_label = lv_label_create(hello_world_button);
  lv_label_set_text(hello_world_button_label, "Test Button");
  lv_obj_add_style(hello_world_button_label, &german_font_style, 0);
  lv_obj_align(hello_world_button_label, LV_ALIGN_CENTER,0,0);



  lv_screen_load_anim(screen,LV_SCR_LOAD_ANIM_NONE,0,0,true);
}


/* callbacks */


/* other functions */
static void *tick_thread(void* data) {
  (void) data;
  while(noStop) {
    usleep(5 * 1000);
    lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
  }
}


