// TODO придумать интерфейсы и функционал
//      темная/светлая тема по времени заката/восхода или по датчику освещенности
//      шрифты: см. какие еще символы и размеры нужны (информация для пересборки в README)

// TODO связь с сервером
//      1) публикация на сервер и вывод с него (mqtt или еще какие подходящие протоколы)
//      2) веб-сервер на контроллере (можно отдельно публиковать на сервер и читать с него)

// TODO погода
//      вывод графиков
//      публикация на свой сервер (mqtt, websocket, nodejs, strapi, graphql, tarantool) (или thingspeak или другой более подходящий)
//      читать прогноз погоды (openweathermap или другой более подходящий)

// TODO таймер с сигналом

// TODO процесс загрузки
//      отображать на экране, инициализировать по порядку

#include <Arduino.h>
#include "config.h"
#include "secure.h"
#include <SPI.h>
#include <Wire.h>

#include <TFT_eSPI.h>
#include <lvgl.h>
#include <indev/XPT2046.h>
#include <WiFi.h>
#include "time.h"
#include <BME280I2C.h>

TFT_eSPI tft = TFT_eSPI();

#define DISP_HOR_RES TFT_HEIGHT
#define DISP_VER_RES TFT_WIDTH
#define MY_DISP_HOR_RES TFT_HEIGHT
#define MY_DISP_VER_RES TFT_WIDTH
static lv_disp_draw_buf_t draw_buf;
//static lv_color_t buf1[ DISP_HOR_RES * 10 ];
static lv_color_t buf1[DISP_HOR_RES * DISP_VER_RES / 10];                        /*Declare a buffer for 1/10 screen size*/

#if USE_LV_LOG != 0
/* Serial debugging */
void my_print(lv_log_level_t level, const char * file, uint32_t line, const char * dsc)
{
	Serial.printf("%s@%d->%s\r\n", file, line, dsc);
	Serial.flush();
}
#endif

WiFiClient wifiClient;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 3;
const int daylightOffset_sec = 0;

// Default : forced mode, standby time = 1000 ms
// Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,
BME280I2C bme;

struct WeatherData {
	lv_obj_t * label_temperature;
	lv_obj_t * label_humidity;
	lv_obj_t * label_pressure;
};

struct TimeData {
	lv_obj_t * label_time;
};

void connectNet(uint32_t recon_delay = 500)
{
	Serial.printf("Connecting to %s \n", SSID_NAME);
	WiFi.begin(SSID_NAME, SSID_PASS);
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(".");
		delay(recon_delay);
	}
	Serial.println(" CONNECTED");
}
void checkNetTask(lv_timer_t * task)
{
	if (WiFi.status() != WL_CONNECTED)
	{
		Serial.printf("Reconnecting to %s \n", SSID_NAME);
		WiFi.begin(SSID_NAME, SSID_PASS);
	}
}

bool photosensorOn = true;
lv_theme_t * theme;
// TODO ? можно еще фон менять в зависимости от уровня освещения
void lightTask(lv_timer_t * task)
{
	if (!photosensorOn)
	{
		return;
	}
	uint16_t photoresistorValue = analogRead(PHOTORESISTOR_PIN);

	// TODO отладить зону нечуствительности для исключения дребезга
	if (photoresistorValue < LIGHT_START_VALUE - LIGHT_OFFSET_VALUE && (!theme || (theme->flags & 1) == 0))
	{
		theme = lv_theme_default_init(
			NULL,  // Use the DPI, size, etc from this display
			lv_palette_main(LV_PALETTE_GREEN), lv_palette_main(LV_PALETTE_LIGHT_GREEN),   // Primary and secondary palette
			true,    // Light or dark mode
			&font_montserrat_16 // Small, normal, large fonts
		);
		lv_disp_set_theme(NULL, theme); // Assign the theme to the display
	}
	else if (photoresistorValue > LIGHT_START_VALUE + LIGHT_OFFSET_VALUE && (!theme || (theme->flags & 1) == 1))
	{
		theme = lv_theme_default_init(
			NULL,  // Use the DPI, size, etc from this display
			lv_palette_main(LV_PALETTE_GREEN), lv_palette_main(LV_PALETTE_LIGHT_GREEN),   //Primary and secondary palette
			false,    // Light or dark mode
			&font_montserrat_16 // Small, normal, large fonts
		);
		lv_disp_set_theme(NULL, theme); // Assign the theme to the display
	}
}

void syncTime()
{
	// init and get the time
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}
void syncTimeTask(lv_timer_t * task)
{
	syncTime();
}
tm readTime()
{
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo))
	{
		Serial.println("Failed to obtain time, Restart in 3 seconds");
		delay(3000);
		esp_restart();
		while (1);
	}
	Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

	return timeinfo;
}
void printTimeTask(lv_timer_t * task)
{
	TimeData * time_data = (TimeData *)(task->user_data);
	tm timeinfo = readTime();
	// TODO выводить полную дату?
	lv_label_set_text_fmt(time_data->label_time, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

void printWeatherTask(lv_timer_t * task)
{
	WeatherData * user_data = (WeatherData *)(task->user_data);

	float t(NAN), h(NAN), p(NAN);
	BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
	BME280::PresUnit presUnit(BME280::PresUnit_inHg);
	bme.read(p, t, h, tempUnit, presUnit);
	p = p * 25.4; // inch to mm

	lv_label_set_text_fmt(user_data->label_temperature, "Temperature: %f *C", t);
	lv_label_set_text_fmt(user_data->label_humidity, "Humidity: %f %%", h);
	lv_label_set_text_fmt(user_data->label_pressure, "Pressure: %f mmHg", p);
}

static void btn_event_cb(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	if (code == LV_EVENT_CLICKED)
	{
		static uint8_t cnt = 0;
		cnt++;

		// Get the first child of the button which is the label and change its text
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
		lv_label_set_text_fmt(label, "Ещё топчи %d", cnt);

		// TODO перенести пищалку в таймер
		digitalWrite(SPEAKER_PIN, HIGH);
		delay(180);
		digitalWrite(SPEAKER_PIN, LOW);
	}
}
static void sw_event_handler(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	if (code == LV_EVENT_VALUE_CHANGED)
	{
    lv_obj_t * obj = lv_event_get_target(e);
		photosensorOn = lv_obj_has_state(obj, LV_STATE_CHECKED);
	}
}
void my_demo()
{

	// Create simple label
	lv_obj_t * label = lv_label_create(lv_scr_act());
	//lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_montserrat_16);
	lv_label_set_text(label, "Hello Arduino! Привет мир!");
	lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

	lv_obj_t * btn = lv_btn_create(lv_scr_act()); // Add a button the current screen
	lv_obj_set_pos(btn, 10, 10); // Set its position
	lv_obj_set_size(btn, 150, 50); // Set its size
	lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, nullptr); // Assign a callback to the button

	lv_obj_t * label2 = lv_label_create(btn); // Add a label to the button
	lv_label_set_text(label2, "Топчи на кнопку"); // Set the labels text

	// Create a switch and apply the styles
	lv_obj_t *sw1 = lv_switch_create(lv_scr_act());
	lv_obj_set_pos(sw1, 10, 100);
	lv_obj_add_event_cb(sw1, sw_event_handler, LV_EVENT_VALUE_CHANGED, nullptr);
	lv_obj_add_state(sw1, LV_STATE_CHECKED);

	lv_obj_t * label_temperature = lv_label_create(lv_scr_act());
	lv_label_set_text(label_temperature, "");
	lv_obj_align(label_temperature, LV_ALIGN_CENTER, 0, 20);
	lv_obj_t * label_humidity = lv_label_create(lv_scr_act());
	lv_label_set_text(label_humidity, "");
	lv_obj_align(label_humidity, LV_ALIGN_CENTER, 0, 35);
	lv_obj_t * label_pressure = lv_label_create(lv_scr_act());
	lv_label_set_text(label_pressure, "");
	lv_obj_align(label_pressure, LV_ALIGN_CENTER, 0, 50);
	WeatherData * user_data = new WeatherData();
	user_data->label_temperature = label_temperature;
	user_data->label_humidity = label_humidity;
	user_data->label_pressure = label_pressure;
	lv_timer_t * taskWeather = lv_timer_create(printWeatherTask, 60000, user_data);
	lv_timer_ready(taskWeather);

	// monitoring net
	lv_timer_t * taskNet = lv_timer_create(checkNetTask, 30000, nullptr);
	lv_timer_ready(taskNet);

	// light
	lv_timer_t * taskLight = lv_timer_create(lightTask, 3000, nullptr);
	lv_timer_ready(taskLight);

	// sync
	lv_timer_t * taskSync = lv_timer_create(syncTimeTask, 86400000, nullptr);
	lv_timer_ready(taskSync);
	// time
	lv_obj_t * label_time = lv_label_create(lv_scr_act());
	lv_label_set_text(label_time, "");
	lv_obj_align(label_time, LV_ALIGN_CENTER, 0, 80);
	TimeData * time_data = new TimeData();
	time_data->label_time = label_time;
	lv_timer_t * taskTime = lv_timer_create(printTimeTask, 1000, time_data);
	lv_timer_ready(taskTime);
}

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
	uint32_t w = (area->x2 - area->x1 + 1);
	uint32_t h = (area->y2 - area->y1 + 1);

	tft.startWrite();
	tft.setAddrWindow(area->x1, area->y1, w, h);
	tft.pushColors(&color_p->full, w * h, true);
	tft.endWrite();

	lv_disp_flush_ready(disp);
}

/* Read the touchpad */
void my_touchpad_read(lv_indev_drv_t * indev, lv_indev_data_t * data)
{
	uint16_t touchX, touchY;

	bool touched = tft.getTouch(&touchX, &touchY, 600);

	if(!touched) {
	  data->state = LV_INDEV_STATE_RELEASED;
	} else {
	  data->state = LV_INDEV_STATE_PRESSED;

	  // Set the coordinates
	  data->point.x = touchX;
	  data->point.y = touchY;
	}
}

void setup()
{
	Serial.begin(115200);

	pinMode(SPEAKER_PIN, OUTPUT);
	digitalWrite(SPEAKER_PIN, LOW);

	lv_init();

#if USE_LV_LOG != 0
	lv_log_register_print_cb(my_print); // register print function for debugging
#endif

	tft.init();

	tft.setRotation(1);
	uint16_t calData[5] = { 159, 3738, 272, 3562, 7 };
	tft.setTouch(calData);

	// Create a draw buffer: LVGL will render the graphics here first, and send the rendered image to the display. The buffer size can be set freely but 1/10 screen size is a good starting point.
	lv_disp_draw_buf_init(&draw_buf, buf1, NULL, MY_DISP_HOR_RES * MY_DISP_VER_RES / 10);  /*Initialize the display buffer.*/

	// Implement and register a function which can copy the rendered image to an area of your display:
	static lv_disp_drv_t disp_drv;        /*Descriptor of a display driver*/
	lv_disp_drv_init(&disp_drv);          /*Basic initialization*/
	disp_drv.flush_cb = my_disp_flush;    /*Set your driver function*/
	disp_drv.draw_buf = &draw_buf;        /*Assign the buffer to the display*/
	disp_drv.hor_res = MY_DISP_HOR_RES;   /*Set the horizontal resolution of the display*/
	disp_drv.ver_res = MY_DISP_VER_RES;   /*Set the vertical resolution of the display*/
	lv_disp_drv_register(&disp_drv);      /*Finally register the driver*/

	// Implement and register a function which can read an input device. E.g. for a touchpad:
	static lv_indev_drv_t indev_drv;           /*Descriptor of a input device driver*/
	lv_indev_drv_init(&indev_drv);             /*Basic initialization*/
	indev_drv.type = LV_INDEV_TYPE_POINTER;    /*Touch pad is a pointer-like device*/
	indev_drv.read_cb = my_touchpad_read;      /*Set your driver function*/
	lv_indev_drv_register(&indev_drv);         /*Finally register the driver*/

	connectNet();

	syncTime();

	Wire.begin();
	while (!bme.begin())
	{
		Serial.println("Could not find BME280 sensor!");
		delay(1000);
	}
	switch (bme.chipModel())
	{
		case BME280::ChipModel_BME280:
			Serial.println("Found BME280 sensor! Success.");
			break;
		case BME280::ChipModel_BMP280:
			Serial.println("Found BMP280 sensor! No Humidity available.");
			break;
		default:
			Serial.println("Found UNKNOWN sensor! Error!");
	}


	my_demo();


}

void loop()
{
	lv_task_handler(); // let the GUI do its work
	delay(5);
}
