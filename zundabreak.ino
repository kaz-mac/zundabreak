/*
  zundabreak.ino
  30分以上PCに向かっているとずんだもんが休憩を促すデバイス for M5Stack ATOM-ECHO
  ToFセンサー Unit-ToF4M使用

  Copyright (c) 2024 Kaz  (https://akibabara.com/blog/)
  Released under the MIT license.
  see https://opensource.org/licenses/MIT

  音声データ VOICEVOX: ずんだもん
*/
#include <M5Unified.h>

// 動作設定
const uint32_t WORK_LIMIT = 1800;       // 30分以上働いたら休憩を促す(s)
const uint32_t BREAK_TIME_LONG = 90;    // 休憩の長さ(s)
const int DISPLAY_DISTANCE_MIN = 400;   // PCの前にいると判断する位置 MIN (mm)
const int DISPLAY_DISTANCE_MAX = 800;   // PCの前にいると判断する位置 MAX (mm)

// LED関係
#include <FastLED.h>
#define NUM_LEDS 1
#define LED_DATA_PIN 27
CRGB leds[NUM_LEDS];

// 音声データ
#include "sound.h"
#define SAMPLE_RATE 24000
const unsigned char* soundData[] PROGMEM = { sound000, sound001, sound002 };
const size_t sizeData[] PROGMEM = { sizeof(sound000), sizeof(sound001), sizeof(sound002) };

// I2S関係
#include <driver/i2s.h>
#define CONFIG_I2S_BCK_PIN     19
#define CONFIG_I2S_LRCK_PIN    33
#define CONFIG_I2S_DATA_PIN    22
#define CONFIG_I2S_DATA_IN_PIN 23
#define SPEAK_I2S_NUMBER I2S_NUM_0
#define MODE_MIC 0
#define MODE_SPK 1

// ハードウェア設定
#define GPIO_SDA 26   // GPIOポート SDA
#define GPIO_SCL 32   // GPIOポート SCL

// ToF距離センサー関係 Unit-ToF4M使用
#include <Wire.h>
#include <VL53L1X.h>
VL53L1X sensor;

// その他
uint32_t workingtime = 0;   // 働いた時間(s)

// デバッグに便利なマクロ定義 --------
#define sp(x) Serial.println(x)
#define spn(x) Serial.print(x)
#define spp(k,v) Serial.println(String(k)+"="+String(v))
#define spf(fmt, ...) Serial.printf(fmt, __VA_ARGS__)

// I2Sの設定
// スケッチ例 > M5Stack > M5Atom > Echo > PlayMusic
bool InitI2SSpeakOrMic(int mode) {
	esp_err_t err = ESP_OK;

	i2s_driver_uninstall(SPEAK_I2S_NUMBER);
	i2s_config_t i2s_config = {
		.mode                 = (i2s_mode_t)(I2S_MODE_MASTER),
		.sample_rate          = SAMPLE_RATE,
		.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
		.channel_format       = I2S_CHANNEL_FMT_ALL_RIGHT,
		.communication_format = I2S_COMM_FORMAT_I2S,
		.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
		.dma_buf_count        = 6,
		.dma_buf_len          = 60,
		.use_apll             = false,
		.tx_desc_auto_clear   = true,
		.fixed_mclk           = 0,
	};
	if (mode == MODE_MIC) {
		i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
	} else {
		i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
	}

	err += i2s_driver_install(SPEAK_I2S_NUMBER, &i2s_config, 0, NULL);

	i2s_pin_config_t tx_pin_config;
  #if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 3, 0))
	tx_pin_config.mck_io_num = I2S_PIN_NO_CHANGE;
  #endif
	tx_pin_config.bck_io_num   = CONFIG_I2S_BCK_PIN;
	tx_pin_config.ws_io_num    = CONFIG_I2S_LRCK_PIN;
	tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
	tx_pin_config.data_in_num  = CONFIG_I2S_DATA_IN_PIN;

	err += i2s_set_pin(SPEAK_I2S_NUMBER, &tx_pin_config);
	err += i2s_set_clk(SPEAK_I2S_NUMBER, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  i2s_zero_dma_buffer(SPEAK_I2S_NUMBER);

	return true;
}

// ずんだもんが喋る
void speak_zundamon(int no) {
  size_t bytes_written;
  i2s_write(SPEAK_I2S_NUMBER, soundData[no], sizeData[no], &bytes_written, portMAX_DELAY);
  i2s_zero_dma_buffer(SPEAK_I2S_NUMBER);
}

// LEDの色を変える
void led_color(uint8_t r, uint8_t g, uint8_t b) {
  leds[0] = CRGB(r, g, b);
  FastLED.show();
} 

// 初期化
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Wire.begin(GPIO_SDA, GPIO_SCL); // 別に引数付けなくてもよい
	Serial.begin(115200);
	sp("start");

  // LEDの設定
  FastLED.addLeds<WS2811, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(20);
  led_color(0, 255, 0); // LED 緑

  // ToFセンサーの設定
  sensor.setBus(&Wire);
  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("Failed to detect and initialize sensor!");
    led_color(255, 0, 0); // LED 赤
    while (1);
  }
  sensor.setDistanceMode(VL53L1X::Long);
  sensor.setMeasurementTimingBudget(50000);
  sensor.startContinuous(50);

  // センサー距離調整デバッグモード　ボタン押しながら電源を入れる
  delay(1000);
  if (M5.BtnA.isPressed()) {
    while (1) {
      int distance = sensor.readRangeContinuousMillimeters();
      sp(distance);
      if (distance >= DISPLAY_DISTANCE_MIN && distance <= DISPLAY_DISTANCE_MAX) {
        led_color(255, 0, 0); // LED 赤
      } else {
        led_color(0, 255, 0); // LED 緑
      }
      delay(200);
    }
  }

  // スピーカーの初期化
	InitI2SSpeakOrMic(MODE_SPK);
	delay(100);
  speak_zundamon(0);  // ずんだもん「はじめるのだ」
}

void loop() {
	M5.update();

  // 一定時間ごとに距離を計測する
  static uint32_t tm = 0;
  if (tm == 0 || tm < millis()) {
    int distance = sensor.readRangeContinuousMillimeters();
    if (distance >= DISPLAY_DISTANCE_MIN && distance <= DISPLAY_DISTANCE_MAX) {
      workingtime += 1;
      tm = millis() + 1*1000;  // 次回は1秒後に測定
      // LEDの色を変える
      int tired = (int)((workingtime / (float)WORK_LIMIT) * 255);
      led_color(tired, 255-tired, 0);   // LED 緑→赤
    } else {
      tm = millis() + 200;  // 200ms毎に測定
    }
    spn(distance);
    spn(" ");
    sp(workingtime);
  }

  // 仕事しすぎたとき
  if (workingtime >= WORK_LIMIT || M5.BtnA.wasPressed()) {
    workingtime = 0;
    led_color(0, 0, 255); // LED 青
    speak_zundamon(1);    // ずんだもん「ちょっと休憩するのだ」
    delay(BREAK_TIME_LONG * 1000);
    speak_zundamon(2);    // ずんだもん「そろそろいいのだ」
    led_color(0, 255, 0); // LED 緑
  }

  delay(10);
}

