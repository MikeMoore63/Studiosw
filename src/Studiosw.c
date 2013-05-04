#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "yachtimermodel.h"


#define MY_UUID { 0x89, 0xB4, 0x4E, 0x72, 0x1C, 0x63, 0x4D, 0xF5, 0xB1, 0x0E, 0x66, 0x13, 0xC5, 0x71, 0x56, 0xFB }
PBL_APP_INFO(MY_UUID,
             "Studio StopWatch", "Mike Moore",
             1, 2, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_STANDARD_APP);

Window window;
AppContextRef app;

YachtTimer myYachtTimer;
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


BmpContainer modeImages[MODES];

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
    #define APP_TIMER_INVALID_HANDLE 0xDEADBEEF
#endif

// Actually keeping track of time
static AppTimerHandle update_timer = APP_TIMER_INVALID_HANDLE;
static int ticklen=0;

void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void toggle_mode(ClickRecognizerRef recognizer, Window *window);
void reset_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void stop_stopwatch();
void start_stopwatch();
void config_provider(ClickConfig **config, Window *window);
// Hook to ticks
void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie);
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

Window window;

Layer minute_layer;
Layer second_layer;
TextLayer time_display_layer;
TextLayer date_display_layer;
TextLayer day_display_layer;
TextLayer ampm_display_layer;
GFont time_font;
GFont date_day_font;
int firstrun = STOPWATCH;  // Note not watch

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
        if(yachtimer_isRunning(&myYachtTimer))
        {
                new_time =  yachtimer_getElapsed(&myYachtTimer) + (increment * adjustnum );
                if(new_time > MAX_TIME) new_time = yachtimer_getElapsed(&myYachtTimer);
                yachtimer_setElapsed(&myYachtTimer, new_time);
        }
        else
        {
                new_time =  yachtimer_getConfigTime(&myYachtTimer) + (increment * adjustnum );
                // Cannot sert below 0
                // Can set above max display time
                // so keep it displayable
                if(new_time > MAX_TIME) new_time = MAX_TIME;
                yachtimer_setConfigTime(&myYachtTimer, new_time);

        }


}

void start_stopwatch() {
    yachtimer_start(&myYachtTimer);

    // default start mode
    startappmode = yachtimer_getMode(&myYachtTimer);;

    // Up the resolution to do deciseconds
    if(update_timer != APP_TIMER_INVALID_HANDLE) {
        if(app_timer_cancel_event(app, update_timer)) {
            update_timer = APP_TIMER_INVALID_HANDLE;
        }
    }
    // Slow update down to once a second to save power
    ticklen = yachtimer_getTick(&myYachtTimer);
    update_timer = app_timer_send_event(app, ASECOND, TIMER_UPDATE);
    firstrun=-3;

}
// Toggle stopwatch timer mode
void toggle_mode(ClickRecognizerRef recognizer, Window *window) {

          // Can only set to first three
	  modetick = (modetick == MODES) ?0:(modetick+1);
          yachtimer_setMode(&myYachtTimer,mapModeImage[modetick].mode);

          // if beyond end mode set back to start
          update_hand_positions();
            // Up the resolution to do deciseconds
            if(update_timer != APP_TIMER_INVALID_HANDLE) {
                if(app_timer_cancel_event(app, update_timer)) {
                    update_timer = APP_TIMER_INVALID_HANDLE;
                }
            }

          for (int i=0;i<MODES;i++)
          {
                layer_set_hidden( &modeImages[i].layer.layer, ((mapModeImage[modetick].mode == mapModeImage[i].mode)?false:true));
          }
          ticks = 0;

            ticklen = yachtimer_getTick(&myYachtTimer);
            update_timer = app_timer_send_event(app, ASECOND, TIMER_UPDATE);
}

void stop_stopwatch() {

    yachtimer_stop(&myYachtTimer);
    if(update_timer != APP_TIMER_INVALID_HANDLE) {
        if(app_timer_cancel_event(app, update_timer)) {
            update_timer = APP_TIMER_INVALID_HANDLE;
        }
    }
    // Slow update down to once a second to save power
    ticklen = yachtimer_getTick(&myYachtTimer);
    update_timer = app_timer_send_event(app, ASECOND, TIMER_UPDATE);
}
void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window) {
    switch(mapModeImage[modetick].mode)
    {
	case YACHTIMER:
	case STOPWATCH:
	case COUNTDOWN:
	    if(yachtimer_isRunning(&myYachtTimer)) {
		stop_stopwatch();
	    } else {
		start_stopwatch();
	    }
	    break;
	default:
            config_watch(mapModeImage[modetick].mode,-1);
    }
}
void reset_stopwatch_handler(ClickRecognizerRef recognizer, Window *window) {


    switch(mapModeImage[modetick].mode)
    {
        case STOPWATCH:
        case YACHTIMER:
        case COUNTDOWN:
    	    yachtimer_reset(&myYachtTimer);
            if(yachtimer_isRunning(&myYachtTimer))
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
	(void)me;
	int32_t minute_angle;
	int minute_position_from_center = 69;
	int i;
	GPoint minute_dot_position;
	
	GPoint center = grect_center_point(&me->frame);
	graphics_context_set_fill_color(ctx, GColorWhite);

	for(i = 0; i < 12; i++) {
		minute_angle = i * (0xffff/12);
		minute_dot_position = GPoint(center.x + minute_position_from_center * sin_lookup(minute_angle)/0xffff,
                                center.y + (-minute_position_from_center) * cos_lookup(minute_angle)/0xffff);
		graphics_fill_circle(ctx, minute_dot_position, 2);
	}
}

//Updates the seconds when called to the number of seconds at the moment
void second_layer_update(Layer *me, GContext* ctx) {
	(void)me;
	 
	PblTm *t;
	t=yachtimer_getPblDisplayTime(&myYachtTimer);	

        int32_t second_angle;
        int second_position_from_center = 62;
        int i;
        GPoint second_dot_position;

        GPoint center = grect_center_point(&me->frame);
        graphics_context_set_fill_color(ctx, GColorWhite);

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

void handle_init(AppContextRef ctx) {
  app=ctx;

  window_init(&window, "StudioClock");
  window_stack_push(&window, true /* Animated */);
  window_set_fullscreen(&window, true);
  window_set_background_color(&window, GColorBlack);

  resource_init_current_app(&APP_RESOURCES);
  // Arrange for user input.
  window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);

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
        text_layer_init(&time_display_layer, GRect(19, 60, 103, 40)); //For Scoreboard (12H Mode)
	#else
	text_layer_init(&time_display_layer, GRect(30, 56, 74, 45)); //For Digital 7 (12H Mode)
	#endif
  }
  else {
	#if USE_SCOREBOARD
        text_layer_init(&time_display_layer, GRect(19, 60, 103, 40)); //For Scoreboard (24H Mode)
	#else
	text_layer_init(&time_display_layer, GRect(26, 56, 92, 45)); //For Digital 7 (24H Mode)
	#endif
  }
  text_layer_set_text_color(&time_display_layer, GColorWhite);
  text_layer_set_background_color(&time_display_layer, GColorClear);
  text_layer_set_font(&time_display_layer, time_font);
  text_layer_set_text_alignment(&time_display_layer, GTextAlignmentCenter);
  layer_add_child(&window.layer, &time_display_layer.layer);

  //Date_Display Layer
  #if DISPLAY_DATE
  text_layer_init(&date_display_layer, GRect(31, 42, 82, 20));
  text_layer_set_text_color(&date_display_layer, GColorWhite);
  text_layer_set_background_color(&date_display_layer, GColorClear);
  text_layer_set_font(&date_display_layer, date_day_font);
  text_layer_set_text_alignment(&date_display_layer, GTextAlignmentCenter);
  layer_add_child(&window.layer, &date_display_layer.layer);
  #endif

  //Day_Display Layer
  #if DISPLAY_DAY
  text_layer_init(&day_display_layer, GRect(31, 106, 82, 20));
  text_layer_set_text_color(&day_display_layer, GColorWhite);
  text_layer_set_background_color(&day_display_layer, GColorClear);
  text_layer_set_font(&day_display_layer, date_day_font);
  text_layer_set_text_alignment(&day_display_layer, GTextAlignmentCenter);
  layer_add_child(&window.layer, &day_display_layer.layer);
  #endif

  //AM PM Layer
  if (!clock_is_24h_style()){
  text_layer_init(&ampm_display_layer, window.layer.frame);
  text_layer_set_text_color(&ampm_display_layer, GColorWhite);
  text_layer_set_background_color(&ampm_display_layer, GColorClear);
  #if USE_SCOREBOARD
  layer_set_frame(&ampm_display_layer.layer, GRect(64, 121, 20, 18));
  #else
  layer_set_frame(&ampm_display_layer.layer, GRect(107, 83, 20, 18));
  #endif
  text_layer_set_font(&ampm_display_layer, date_day_font);
  text_layer_set_text_alignment(&ampm_display_layer, GTextAlignmentLeft);
  layer_add_child(&window.layer, &ampm_display_layer.layer);  
  }

  //Minute Layer
  layer_init(&minute_layer, window.layer.frame);
  minute_layer.update_proc = &minute_layer_create;
  layer_add_child(&window.layer, &minute_layer);

  //Second Layer
  layer_init(&second_layer, window.layer.frame);
  second_layer.update_proc = &second_layer_update;
  layer_add_child(&window.layer, &second_layer);

  // Mode icons
  for (int i=0;i<MODES;i++)
  {
        bmp_init_container(mapModeImage[i].resourceid,&modeImages[i]);
        layer_set_frame(&modeImages[i].layer.layer, GRect(0,0,13,23));
        // layer_set_frame(&modeImages[i].layer.layer, GRect((144 - 12)/2,((144 - 16)/2)+ 25,12,16));
        layer_set_hidden( &modeImages[i].layer.layer, true);
        layer_add_child(&window.layer,&modeImages[i].layer.layer);
  }
  ticks = 0;
  // Set up a layer for the second hand
 yachtimer_init(&myYachtTimer,mapModeImage[modetick].mode);
 yachtimer_setConfigTime(&myYachtTimer, ASECOND * 60 * 10);
 yachtimer_tick(&myYachtTimer,0);
 stop_stopwatch();
}

void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie ) {
  (void)ctx;

   if(cookie == TIMER_UPDATE)
   { 
	  yachtimer_tick(&myYachtTimer,ASECOND);
	  ticklen = yachtimer_getTick(&myYachtTimer);
	  update_timer = app_timer_send_event(ctx, ASECOND, TIMER_UPDATE);
          ticks++;
          if(ticks >= TICKREMOVE)
          {
                for(int i=0;i<MODES;i++)
                {
                        layer_set_hidden( &modeImages[i].layer.layer, true);
                }
          }
	  theTimeEventType event = yachtimer_triggerEvent(&myYachtTimer);

  	if(event == MinorTime) vibes_double_pulse();
  	if(event == MajorTime) vibes_enqueue_custom_pattern(start_pattern);
	  update_hand_positions();
   }

}
void update_hand_positions()
{

	  PblTm *pebble_time;
	  pebble_time=yachtimer_getPblDisplayTime(&myYachtTimer);	
	  
	  //Update Second Display every second 
	  #if DISPLAY_SECONDS
	  layer_mark_dirty(&second_layer);
	  #endif  
          int modenow = yachtimer_getMode(&myYachtTimer);

	  // Update Every Minute

		static char time_text[] = "00:00"; // Need to be static because they're used by the system later.
		char *time_format;

		// Hour & Minute Formatting Type
		if (clock_is_24h_style() || yachtimer_getMode(&myYachtTimer)!=WATCHMODE) {
		time_format = "%R";
		} else {
		time_format = "%I:%M";
		}

		// Hour & Minute Formatting and Update
		string_format_time(time_text, sizeof(time_text), time_format, pebble_time);

		// Kludge to handle lack of non-padded hour format string
		// for twelve hour clock.
		if (!clock_is_24h_style() && (time_text[0] == '0')) {
		memmove(time_text, &time_text[1], sizeof(time_text) - 1);
		}

		text_layer_set_text(&time_display_layer, time_text);

	  // Update on the hour every hour
		if (!clock_is_24h_style() && yachtimer_getMode(&myYachtTimer)==WATCHMODE) {
			static char ampm_text[] = "XX";
			string_format_time(ampm_text, sizeof(ampm_text), "%p", pebble_time);
			text_layer_set_text(&ampm_display_layer, ampm_text);
		}
		else
		{
			text_layer_set_text(&ampm_display_layer, "");
		}

		// Set day name text
		#if DISPLAY_DAY
		if(modenow == WATCHMODE)
		{
			static char day_text[] = "Xxxxxxxxx";
			string_format_time(day_text, sizeof(day_text), "%A", yachtimer_getPblLastTime(&myYachtTimer));
			text_layer_set_text(&day_display_layer, day_text);
		}
		#endif

		// Set date text
		#if DISPLAY_DATE
		static char date_text[] = "00/00/0000";
		#if US_DATE
		string_format_time(date_text, sizeof(date_text), "%m/%d/%Y", yachtimer_getPblLastTime(&myYachtTimer));
		#else
		string_format_time(date_text, sizeof(date_text), "%d/%m/%Y", yachtimer_getPblLastTime(&myYachtTimer));
		#endif

			text_layer_set_text(&date_display_layer, date_text);
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
void config_provider(ClickConfig **config, Window *window) {
    config[BUTTON_RUN]->click.handler = (ClickHandler)toggle_stopwatch_handler;
    config[BUTTON_LAP]->long_click.handler = (ClickHandler) toggle_mode;
    config[BUTTON_RESET]->click.handler = (ClickHandler)reset_stopwatch_handler;
//    config[BUTTON_LAP]->click.handler = (ClickHandler)lap_time_handler;
//    config[BUTTON_LAP]->long_click.handler = (ClickHandler)handle_display_lap_times;
//    config[BUTTON_LAP]->long_click.delay_ms = 700;
    (void)window;
}
  
void handle_deinit(AppContextRef ctx) {
  (void)ctx;

  for(int i=0;i<MODES;i++)
        bmp_deinit_container(&modeImages[i]);

  fonts_unload_custom_font(time_font);
  fonts_unload_custom_font(date_day_font);
}

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
     .timer_handler = &handle_timer
  };
  app_event_loop(params, &handlers);
}
