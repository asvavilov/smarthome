// TODO придумать интерфейсы и функционал
//      темная/светлая тема по времени заката/восхода
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

static lv_disp_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];

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
void checkNetTask(lv_task_t * task)
{
	if (WiFi.status() != WL_CONNECTED)
	{
		Serial.printf("Reconnecting to %s \n", SSID_NAME);
		WiFi.begin(SSID_NAME, SSID_PASS);
	}
}

void syncTime()
{
	// init and get the time
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}
void syncTimeTask(lv_task_t * task)
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
	Serial.println("Time synchronization succeeded");

	return timeinfo;
}
void printTimeTask(lv_task_t * task)
{
	TimeData * time_data = (TimeData *)(task->user_data);
	tm timeinfo = readTime();
	// TODO выводить полную дату?
	lv_label_set_text_fmt(time_data->label_time, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

void printWeatherTask(lv_task_t * task)
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

static void btn_event_cb(lv_obj_t * btn, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED)
	{
		static uint8_t cnt = 0;
		cnt++;

		// Get the first child of the button which is the label and change its text
		lv_obj_t * label = lv_obj_get_child(btn, NULL);
		lv_label_set_text_fmt(label, "Ещё топчи %d", cnt);

		// TODO перенести пищалку в таймер
		digitalWrite(SPEAKER_PIN, HIGH);
		delay(180);
		digitalWrite(SPEAKER_PIN, LOW);
	}
}
void my_demo()
{

	// Create simple label
	lv_obj_t * label = lv_label_create(lv_scr_act(), NULL);
	//lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_montserrat_16);
	lv_label_set_text(label, "Hello Arduino! Привет мир!");
	lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);

	lv_obj_t * btn = lv_btn_create(lv_scr_act(), NULL); // Add a button the current screen
	lv_obj_set_pos(btn, 10, 10); // Set its position
	lv_obj_set_size(btn, 150, 50); // Set its size
	lv_obj_set_event_cb(btn, btn_event_cb); // Assign a callback to the button

	lv_obj_t * label2 = lv_label_create(btn, NULL); // Add a label to the button
	lv_label_set_text(label2, "Топчи на кнопку"); // Set the labels text

	lv_obj_t * label_temperature = lv_label_create(lv_scr_act(), NULL);
	lv_label_set_text(label_temperature, "");
	lv_obj_align(label_temperature, NULL, LV_ALIGN_CENTER, 0, 20);
	lv_obj_t * label_humidity = lv_label_create(lv_scr_act(), NULL);
	lv_label_set_text(label_humidity, "");
	lv_obj_align(label_humidity, NULL, LV_ALIGN_CENTER, 0, 35);
	lv_obj_t * label_pressure = lv_label_create(lv_scr_act(), NULL);
	lv_label_set_text(label_pressure, "");
	lv_obj_align(label_pressure, NULL, LV_ALIGN_CENTER, 0, 50);
	WeatherData * user_data = new WeatherData();
	user_data->label_temperature = label_temperature;
	user_data->label_humidity = label_humidity;
	user_data->label_pressure = label_pressure;
	lv_task_t * taskWeather = lv_task_create(printWeatherTask, 60000, LV_TASK_PRIO_MID, user_data);
	lv_task_ready(taskWeather);

	// monitoring net
	lv_task_t * taskNet = lv_task_create(checkNetTask, 30000, LV_TASK_PRIO_MID, nullptr);
	lv_task_ready(taskNet);

	// sync
	lv_task_t * taskSync = lv_task_create(syncTimeTask, 86400000, LV_TASK_PRIO_MID, nullptr);
	lv_task_ready(taskSync);
	// time
	lv_obj_t * label_time = lv_label_create(lv_scr_act(), NULL);
	lv_label_set_text(label_time, "");
	lv_obj_align(label_time, NULL, LV_ALIGN_CENTER, 0, 80);
	TimeData * time_data = new TimeData();
	time_data->label_time = label_time;
	lv_task_t * taskTime = lv_task_create(printTimeTask, 1000, LV_TASK_PRIO_MID, time_data);
	lv_task_ready(taskTime);
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
bool my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data)
{
	uint16_t touchX, touchY;

	bool touched = tft.getTouch(&touchX, &touchY, 600);

	if(!touched) {
		data->state = LV_INDEV_STATE_REL;
	} else {
		data->state = LV_INDEV_STATE_PR;

		// Set the coordinates
		data->point.x = touchX;
		data->point.y = touchY;
	}

	return false; // Return `false` because we are not buffering and no more data to read
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

	lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);

	// Initialize the display
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.hor_res = TFT_HEIGHT;
	disp_drv.ver_res = TFT_WIDTH;
	disp_drv.flush_cb = my_disp_flush;
	disp_drv.buffer = &disp_buf;
	lv_disp_drv_register(&disp_drv);

	// Initialize the (dummy) input device driver
	lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = my_touchpad_read;
	lv_indev_drv_register(&indev_drv);

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
