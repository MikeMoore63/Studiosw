#include <pebble.h>
#include "yachtimermodel.h"

// Removed for version 2.0 SDK
// #define MY_UUID { 0x89, 0xB4, 0x4E, 0x72, 0x1C, 0x63, 0x4D, 0xF5, 0xB1, 0x0E, 0x66, 0x13, 0xC5, 0x71, 0x56, 0xFB }
// PBL_APP_INFO(MY_UUID,
//             "Studio StopWatch", "Mike Moore",
//             1, 4, /* App version */
//             DEFAULT_MENU_ICON,
//             APP_INFO_STANDARD_APP);
// 

Window *window;

YachtTimer *myYachtTimer;
int startappmode=WATCHMODE;
int modetick=0;

#define BUTTON_LAP BUTTON_ID_DOWN
#define BUTTON_RUN BUTTON_ID_SELECT
#define BUTTON_RESET BUTTON_ID_UP
#define TIMER_UPDATE 1
#define MODES 5 // Number of watch types stopwatch, coutdown, yachttimer, watch
#define TICKREMOVE 5
#define CNTDWNCFG 99
#define MAX_TIME  (ASECOND * 60 * 60 * 24)

int ticks=0;


BitmapLayer  *modeImages[MODES];
GBitmap  *images[MODES];

struct modresource {
        int mode;
        int resourceid;
} mapModeImage[MODES] = {
           { WATCHMODE, RESOURCE_ID_IMAGE_WATCH },
           { STOPWATCH, RESOURCE_ID_IMAGE_STOPWATCH },
           { YACHTIMER, RESOURCE_ID_IMAGE_YACHTTIMER },
           { COUNTDOWN, RESOURCE_ID_IMAGE_COUNTDOWN },
           { CNTDWNCFG, RESOURCE_ID_IMAGE_CNTDWNCFG },
};

// The documentation claims this is defined, but it is not.
// Define it here for now.
#ifndef APP_TIMER_INVALID_HANDLE
    #define APP_TIMER_INVALID_HANDLE NULL
#endif

// Actually keeping track of time
static AppTimer *update_timer = NULL;
static int ticklen=0;

void toggle_stopwatch_handler(ClickRecognizerRef recognizer, void *data);
void toggle_mode(ClickRecognizerRef recognizer, void *data);
void reset_stopwatch_handler(ClickRecognizerRef recognizer, void *data);
void stop_stopwatch();
void start_stopwatch();
void config_provider( void *context);
// Hook to ticks
void handle_timer(void *data);
void config_watch(int appmode,int increment);

// Custom vibration pattern
const VibePattern start_pattern = {
  .durations = (uint32_t []) {100, 300, 300, 300, 100, 300},
  .num_segments = 6
};
void update_hand_positions();

#define HOUR_VIBRATION false
#define US_DATE false
#define DISPLAY_SECONDS true
#define DISPLAY_DAY true
#define DISPLAY_DATE true
#define USE_SCOREBOARD false


Layer *minute_layer;
Layer *second_layer;
TextLayer *time_display_layer;
TextLayer *date_display_layer = NULL;
TextLayer *day_display_layer;
TextLayer *ampm_display_layer;
GFont time_font;
GFont date_day_font;
int firstrun = STOPWATCH;  // Note not watch
GColor minuteColor,secondColor,fontColor;



void config_watch(int appmode,int increment)
{
    int adjustnum = 0;

    // even if running allow minute changes
    switch(mapModeImage[modetick].mode)
        {
        // Ok so we want to lower countdown config
        // Down in increments of 1 minute
        case CNTDWNCFG:
                adjustnum=ASECOND * 60;
                break;

        }


        /* for non adjust appmodes does nothing as adjustnum is 0 */
        time_t new_time=0;

        /* if running adjust running time otherwise adjust config time */
        if(yachtimer_isRunning(myYachtTimer))
        {
                new_time =  yachtimer_getElapsed(myYachtTimer) + (increment * adjustnum );
                if(new_time > MAX_TIME) new_time = yachtimer_getElapsed(myYachtTimer);
                yachtimer_setElapsed(myYachtTimer, new_time);
        }
        else
        {
                new_time =  yachtimer_getConfigTime(myYachtTimer) + (increment * adjustnum );
                // Cannot sert below 0
                // Can set above max display time
                // so keep it displayable
                if(new_time > MAX_TIME) new_time = MAX_TIME;
                yachtimer_setConfigTime(myYachtTimer, new_time);

        }


}

void start_stopwatch() {
    yachtimer_start(myYachtTimer);

    // default start mode
    startappmode = yachtimer_getMode(myYachtTimer);;

    // Up the resolution to do deciseconds
    if(update_timer != NULL) {
        app_timer_cancel( update_timer);
        update_timer = NULL;
    }
    // Slow update down to once a second to save power
    ticklen = yachtimer_getTick(myYachtTimer);
    static uint32_t cookie = TIMER_UPDATE;
    update_timer = app_timer_register( 1000, handle_timer,  &cookie);
    firstrun=-3;

}
// Toggle stopwatch timer mode
void toggle_mode(ClickRecognizerRef recognizer, void *data) {

          // Can only set to first three
	  modetick = (modetick == MODES) ?0:(modetick+1);
          yachtimer_setMode(myYachtTimer,mapModeImage[modetick].mode);

          // if beyond end mode set back to start
          update_hand_positions();
            // Up the resolution to do deciseconds
            if(update_timer != NULL) {
                app_timer_cancel( update_timer);
                update_timer = NULL;
            }

          for (int i=0;i<MODES;i++)
          {
                layer_set_hidden( (Layer *)modeImages[i], ((mapModeImage[modetick].mode == mapModeImage[i].mode)?false:true));
          }
          ticks = 0;

            ticklen = yachtimer_getTick(myYachtTimer);
	    static uint32_t cookie = TIMER_UPDATE;
            update_timer = app_timer_register( 1000, handle_timer,  &cookie);
}

void stop_stopwatch() {

    yachtimer_stop(myYachtTimer);
    if(update_timer != NULL) {
        app_timer_cancel( update_timer);
        update_timer = NULL;
    }
    // Slow update down to once a second to save power
    ticklen = yachtimer_getTick(myYachtTimer);
    static uint32_t cookie = TIMER_UPDATE;
    update_timer = app_timer_register( 1000, handle_timer,  &cookie);
}
void toggle_stopwatch_handler(ClickRecognizerRef recognizer, void *data) {
    switch(mapModeImage[modetick].mode)
    {
	case YACHTIMER:
	case STOPWATCH:
	case COUNTDOWN:
	    if(yachtimer_isRunning(myYachtTimer)) {
		stop_stopwatch();
	    } else {
		start_stopwatch();
	    }
	    break;
	default:
            config_watch(mapModeImage[modetick].mode,-1);
    }
}
void reset_stopwatch_handler(ClickRecognizerRef recognizer, void *data) {


    switch(mapModeImage[modetick].mode)
    {
        case STOPWATCH:
        case YACHTIMER:
        case COUNTDOWN:
    	    yachtimer_reset(myYachtTimer);
            if(yachtimer_isRunning(myYachtTimer))
            {
                 stop_stopwatch();
                 start_stopwatch();
            }
            else
            {
                stop_stopwatch();
            }

            break;
        default:
            ;
            // if not in config mode won't do anything which makes this easy
            config_watch(mapModeImage[modetick].mode,1);
    }
    // Force redisplay
    update_hand_positions();
}
//Creates the 12 dots representing the hour
void minute_layer_create(Layer *me, GContext* ctx) {
	int32_t minute_angle;
	int minute_position_from_center = 69;
	int i;
	GPoint minute_dot_position;
        GRect lframe = layer_get_frame(me);	
	GPoint center = grect_center_point(&lframe);
	graphics_context_set_fill_color(ctx, minuteColor);

	for(i = 0; i < 12; i++) {
		minute_angle = i * (0xffff/12);
		minute_dot_position = GPoint(center.x + minute_position_from_center * sin_lookup(minute_angle)/0xffff,
                                center.y + (-minute_position_from_center) * cos_lookup(minute_angle)/0xffff);
		graphics_fill_circle(ctx, minute_dot_position, 2);
	}
}

//Updates the seconds when called to the number of seconds at the moment
void second_layer_update(Layer *me, GContext* ctx) {
	 
	struct tm  *t;
	t=yachtimer_getPblDisplayTime(myYachtTimer);	

        int32_t second_angle;
        int second_position_from_center = 62;
        int i;
        GPoint second_dot_position;
	GRect lframe = layer_get_frame(me);
        GPoint center = grect_center_point(&lframe);
        graphics_context_set_fill_color(ctx, secondColor);

	for(i=0; i < 60; i++)
	{
		if(i == t->tm_sec+1)
		{
        		graphics_context_set_fill_color(ctx, GColorBlack);
		}
                second_angle = i * (0xffff/60);
		second_dot_position = GPoint(center.x + second_position_from_center * sin_lookup(second_angle)/0xffff,
                               center.y + (-second_position_from_center) * cos_lookup(second_angle)/0xffff);
        	graphics_fill_circle(ctx, second_dot_position, 2);
        }
}

#if HOUR_VIBRATION
const VibePattern hour_pattern = {
        .durations = (uint32_t []) {300, 100, 300, 100, 300},
        .num_segments = 5
};
#endif

void handle_init() {

  window = window_create();
  minuteColor=GColorWhite;
  secondColor=GColorWhite;
  fontColor=GColorWhite;

#ifdef PBL_COLOR
  minuteColor=GColorGreen;
  secondColor=GColorRed;
  fontColor=GColorGreen;
#endif

#ifdef PBL_SDK_3
#else
  window_set_fullscreen(window, true);
#endif
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  // Arrange for user input.
  window_set_click_config_provider(window,  config_provider);

  //Define Fonts
  #if USE_SCOREBOARD
  time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SCOREBOARD_35));
  #else
  time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITAL_SEVEN_43));
  #endif
  date_day_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITAL_SEVEN_16));

  //Time Display Layer
  if (!clock_is_24h_style()){
	#if USE_SCOREBOARD
        time_display_layer = text_layer_create(GRect(19, 60, 103, 40)); //For Scoreboard (12H Mode)
	#else
	time_display_layer = text_layer_create(GRect(30, 56, 74, 45)); //For Digital 7 (12H Mode)
	#endif
  }
  else {
	#if USE_SCOREBOARD
        time_display_layer = text_layer_create(GRect(19, 60, 103, 40)); //For Scoreboard (24H Mode)
	#else
        time_display_layer = text_layer_create(GRect(26, 56, 92, 45)); //For Digital 7 (24H Mode)
	#endif
  }
  text_layer_set_text_color(time_display_layer, fontColor);
  text_layer_set_background_color(time_display_layer, GColorClear);
  text_layer_set_font(time_display_layer, time_font);
  text_layer_set_text_alignment(time_display_layer, GTextAlignmentCenter);
  layer_add_child((Layer *)window, (Layer *)time_display_layer);

  //Date_Display Layer
  // #if DISPLAY_DATE
  date_display_layer = text_layer_create(GRect(31, 42, 82, 20));
  text_layer_set_text_color(date_display_layer, fontColor);
  text_layer_set_background_color(date_display_layer, GColorClear);
  text_layer_set_font(date_display_layer, date_day_font);
  text_layer_set_text_alignment(date_display_layer, GTextAlignmentCenter);
  layer_add_child((Layer *)window, (Layer *)date_display_layer);
  // #endif

  //Day_Display Layer
  // #if DISPLAY_DAY removed to ensure memory management is consistent simplifies testing and reduces bugs
  day_display_layer = text_layer_create(GRect(31, 106, 82, 20));
  text_layer_set_text_color(day_display_layer, fontColor);
  text_layer_set_background_color(day_display_layer, GColorClear);
  text_layer_set_font(day_display_layer, date_day_font);
  text_layer_set_text_alignment(day_display_layer, GTextAlignmentCenter);
  layer_add_child((Layer *)window, (Layer *)day_display_layer);
  // #endif

  //AM PM Layer
  // if (!clock_is_24h_style()){
  #if USE_SCOREBOARD
  ampm_display_layer = text_layer_create(GRect(64, 121, 20, 18));
  #else
  ampm_display_layer = text_layer_create(GRect(107, 83, 20, 18));
  #endif
  text_layer_set_text_color(ampm_display_layer, fontColor);
  text_layer_set_background_color(ampm_display_layer, GColorClear);
  text_layer_set_font(ampm_display_layer, date_day_font);
  layer_add_child((Layer *)window, (Layer *)ampm_display_layer);  
  // }

  //Minute Layer
  minute_layer = layer_create(layer_get_frame( (Layer *)window));
  layer_set_update_proc(minute_layer,minute_layer_create);
  // minute_layer.update_proc = minute_layer_create;
  layer_add_child((Layer *)window, (Layer *)minute_layer);

  //Second Layer
  second_layer = layer_create(layer_get_frame((Layer *)window));
  layer_set_update_proc(second_layer, second_layer_update);
  // second_layer.update_proc = &second_layer_update;
  layer_add_child((Layer *)window, (Layer*)second_layer);

  // Mode icons
  for (int i=0;i<MODES;i++)
  {
	images[i] = gbitmap_create_with_resource( mapModeImage[i].resourceid);
	modeImages[i] = bitmap_layer_create(gbitmap_get_bounds(images[i]));
        bitmap_layer_set_bitmap(modeImages[i],images[i]);
        layer_set_frame((Layer *)modeImages[i], GRect(0,0,13,23));
        // layer_set_frame(&modeImages[i].layer.layer, GRect((144 - 12)/2,((144 - 16)/2)+ 25,12,16));
        layer_set_hidden( (Layer *)modeImages[i], true);
        layer_add_child((Layer *)window,(Layer *)modeImages[i]);
  }
  ticks = 0;
  myYachtTimer = yachtimer_create(mapModeImage[modetick].mode);
  for (int i=0;i<MODES;i++)
  {
	if(yachtimer_getMode(myYachtTimer) == mapModeImage[i].mode)
	{
		modetick = i;
		break;
	}
  }
  yachtimer_tick(myYachtTimer,0);
  // Slow update down to once a second to save power
  ticklen = yachtimer_getTick(myYachtTimer);
  static uint32_t cookie = TIMER_UPDATE;
  update_timer = app_timer_register( 1000, handle_timer,  &cookie);
}

void handle_timer(void  *data ) {
   uint32_t cookie = *(uint32_t *)data;

   if(cookie == TIMER_UPDATE)
   { 
	  yachtimer_tick(myYachtTimer,ASECOND);
	  ticklen = yachtimer_getTick(myYachtTimer);
	  update_timer = app_timer_register( 1000, handle_timer,  data);
          ticks++;
          if(ticks >= TICKREMOVE)
          {
                for(int i=0;i<MODES;i++)
                {
                        layer_set_hidden( (Layer *)modeImages[i], true);
                }
          }
	  theTimeEventType event = yachtimer_triggerEvent(myYachtTimer);

  	if(event == MinorTime) vibes_double_pulse();
  	if(event == MajorTime) vibes_enqueue_custom_pattern(start_pattern);
	  update_hand_positions();
   }

}
void update_hand_positions()
{

	  struct tm *pebble_time;
	  pebble_time=yachtimer_getPblDisplayTime(myYachtTimer);	
	  
	  //Update Second Display every second 
	  #if DISPLAY_SECONDS
	  layer_mark_dirty(second_layer);
	  #endif  
          int modenow = yachtimer_getMode(myYachtTimer);

	  // Update Every Minute

		static char time_text[] = "00:00"; // Need to be static because they're used by the system later.
		char *time_format;

		// Hour & Minute Formatting Type
		if (clock_is_24h_style() || yachtimer_getMode(myYachtTimer)!=WATCHMODE) {
		time_format = "%R";
		} else {
		time_format = "%I:%M";
		}

		// Hour & Minute Formatting and Update
		strftime(time_text, sizeof(time_text), time_format, pebble_time);

		// Kludge to handle lack of non-padded hour format string
		// for twelve hour clock.
		if (!clock_is_24h_style() && (time_text[0] == '0')) {
		memmove(time_text, &time_text[1], sizeof(time_text) - 1);
		}

		text_layer_set_text(time_display_layer, time_text);

	  // Update on the hour every hour
		if (!clock_is_24h_style() && yachtimer_getMode(myYachtTimer)==WATCHMODE) {
			static char ampm_text[] = "XX";
			strftime(ampm_text, sizeof(ampm_text), "%p", pebble_time);
			text_layer_set_text(ampm_display_layer, ampm_text);
		}
		else
		{
			text_layer_set_text(ampm_display_layer, "");
		}

		// Set day name text
		if(modenow == WATCHMODE)
		{
			static char day_text[] = "Xxxxxxxxx";
			#if DISPLAY_DAY
			strftime(day_text, sizeof(day_text), "%A", yachtimer_getPblLastTime(myYachtTimer));
			text_layer_set_text(day_display_layer, day_text);
			#endif
		}

		// Set date text
		static char date_text[] = "00/00/0000";
		#if DISPLAY_DATE
		#if US_DATE
		strftime(date_text, sizeof(date_text), "%m/%d/%Y", yachtimer_getPblLastTime(myYachtTimer));
		#else
		strftime(date_text, sizeof(date_text), "%d/%m/%Y", yachtimer_getPblLastTime(myYachtTimer));
		#endif
		text_layer_set_text(date_display_layer, date_text);
		#endif

	  // Vibrate Every Hour
	  #if HOUR_VIBRATION
	  if (pebble_time->tm_min == 0 && pebble_time->tm_sec == 0 ){
		vibes_enqueue_custom_pattern(hour_pattern);
	  }
	  #endif

	  //Set firstrun to 0
	  if (firstrun != modenow && ticks >= TICKREMOVE) {
		firstrun = modenow;
	  }
}
void config_provider( void *context) {
    window_single_click_subscribe(BUTTON_RUN,toggle_stopwatch_handler);
    // config[BUTTON_RUN]->click.handler = (ClickHandler)toggle_stopwatch_handler;
    window_long_click_subscribe(BUTTON_LAP, 1000,  toggle_mode, NULL);
    // config[BUTTON_LAP]->long_click.handler = (ClickHandler) toggle_mode;
    window_single_click_subscribe(BUTTON_RESET, reset_stopwatch_handler);
    // config[BUTTON_RESET]->click.handler = (ClickHandler)reset_stopwatch_handler;
//    config[BUTTON_LAP]->click.handler = (ClickHandler)lap_time_handler;
//    config[BUTTON_LAP]->long_click.handler = (ClickHandler)handle_display_lap_times;
//    config[BUTTON_LAP]->long_click.delay_ms = 700;
}
  
void handle_deinit() {

  for(int i=0;i<MODES;i++)
  {
        bitmap_layer_destroy(modeImages[i]);
        gbitmap_destroy(images[i]);
  }
  text_layer_destroy(time_display_layer);
  text_layer_destroy(date_display_layer);
  text_layer_destroy(ampm_display_layer);

  fonts_unload_custom_font(time_font);
  fonts_unload_custom_font(date_day_font);
  layer_destroy(minute_layer);
  layer_destroy(second_layer);
  window_destroy(window);
  yachtimer_destroy(myYachtTimer);
}

int main(void) {
        handle_init();
        app_event_loop();
        handle_deinit();
}
/*
void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
     .timer_handler = &handle_timer
  };
  app_event_loop(params, &handlers);
}
*/
