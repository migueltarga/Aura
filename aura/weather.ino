#include <Arduino.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include "esp_system.h"

// Constants
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS
#define LCD_BACKLIGHT_PIN 21
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))

#define LATITUDE_DEFAULT "51.5074"
#define LONGITUDE_DEFAULT "-0.1278"
#define LOCATION_DEFAULT "London"
#define DEFAULT_CAPTIVE_SSID "Aura"
#define UPDATE_INTERVAL 600000UL  // 10 minutes

// Language support
enum Language {
  LANG_EN = 0,  // English (default)
  LANG_PT_BR,   // Portuguese (Brazil)
  LANG_ES,      // Spanish
  LANG_FR,      // French
  LANG_COUNT    // Number of supported languages
};

// Weather image type enum
enum ImageType { ICON, LARGE_IMAGE };

// Language names for display
static const char* language_names[LANG_COUNT] = {
  "English",
  "Portugues",
  "Espanol",
  "Francais"
};

// Current language
static Language current_language = LANG_EN;

// Touchscreen setup
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
uint32_t draw_buf[DRAW_BUF_SIZE / 4];
static const char *weekdays[] = {"Sun", "Mon", "Tues", "Wed", "Thurs", "Fri", "Sat"};
static const char *weekdays_pt_br[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};
static const char *weekdays_es[] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
static const char *weekdays_fr[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
int x, y, z;

// Preferences
static Preferences prefs;
static bool use_fahrenheit = false;
static char latitude[16] = LATITUDE_DEFAULT;
static char longitude[16] = LONGITUDE_DEFAULT;
static String location = String(LOCATION_DEFAULT);
static char dd_opts[512];
static DynamicJsonDocument geoDoc(8 * 1024);
static JsonArray geoResults;

// UI components
static lv_obj_t *lbl_today_temp;
static lv_obj_t *lbl_today_feels_like;
static lv_obj_t *img_today_icon;
static lv_obj_t *lbl_forecast;
static lv_obj_t *box_daily;
static lv_obj_t *box_hourly;
static lv_obj_t *lbl_daily_day[7];
static lv_obj_t *lbl_daily_high[7];
static lv_obj_t *lbl_daily_low[7];
static lv_obj_t *img_daily[7];
static lv_obj_t *lbl_hourly[7];
static lv_obj_t *lbl_precipitation_probability[7];
static lv_obj_t *lbl_hourly_temp[7];
static lv_obj_t *img_hourly[7];
static lv_obj_t *lbl_loc;
static lv_obj_t *loc_ta;
static lv_obj_t *results_dd;
static lv_obj_t *btn_close_loc;
static lv_obj_t *btn_close_obj;
static lv_obj_t *kb;
static lv_obj_t *settings_win;
static lv_obj_t *location_win = nullptr;
static lv_obj_t *unit_switch;
static lv_obj_t *lbl_clock;
static lv_obj_t *lang_dropdown;

// Weather icons
LV_IMG_DECLARE(icon_blizzard);
LV_IMG_DECLARE(icon_blowing_snow);
LV_IMG_DECLARE(icon_clear_night);
LV_IMG_DECLARE(icon_cloudy);
LV_IMG_DECLARE(icon_drizzle);
LV_IMG_DECLARE(icon_flurries);
LV_IMG_DECLARE(icon_haze_fog_dust_smoke);
LV_IMG_DECLARE(icon_heavy_rain);
LV_IMG_DECLARE(icon_heavy_snow);
LV_IMG_DECLARE(icon_isolated_scattered_tstorms_day);
LV_IMG_DECLARE(icon_isolated_scattered_tstorms_night);
LV_IMG_DECLARE(icon_mostly_clear_night);
LV_IMG_DECLARE(icon_mostly_cloudy_day);
LV_IMG_DECLARE(icon_mostly_cloudy_night);
LV_IMG_DECLARE(icon_mostly_sunny);
LV_IMG_DECLARE(icon_partly_cloudy);
LV_IMG_DECLARE(icon_partly_cloudy_night);
LV_IMG_DECLARE(icon_scattered_showers_day);
LV_IMG_DECLARE(icon_scattered_showers_night);
LV_IMG_DECLARE(icon_showers_rain);
LV_IMG_DECLARE(icon_sleet_hail);
LV_IMG_DECLARE(icon_snow_showers_snow);
LV_IMG_DECLARE(icon_strong_tstorms);
LV_IMG_DECLARE(icon_sunny);
LV_IMG_DECLARE(icon_tornado);
LV_IMG_DECLARE(icon_wintry_mix_rain_snow);

// Weather Images
LV_IMG_DECLARE(image_blizzard);
LV_IMG_DECLARE(image_blowing_snow);
LV_IMG_DECLARE(image_clear_night);
LV_IMG_DECLARE(image_cloudy);
LV_IMG_DECLARE(image_drizzle);
LV_IMG_DECLARE(image_flurries);
LV_IMG_DECLARE(image_haze_fog_dust_smoke);
LV_IMG_DECLARE(image_heavy_rain);
LV_IMG_DECLARE(image_heavy_snow);
LV_IMG_DECLARE(image_isolated_scattered_tstorms_day);
LV_IMG_DECLARE(image_isolated_scattered_tstorms_night);
LV_IMG_DECLARE(image_mostly_clear_night);
LV_IMG_DECLARE(image_mostly_cloudy_day);
LV_IMG_DECLARE(image_mostly_cloudy_night);
LV_IMG_DECLARE(image_mostly_sunny);
LV_IMG_DECLARE(image_partly_cloudy);
LV_IMG_DECLARE(image_partly_cloudy_night);
LV_IMG_DECLARE(image_scattered_showers_day);
LV_IMG_DECLARE(image_scattered_showers_night);
LV_IMG_DECLARE(image_showers_rain);
LV_IMG_DECLARE(image_sleet_hail);
LV_IMG_DECLARE(image_snow_showers_snow);
LV_IMG_DECLARE(image_strong_tstorms);
LV_IMG_DECLARE(image_sunny);
LV_IMG_DECLARE(image_tornado);
LV_IMG_DECLARE(image_wintry_mix_rain_snow);

// Translation dictionary
struct TranslationEntry {
  const char* en;       // English (default)
  const char* pt_br;    // Portuguese (Brazil) 
  const char* es;       // Spanish
  const char* fr;       // French
};

// UI Text translations
static const TranslationEntry tr_today = {"Today", "Hoje", "Hoy", "Ajd"};
static const TranslationEntry tr_feels_like = {"Feels Like", "Sensacao", "Sensacion", "Ressenti"};
static const TranslationEntry tr_forecast_7day = {"SEVEN DAY FORECAST", "PREVISAO DE 7 DIAS", "PRONOSTICO DE 7 DIAS", "PREVISION 7 JOURS"};
static const TranslationEntry tr_forecast_hourly = {"HOURLY FORECAST", "PREVISAO POR HORA", "PRONOSTICO POR HORA", "PREVISION HORAIRE"};
static const TranslationEntry tr_now = {"Now", "Agora", "Ahora", "mnt"};
static const TranslationEntry tr_save = {"Save", "Salvar", "Guardar", "Enregistrer"};
static const TranslationEntry tr_cancel = {"Cancel", "Cancelar", "Cancelar", "Annuler"};
static const TranslationEntry tr_close = {"Close", "Fechar", "Cerrar", "Fermer"};
static const TranslationEntry tr_language = {"Language:", "Idioma:", "Idioma:", "Langue:"};
static const TranslationEntry tr_brightness = {"Brightness:", "Brilho:", "Brillo:", "Luminosite:"};
static const TranslationEntry tr_location = {"Location:", "Localizacao:", "Ubicacion:", "Emplacement:"};
static const TranslationEntry tr_location_btn = {"Location", "Localizacao", "Ubicacion", "Emplacement"};
static const TranslationEntry tr_city = {"City:", "Cidade:", "Ciudad:", "Ville:"};
static const TranslationEntry tr_search_results = {"Search Results", "Resultados", "Resultados", "Resultats"};
static const TranslationEntry tr_use_f = {"Use F:", "Usar F:", "Usar F:", "Utiliser F:"};
static const TranslationEntry tr_reset_wifi = {"Reset Wi-Fi", "Reset Wi-Fi", "Reset Wi-Fi", "Reset Wi-Fi"};
static const TranslationEntry tr_settings = {"Aura Settings", "Configuracoes", "Configuracion", "Parametres"};
static const TranslationEntry tr_change_location = {"Change Location", "Mudar Localizacao", "Cambiar Ubicacion", "Changer Emplacement"};
static const TranslationEntry tr_reset = {"Reset", "Redefinir", "Restablecer", "Reinitialiser"};
static const TranslationEntry tr_wifi_msg = {"Are you sure you want to reset Wi-Fi credentials?\n\nYou'll need to reconnect to the Wifi SSID " DEFAULT_CAPTIVE_SSID " with your phone or browser to reconfigure Wi-Fi credentials.",
                                        "Tem certeza que deseja redefinir as credenciais Wi-Fi?\n\nVoce precisara reconectar ao SSID " DEFAULT_CAPTIVE_SSID " com seu celular ou navegador para reconfigurar as credenciais Wi-Fi.",
                                        "Estas seguro de querer restablecer las credenciales Wi-Fi?\n\nNecesitaras reconectar al SSID " DEFAULT_CAPTIVE_SSID " con tu telefono o navegador para reconfigurar las credenciales Wi-Fi.",
                                        "Etes-vous sur de vouloir reinitialiser les identifiants Wi-Fi?\n\nVous devrez vous reconnecter au SSID " DEFAULT_CAPTIVE_SSID " avec votre telephone ou navigateur pour reconfigurer les identifiants Wi-Fi."};
static const TranslationEntry tr_wifi_config = {"Wi-Fi Configuration:\n\nPlease connect your\nphone or laptop to the\ntemporary Wi-Fi access\npoint " DEFAULT_CAPTIVE_SSID "\nto configure.\n\nIf you don't see a \nconfiguration screen \nafter connecting,\nvisit http://192.168.4.1\nin your web browser.",
                                          "Configuracao Wi-Fi:\n\nPor favor, conecte seu\ncelular ou notebook ao\nponto de acesso\ntemporario " DEFAULT_CAPTIVE_SSID "\npara configurar.\n\nSe voce nao vir uma\ntela de configuracao\ndepois de conectar,\nvisite http://192.168.4.1\nem seu navegador.",
                                          "Configuracion Wi-Fi:\n\nPor favor, conecta tu\ntelefono o portatil al\npunto de acceso\ntemporal " DEFAULT_CAPTIVE_SSID "\npara configurar.\n\nSi no ves una\npantalla de configuracion\ndespues de conectar,\nvisita http://192.168.4.1\nen tu navegador.",
                                          "Configuration Wi-Fi:\n\nVeuillez connecter votre\ntelephone ou ordinateur au\npoint d'acces\ntemporaire " DEFAULT_CAPTIVE_SSID "\npour configurer.\n\nSi vous ne voyez pas un\necran de configuration\napres la connexion,\nvisitez http://192.168.4.1\ndans votre navigateur."};

// Helper function to get translated text
const char* tr(const TranslationEntry& entry) {
  switch (current_language) {
    case LANG_PT_BR: return entry.pt_br;
    case LANG_ES: return entry.es;
    case LANG_FR: return entry.fr;
    case LANG_EN:
    default: return entry.en;
  }
}

// Function to get weekday name based on current language
const char* get_weekday(int day_index) {
  if (day_index < 0 || day_index > 6) return "???";
  
  switch (current_language) {
    case LANG_PT_BR: return weekdays_pt_br[day_index];
    case LANG_ES: return weekdays_es[day_index];
    case LANG_FR: return weekdays_fr[day_index];
    case LANG_EN:
    default: return weekdays[day_index];
  }
}

// Function prototypes
void create_ui();
void fetch_and_update_weather();
void create_settings_window();
void create_location_dialog();
void do_geocode_query(const char *q);
void update_ui_language();
static void screen_event_cb(lv_event_t *e);
static void settings_event_handler(lv_event_t *e);
static void daily_cb(lv_event_t *e);
static void hourly_cb(lv_event_t *e);
static void reset_wifi_event_handler(lv_event_t *e);
static void reset_confirm_yes_cb(lv_event_t *e);
static void reset_confirm_no_cb(lv_event_t *e);
static void change_location_event_cb(lv_event_t *e);
static void location_save_event_cb(lv_event_t *e);
static void location_cancel_event_cb(lv_event_t *e);
static void language_changed_cb(lv_event_t *e);
static void ta_event_cb(lv_event_t *e);
static void kb_event_cb(lv_event_t *e);
static void ta_defocus_cb(lv_event_t *e);
void touchscreen_read(lv_indev_t *indev, lv_indev_data_t *data);
void wifi_splash_screen();
void flush_wifi_splashscreen(uint32_t ms);
void apModeCallback(WiFiManager *mgr);
static void update_clock(lv_timer_t *timer);
void populate_results_dropdown();

// Weather mapping functions - combined to reduce duplication
const lv_img_dsc_t* get_weather_image(int wmo_code, int is_day, ImageType type);
const lv_img_dsc_t* choose_icon(int wmo_code, int is_day);
const lv_img_dsc_t* choose_image(int wmo_code, int is_day);

// Helper function to convert Celsius to Fahrenheit if needed
float convert_temp(float temp_c) {
  return use_fahrenheit ? (temp_c * 9.0 / 5.0 + 32.0) : temp_c;
}

// Utility functions
int day_of_week(int y, int m, int d) {
  static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

String hour_of_day(int hour) {
  if(hour < 0 || hour > 23) return String("Invalid hour");

  if(hour == 0)   return String("12am");
  if(hour == 12)  return String("Noon");

  bool isMorning = (hour < 12);
  String suffix = isMorning ? "am" : "pm";

  int displayHour = hour % 12;
  if (displayHour == 0) displayHour = 12;

  return String(displayHour) + suffix;
}

String urlencode(const String &str) {
  String encoded = "";
  char buf[5];
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    // Unreserved characters according to RFC 3986
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      // Percent-encode others
      sprintf(buf, "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

static void update_clock(lv_timer_t *timer) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  char buf[16];
  int hour = timeinfo.tm_hour % 12;
  if(hour == 0) hour = 12;
  
  snprintf(buf, sizeof(buf), "%d:%02d%s", hour, timeinfo.tm_min, 
           (timeinfo.tm_hour < 12) ? "am" : "pm");

  lv_label_set_text(lbl_clock, buf);
}

static void ta_event_cb(lv_event_t *e) {
  lv_obj_t *kb = (lv_obj_t *)lv_event_get_user_data(e);

  // Show keyboard
  lv_keyboard_set_textarea(kb, (lv_obj_t *)lv_event_get_target(e));
  lv_obj_move_foreground(kb);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

static void kb_event_cb(lv_event_t *e) {
  lv_obj_t *kb = static_cast<lv_obj_t *>(lv_event_get_target(e));
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

  if (lv_event_get_code(e) == LV_EVENT_READY) {
    const char *loc = lv_textarea_get_text(loc_ta);
    if (strlen(loc) > 0) {
      do_geocode_query(loc);
    }
  }
}

static void ta_defocus_cb(lv_event_t *e) {
  lv_obj_add_flag((lv_obj_t *)lv_event_get_user_data(e), LV_OBJ_FLAG_HIDDEN);
}

void touchscreen_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();

    // Map touchscreen coordinates to screen coordinates
    x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    z = p.z;

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  TFT_eSPI tft = TFT_eSPI();
  tft.init();
  pinMode(LCD_BACKLIGHT_PIN, OUTPUT);

  // Initialize LVGL
  lv_init();

  // Init touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);

  // Set up LVGL display and input
  lv_display_t *disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);

  // Load saved preferences
  prefs.begin("weather", false);
  String lat = prefs.getString("latitude", LATITUDE_DEFAULT);
  lat.toCharArray(latitude, sizeof(latitude));
  String lon = prefs.getString("longitude", LONGITUDE_DEFAULT);
  lon.toCharArray(longitude, sizeof(longitude));
  use_fahrenheit = prefs.getBool("useFahrenheit", false);
  location = prefs.getString("location", LOCATION_DEFAULT);
  uint32_t brightness = prefs.getUInt("brightness", 255);
  current_language = (Language)prefs.getUInt("language", LANG_EN);
  // Validate language is in range
  if (current_language >= LANG_COUNT) {
    current_language = LANG_EN;
  }
  
  analogWrite(LCD_BACKLIGHT_PIN, brightness);
  
  // Initialize Wi-Fi
  WiFiManager wm;
  wm.setAPCallback(apModeCallback);
  wm.autoConnect(DEFAULT_CAPTIVE_SSID);

  // Create a timer to update the clock
  lv_timer_create(update_clock, 1000, NULL);

  // Clear the screen and create the UI
  lv_obj_clean(lv_scr_act());
  create_ui();
  
  // Fetch weather data
  fetch_and_update_weather();
}

void loop() {
  static uint32_t last_weather_update = millis();

  // Handle LVGL tasks
  lv_timer_handler();

  // Update weather data at the specified interval
  if (millis() - last_weather_update >= UPDATE_INTERVAL) {
    fetch_and_update_weather();
    last_weather_update = millis();
  }

  // Update LVGL tick
  lv_tick_inc(5);
  delay(5);
}

void flush_wifi_splashscreen(uint32_t ms = 200) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    lv_timer_handler();
    delay(5);
  }
}

void apModeCallback(WiFiManager *mgr) {
  wifi_splash_screen();
  flush_wifi_splashscreen();
}

void wifi_splash_screen() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  
  // Set gradient background
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x4c8cb9), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0xa6cdec), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Create info label
  lv_obj_t *lbl = lv_label_create(scr);
  lv_label_set_text(lbl, tr(tr_wifi_config));
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(lbl);
  lv_scr_load(scr);
}

// Helper function to create a forecast box with common properties
lv_obj_t* create_forecast_box(lv_obj_t *parent, bool hidden) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_size(box, 220, 180);
  lv_obj_align(box, LV_ALIGN_TOP_LEFT, 10, 135);
  
  // Set common style properties
  lv_obj_set_style_bg_color(box, lv_color_hex(0x5e9bc8), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(box, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF); 
  lv_obj_set_style_pad_all(box, 10, LV_PART_MAIN); 
  lv_obj_set_style_pad_gap(box, 0, LV_PART_MAIN);
  
  if (hidden) {
    lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
  }
  
  return box;
}

// Helper function to create a styled label with default properties
lv_obj_t* create_styled_label(lv_obj_t *parent, lv_style_t *style, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *label = lv_label_create(parent);
  
  if (style) {
    lv_obj_add_style(label, style, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
  
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
  
  return label;
}

void create_ui() {
  // Set up the main screen
  lv_obj_t *scr = lv_scr_act();
  
  // Set gradient background
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x4c8cb9), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0xa6cdec), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Trigger settings screen on touch
  lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_CLICKED, NULL);

  // Create current weather display
  img_today_icon = lv_img_create(scr);
  lv_img_set_src(img_today_icon, &image_partly_cloudy);
  lv_obj_align(img_today_icon, LV_ALIGN_TOP_MID, -64, 4);

  // Create a default style for labels
  static lv_style_t default_label_style;
  lv_style_init(&default_label_style);
  lv_style_set_text_color(&default_label_style, lv_color_hex(0xFFFFFF));
  lv_style_set_text_opa(&default_label_style, LV_OPA_COVER);

  // Create temperature labels
  lbl_today_temp = create_styled_label(scr, &default_label_style, &lv_font_montserrat_42, lv_color_hex(0xFFFFFF));
  lv_label_set_text(lbl_today_temp, "--°C");
  lv_obj_align(lbl_today_temp, LV_ALIGN_TOP_MID, 45, 25);

  lbl_today_feels_like = create_styled_label(scr, NULL, &lv_font_montserrat_14, lv_color_hex(0xe4ffff));
  lv_label_set_text(lbl_today_feels_like, tr(tr_feels_like));
  lv_obj_align(lbl_today_feels_like, LV_ALIGN_TOP_MID, 45, 75);

  // Create forecast label
  lbl_forecast = create_styled_label(scr, NULL, &lv_font_montserrat_12, lv_color_hex(0xe4ffff));
  lv_label_set_text(lbl_forecast, tr(tr_forecast_7day));
  lv_obj_align(lbl_forecast, LV_ALIGN_TOP_LEFT, 20, 110);

  // Create daily forecast box
  box_daily = create_forecast_box(scr, false);
  lv_obj_add_event_cb(box_daily, daily_cb, LV_EVENT_CLICKED, NULL);

  // Create and configure daily forecast elements
  for (int i = 0; i < 7; i++) {
    lbl_daily_day[i] = create_styled_label(box_daily, &default_label_style, &lv_font_montserrat_16, lv_color_hex(0xFFFFFF));
    lv_obj_align(lbl_daily_day[i], LV_ALIGN_TOP_LEFT, 2, i * 24);

    lbl_daily_high[i] = create_styled_label(box_daily, &default_label_style, &lv_font_montserrat_16, lv_color_hex(0xFFFFFF));
    lv_obj_align(lbl_daily_high[i], LV_ALIGN_TOP_RIGHT, 0, i * 24);

    lbl_daily_low[i] = create_styled_label(box_daily, NULL, &lv_font_montserrat_16, lv_color_hex(0xb9ecff));
    lv_label_set_text(lbl_daily_low[i], "");
    lv_obj_align(lbl_daily_low[i], LV_ALIGN_TOP_RIGHT, -50, i * 24);

    img_daily[i] = lv_img_create(box_daily);
    lv_img_set_src(img_daily[i], &icon_partly_cloudy);
    lv_obj_align(img_daily[i], LV_ALIGN_TOP_LEFT, 72, i * 24);
  }

  // Create hourly forecast box
  box_hourly = create_forecast_box(scr, true);
  lv_obj_add_event_cb(box_hourly, hourly_cb, LV_EVENT_CLICKED, NULL);

  // Create and configure hourly forecast elements
  for (int i = 0; i < 7; i++) {
    lbl_hourly[i] = create_styled_label(box_hourly, &default_label_style, &lv_font_montserrat_16, lv_color_hex(0xFFFFFF));
    lv_obj_align(lbl_hourly[i], LV_ALIGN_TOP_LEFT, 2, i * 24);

    lbl_hourly_temp[i] = create_styled_label(box_hourly, &default_label_style, &lv_font_montserrat_16, lv_color_hex(0xFFFFFF));
    lv_obj_align(lbl_hourly_temp[i], LV_ALIGN_TOP_RIGHT, 0, i * 24);

    lbl_precipitation_probability[i] = create_styled_label(box_hourly, NULL, &lv_font_montserrat_16, lv_color_hex(0xb9ecff));
    lv_label_set_text(lbl_precipitation_probability[i], "");
    lv_obj_align(lbl_precipitation_probability[i], LV_ALIGN_TOP_RIGHT, -55, i * 24);

    img_hourly[i] = lv_img_create(box_hourly);
    lv_img_set_src(img_hourly[i], &icon_partly_cloudy);
    lv_obj_align(img_hourly[i], LV_ALIGN_TOP_LEFT, 72, i * 24);
  }

  // Create clock label
  lbl_clock = create_styled_label(scr, NULL, &lv_font_montserrat_14, lv_color_hex(0xb9ecff));
  lv_label_set_text(lbl_clock, "");
  lv_obj_align(lbl_clock, LV_ALIGN_TOP_RIGHT, -10, 5);
}

void populate_results_dropdown() {
  dd_opts[0] = '\0';
  for (JsonObject item : geoResults) {
    strcat(dd_opts, item["name"].as<const char *>());
    if (item["admin1"]) {
      strcat(dd_opts, ", ");
      strcat(dd_opts, item["admin1"].as<const char *>());
    }

    strcat(dd_opts, "\n");
  }

  if (geoResults.size() > 0) {
    lv_dropdown_set_options_static(results_dd, dd_opts);
    lv_obj_add_flag(results_dd, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(btn_close_loc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn_close_loc, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_close_loc, lv_palette_darken(LV_PALETTE_GREEN, 1), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_flag(btn_close_loc, LV_OBJ_FLAG_CLICKABLE);
  }
}

static void location_save_event_cb(lv_event_t *e) {
  JsonArray *pres = static_cast<JsonArray *>(lv_event_get_user_data(e));
  uint16_t idx = lv_dropdown_get_selected(results_dd);

  JsonObject obj = (*pres)[idx];
  double lat = obj["latitude"].as<double>();
  double lon = obj["longitude"].as<double>();

  // Update and save coordinates
  snprintf(latitude, sizeof(latitude), "%.6f", lat);
  snprintf(longitude, sizeof(longitude), "%.6f", lon);
  prefs.putString("latitude", latitude);
  prefs.putString("longitude", longitude);

  // Format location name
  String opts;
  const char *name = obj["name"];
  const char *admin = obj["admin1"];
  opts += name;
  if (admin) {
    opts += ", ";
    opts += admin;
  }

  // Save and update location
  prefs.putString("location", opts);
  location = opts;
  lv_label_set_text(lbl_loc, opts.c_str());
  
  // Fetch new weather data
  fetch_and_update_weather();

  // Close window
  lv_obj_del(location_win);
  location_win = nullptr;
}

static void location_cancel_event_cb(lv_event_t *e) {
  lv_obj_del(location_win);
  location_win = nullptr;
}

static void screen_event_cb(lv_event_t *e) {
  create_settings_window();
}

static void daily_cb(lv_event_t *e) {
  lv_obj_add_flag(box_daily, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(lbl_forecast, tr(tr_forecast_hourly));
  lv_obj_clear_flag(box_hourly, LV_OBJ_FLAG_HIDDEN);
}

static void hourly_cb(lv_event_t *e) {
  lv_obj_add_flag(box_hourly, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(lbl_forecast, tr(tr_forecast_7day));
  lv_obj_clear_flag(box_daily, LV_OBJ_FLAG_HIDDEN);
}

static void reset_wifi_event_handler(lv_event_t *e) {
  lv_obj_t *mbox = lv_msgbox_create(lv_scr_act());
  lv_obj_t *title = lv_msgbox_add_title(mbox, tr(tr_reset));
  lv_obj_set_style_margin_left(title, 10, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

  lv_msgbox_add_text(mbox, tr(tr_wifi_msg));
  lv_msgbox_add_close_button(mbox);

  // Create buttons with consistent styling
  lv_obj_t *btn_no = lv_msgbox_add_footer_button(mbox, tr(tr_cancel));
  lv_obj_t *btn_yes = lv_msgbox_add_footer_button(mbox, tr(tr_reset));

  lv_obj_set_style_bg_color(btn_yes, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn_yes, lv_palette_darken(LV_PALETTE_RED, 1), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_text_color(btn_yes, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Set messagebox size and style
  lv_obj_set_width(mbox, 230);
  lv_obj_center(mbox);

  lv_obj_set_style_border_width(mbox, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(mbox, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_border_opa(mbox, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(mbox, 4, LV_PART_MAIN);

  // Add event handlers
  lv_obj_add_event_cb(btn_yes, reset_confirm_yes_cb, LV_EVENT_CLICKED, mbox);
  lv_obj_add_event_cb(btn_no, reset_confirm_no_cb, LV_EVENT_CLICKED, mbox);
}

static void reset_confirm_yes_cb(lv_event_t *e) {
  Serial.println("Clearing Wi-Fi creds and rebooting");
  WiFiManager wm;
  wm.resetSettings();
  delay(100);
  esp_restart();
}

static void reset_confirm_no_cb(lv_event_t *e) {
  lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);
  lv_obj_del(mbox);
}

static void change_location_event_cb(lv_event_t *e) {
  if (location_win) return;
  create_location_dialog();
}

// Helper function to create a styled button
lv_obj_t* create_styled_button(lv_obj_t *parent, int width, int height, lv_color_t color, 
                              lv_color_t pressed_color, const char *text, lv_event_cb_t event_cb, 
                              void *user_data) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, width, height);
  
  lv_obj_set_style_bg_color(btn, color, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn, pressed_color, LV_PART_MAIN | LV_STATE_PRESSED);
  
  if (event_cb) {
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, user_data);
  }
  
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  
  return btn;
}

// Helper function to create a window with title
lv_obj_t* create_window(const char *title_text, int width = 0) {
  lv_obj_t *win = lv_win_create(lv_scr_act());
  lv_obj_t *title = lv_win_add_title(win, title_text);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_margin_left(title, 10, 0);
  
  if (width > 0) {
    lv_obj_set_width(win, width);
  }
  
  lv_obj_center(win);
  return win;
}

void create_location_dialog() {
  location_win = create_window(tr(tr_change_location), 240);
  lv_obj_set_size(location_win, 240, 320);
  
  lv_obj_t *cont = lv_win_get_content(location_win);
  lv_obj_set_style_pad_all(cont, 8, LV_PART_MAIN);

  // Create city label and text input
  lv_obj_t *lbl = lv_label_create(cont);
  lv_label_set_text(lbl, tr(tr_city));
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 5, 8);

  loc_ta = lv_textarea_create(cont);
  lv_textarea_set_one_line(loc_ta, true);
  lv_textarea_set_placeholder_text(loc_ta, "e.g. London");
  lv_obj_set_width(loc_ta, 160);
  lv_obj_align_to(loc_ta, lbl, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  lv_obj_add_event_cb(loc_ta, ta_event_cb, LV_EVENT_CLICKED, kb);
  lv_obj_add_event_cb(loc_ta, ta_defocus_cb, LV_EVENT_DEFOCUSED, kb);

  // Create results label and dropdown
  lv_obj_t *lbl2 = lv_label_create(cont);
  lv_label_set_text(lbl2, tr(tr_search_results));
  lv_obj_align(lbl2, LV_ALIGN_TOP_LEFT, 5, 45);

  results_dd = lv_dropdown_create(cont);
  lv_obj_set_width(results_dd, 210);
  lv_obj_align(results_dd, LV_ALIGN_TOP_LEFT, 5, 65);
  lv_dropdown_set_options(results_dd, "");
  lv_obj_clear_flag(results_dd, LV_OBJ_FLAG_CLICKABLE);

  // Create Save button
  btn_close_loc = create_styled_button(cont, 80, 40, 
                                      lv_palette_main(LV_PALETTE_GREY), 
                                      lv_palette_darken(LV_PALETTE_GREY, 1),
                                      tr(tr_save), location_save_event_cb, &geoResults);
  lv_obj_align(btn_close_loc, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
  lv_obj_clear_flag(btn_close_loc, LV_OBJ_FLAG_CLICKABLE);

  // Create Cancel button
  lv_obj_t *btn_cancel_loc = create_styled_button(cont, 80, 40,
                                                 lv_palette_main(LV_PALETTE_GREY),
                                                 lv_palette_darken(LV_PALETTE_GREY, 1),
                                                 tr(tr_cancel), location_cancel_event_cb, &geoResults);
  lv_obj_align_to(btn_cancel_loc, btn_close_loc, LV_ALIGN_OUT_LEFT_MID, -5, 0);
}

void create_settings_window() {
  if (settings_win) return;

  settings_win = create_window(tr(tr_settings), 240);
  lv_obj_t *cont = lv_win_get_content(settings_win);
  
  // More compact layout - reduce padding and margins
  lv_obj_set_style_pad_all(cont, 8, LV_PART_MAIN);

  // Create brightness control
  lv_obj_t *lbl_b = lv_label_create(cont);
  lv_label_set_text(lbl_b, tr(tr_brightness));
  lv_obj_align(lbl_b, LV_ALIGN_TOP_LEFT, 0, 5);
  
  lv_obj_t *slider = lv_slider_create(cont);
  lv_slider_set_range(slider, 10, 255);
  uint32_t saved_b = prefs.getUInt("brightness", 128);
  lv_slider_set_value(slider, saved_b, LV_ANIM_OFF);
  lv_obj_set_width(slider, 100);
  lv_obj_align_to(slider, lbl_b, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

  lv_obj_add_event_cb(slider, [](lv_event_t *e){
    lv_obj_t *s = (lv_obj_t*)lv_event_get_target(e);
    uint32_t v = lv_slider_get_value(s);
    analogWrite(LCD_BACKLIGHT_PIN, v);
    prefs.putUInt("brightness", v);
  }, LV_EVENT_VALUE_CHANGED, NULL);

  // Create language selector - more compact
  lv_obj_t *lbl_lang = lv_label_create(cont);
  lv_label_set_text(lbl_lang, tr(tr_language));
  lv_obj_align(lbl_lang, LV_ALIGN_TOP_LEFT, 0, 38);

  lang_dropdown = lv_dropdown_create(cont);
  lv_obj_set_width(lang_dropdown, 130);
  
  // Create dropdown options string with all language names
  char lang_buf[128] = "";
  for (int i = 0; i < LANG_COUNT; i++) {
    if (i > 0) strcat(lang_buf, "\n");
    strcat(lang_buf, language_names[i]);
  }
  
  lv_dropdown_set_options(lang_dropdown, lang_buf);
  lv_dropdown_set_selected(lang_dropdown, current_language);
  lv_obj_align_to(lang_dropdown, lbl_lang, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
  lv_obj_add_event_cb(lang_dropdown, language_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // Create temperature unit switch
  lv_obj_t *lbl_u = lv_label_create(cont);
  lv_label_set_text(lbl_u, tr(tr_use_f));
  lv_obj_align(lbl_u, LV_ALIGN_TOP_LEFT, 0, 71);

  unit_switch = lv_switch_create(cont);
  lv_obj_align_to(unit_switch, lbl_u, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

  if (use_fahrenheit) {
    lv_obj_add_state(unit_switch, LV_STATE_CHECKED);
  } else {
    lv_obj_remove_state(unit_switch, LV_STATE_CHECKED);
  }

  lv_obj_add_event_cb(unit_switch, settings_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

  // Create location display
  lv_obj_t *lbl_loc_l = lv_label_create(cont);
  lv_label_set_text(lbl_loc_l, tr(tr_location));
  lv_obj_align(lbl_loc_l, LV_ALIGN_TOP_LEFT, 0, 104);

  lbl_loc = lv_label_create(cont);
  lv_label_set_text(lbl_loc, location.c_str());
  lv_obj_set_width(lbl_loc, 120);
  lv_obj_set_style_text_font(lbl_loc, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_align_to(lbl_loc, lbl_loc_l, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  // Create keyboard if needed
  if (!kb) {
    kb = lv_keyboard_create(lv_scr_act());
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, NULL);
  }

  // Create action buttons - more compact layout
  lv_obj_t *button_container = lv_obj_create(cont);
  lv_obj_set_size(button_container, 220, 55);
  lv_obj_set_style_pad_all(button_container, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(button_container, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(button_container, 0, LV_PART_MAIN);
  lv_obj_align(button_container, LV_ALIGN_TOP_LEFT, 0, 135);
  
  // Create change location button
  lv_obj_t *btn_change_loc = create_styled_button(button_container, 100, 40,
                                                 lv_palette_main(LV_PALETTE_BLUE),
                                                 lv_palette_darken(LV_PALETTE_BLUE, 1),
                                                 tr(tr_location_btn), change_location_event_cb, NULL);
  lv_obj_align(btn_change_loc, LV_ALIGN_LEFT_MID, 0, 0);

  // Create reset Wi-Fi button
  lv_obj_t *btn_reset = create_styled_button(button_container, 100, 40,
                                            lv_palette_main(LV_PALETTE_RED),
                                            lv_palette_darken(LV_PALETTE_RED, 1),
                                            tr(tr_reset_wifi), reset_wifi_event_handler, nullptr);
  lv_obj_set_style_text_color(btn_reset, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_align(btn_reset, LV_ALIGN_RIGHT_MID, 0, 0);

  // Create close button
  btn_close_obj = create_styled_button(cont, 80, 40,
                                      lv_palette_main(LV_PALETTE_BLUE),
                                      lv_palette_darken(LV_PALETTE_BLUE, 1),
                                      tr(tr_close), settings_event_handler, NULL);
  lv_obj_align(btn_close_obj, LV_ALIGN_BOTTOM_RIGHT, 0, -8);
}

static void settings_event_handler(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *tgt = (lv_obj_t *)lv_event_get_target(e);

  if (tgt == unit_switch && code == LV_EVENT_VALUE_CHANGED) {
    use_fahrenheit = lv_obj_has_state(unit_switch, LV_STATE_CHECKED);
  }

  if (tgt == btn_close_obj && code == LV_EVENT_CLICKED) {
    // Save temperature unit preference
    prefs.putBool("useFahrenheit", use_fahrenheit);

    // Hide keyboard
    lv_keyboard_set_textarea(kb, nullptr);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    // Close settings window
    lv_obj_del(settings_win);
    settings_win = nullptr;

    // Update weather display with new settings
    fetch_and_update_weather();
  }
}

// Language change event handler
static void language_changed_cb(lv_event_t *e) {
  uint16_t selected = lv_dropdown_get_selected(lang_dropdown);
  if (selected < LANG_COUNT && selected != current_language) {
    current_language = (Language)selected;
    prefs.putUInt("language", current_language);
    
    // Update UI with new language
    update_ui_language();
  }
}

void do_geocode_query(const char *q) {
  geoDoc.clear();
  String url = String("https://geocoding-api.open-meteo.com/v1/search?name=") + urlencode(q) + "&count=15";

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DeserializationError err = deserializeJson(geoDoc, payload);
    
    if (!err) {
      geoResults = geoDoc["results"].as<JsonArray>();
      populate_results_dropdown();
    }
  }
  http.end();
}

void fetch_and_update_weather() {
  if (WiFi.status() != WL_CONNECTED) return;

  String url = String("http://api.open-meteo.com/v1/forecast?latitude=")
               + latitude + "&longitude=" + longitude
               + "&current=temperature_2m,apparent_temperature,is_day,weather_code"
               + "&daily=temperature_2m_min,temperature_2m_max,weather_code"
               + "&hourly=temperature_2m,precipitation_probability,weather_code"
               + "&forecast_hours=7"
               + "&timezone=auto";

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(32 * 1024);

    DeserializationError err = deserializeJson(doc, payload);
    if (err == DeserializationError::Ok) {
      // Extract and convert current temperature data
      float t_now = doc["current"]["temperature_2m"].as<float>();
      float t_ap = doc["current"]["apparent_temperature"].as<float>();
      int code_now = doc["current"]["weather_code"].as<int>();
      int is_day = doc["current"]["is_day"].as<int>();

      // Convert temperatures if needed
      t_now = convert_temp(t_now);
      t_ap = convert_temp(t_ap);

      // Update time from NTP server
      int utc_offset_seconds = doc["utc_offset_seconds"].as<int>();
      configTime(utc_offset_seconds, 0, "pool.ntp.org", "time.nist.gov");
      
      // Update current weather display
      char unit = use_fahrenheit ? 'F' : 'C';
      lv_label_set_text_fmt(lbl_today_temp, "%.0f°%c", t_now, unit);
      lv_label_set_text_fmt(lbl_today_feels_like, "%s %.0f°%c", tr(tr_feels_like), t_ap, unit);
      lv_img_set_src(img_today_icon, choose_image(code_now, is_day));

      // Update daily forecast
      JsonArray times = doc["daily"]["time"].as<JsonArray>();
      JsonArray tmin = doc["daily"]["temperature_2m_min"].as<JsonArray>();
      JsonArray tmax = doc["daily"]["temperature_2m_max"].as<JsonArray>();
      JsonArray weather_codes = doc["daily"]["weather_code"].as<JsonArray>();

      for (int i = 0; i < 7; i++) {
        // Parse date and get day of week
        const char *date = times[i];
        int year = atoi(date + 0);
        int mon = atoi(date + 5);
        int dayd = atoi(date + 8);
        int dow = day_of_week(year, mon, dayd);
        const char *dayStr = (i == 0) ? tr(tr_today) : get_weekday(dow);

        // Convert temperatures
        float mn = convert_temp(tmin[i].as<float>());
        float mx = convert_temp(tmax[i].as<float>());

        // Update UI
        lv_label_set_text_fmt(lbl_daily_day[i], "%s", dayStr);
        lv_label_set_text_fmt(lbl_daily_high[i], "%.0f°%c", mx, unit);
        lv_label_set_text_fmt(lbl_daily_low[i], "%.0f°%c", mn, unit);
        lv_img_set_src(img_daily[i], choose_icon(weather_codes[i].as<int>(), is_day));
      }

      // Update hourly forecast
      JsonArray hours = doc["hourly"]["time"].as<JsonArray>();
      JsonArray hourly_temps = doc["hourly"]["temperature_2m"].as<JsonArray>();
      JsonArray precipitation_probabilities = doc["hourly"]["precipitation_probability"].as<JsonArray>();
      JsonArray hourly_weather_codes = doc["hourly"]["weather_code"].as<JsonArray>();

      for (int i = 0; i < 7; i++) {
        // Extract hour
        const char *date = hours[i];
        int hour = atoi(date + 11);
        String hour_name = hour_of_day(hour);

        // Get weather data
        float precipitation_probability = precipitation_probabilities[i].as<float>();
        float temp = convert_temp(hourly_temps[i].as<float>());

        // Update UI
        if (i == 0) {
          lv_label_set_text(lbl_hourly[i], tr(tr_now));
        } else {
          lv_label_set_text(lbl_hourly[i], hour_name.c_str());
        }
        lv_label_set_text_fmt(lbl_precipitation_probability[i], "%.0f%%", precipitation_probability);
        lv_label_set_text_fmt(lbl_hourly_temp[i], "%.0f°%c", temp, unit);
        lv_img_set_src(img_hourly[i], choose_icon(hourly_weather_codes[i].as<int>(), is_day));
      }
    } else {
      Serial.print("JSON parse failed\n");
    }
  } else {
    Serial.println("HTTP GET failed");
  }
  http.end();
}

const lv_img_dsc_t* get_weather_image(int code, int is_day, ImageType type) {
  const lv_img_dsc_t* result = nullptr;
  
  switch (code) {
    // Clear sky
    case 0:
      result = is_day ? 
        (type == ICON ? &icon_sunny : &image_sunny) :
        (type == ICON ? &icon_clear_night : &image_clear_night);
      break;

    // Mainly clear
    case 1:
      result = is_day ? 
        (type == ICON ? &icon_mostly_sunny : &image_mostly_sunny) :
        (type == ICON ? &icon_mostly_clear_night : &image_mostly_clear_night);
      break;

    // Partly cloudy
    case 2:
      result = is_day ? 
        (type == ICON ? &icon_partly_cloudy : &image_partly_cloudy) :
        (type == ICON ? &icon_partly_cloudy_night : &image_partly_cloudy_night);
      break;

    // Overcast
    case 3:
      result = type == ICON ? &icon_cloudy : &image_cloudy;
      break;

    // Fog / mist
    case 45:
    case 48:
      result = type == ICON ? &icon_haze_fog_dust_smoke : &image_haze_fog_dust_smoke;
      break;

    // Drizzle (light → dense)
    case 51:
    case 53:
    case 55:
      result = type == ICON ? &icon_drizzle : &image_drizzle;
      break;

    // Freezing drizzle
    case 56:
    case 57:
      result = type == ICON ? &icon_sleet_hail : &image_sleet_hail;
      break;

    // Rain: slight showers
    case 61:
      result = is_day ? 
        (type == ICON ? &icon_scattered_showers_day : &image_scattered_showers_day) :
        (type == ICON ? &icon_scattered_showers_night : &image_scattered_showers_night);
      break;

    // Rain: moderate
    case 63:
      result = type == ICON ? &icon_showers_rain : &image_showers_rain;
      break;

    // Rain: heavy
    case 65:
      result = type == ICON ? &icon_heavy_rain : &image_heavy_rain;
      break;

    // Freezing rain
    case 66:
    case 67:
      result = type == ICON ? &icon_wintry_mix_rain_snow : &image_wintry_mix_rain_snow;
      break;

    // Snow fall (light, moderate, heavy) & snow showers (light)
    case 71:
    case 73:
    case 75:
    case 85:
      result = type == ICON ? &icon_snow_showers_snow : &image_snow_showers_snow;
      break;

    // Snow grains
    case 77:
      result = type == ICON ? &icon_flurries : &image_flurries;
      break;

    // Rain showers (slight → moderate)
    case 80:
    case 81:
      result = is_day ? 
        (type == ICON ? &icon_scattered_showers_day : &image_scattered_showers_day) :
        (type == ICON ? &icon_scattered_showers_night : &image_scattered_showers_night);
      break;

    // Rain showers: violent
    case 82:
      result = type == ICON ? &icon_heavy_rain : &image_heavy_rain;
      break;

    // Heavy snow showers
    case 86:
      result = type == ICON ? &icon_heavy_snow : &image_heavy_snow;
      break;

    // Thunderstorm (light)
    case 95:
      result = is_day ? 
        (type == ICON ? &icon_isolated_scattered_tstorms_day : &image_isolated_scattered_tstorms_day) :
        (type == ICON ? &icon_isolated_scattered_tstorms_night : &image_isolated_scattered_tstorms_night);
      break;

    // Thunderstorm with hail
    case 96:
    case 99:
      result = type == ICON ? &icon_strong_tstorms : &image_strong_tstorms;
      break;

    // Fallback for any other code
    default:
      result = is_day ? 
        (type == ICON ? &icon_mostly_cloudy_day : &image_mostly_cloudy_day) :
        (type == ICON ? &icon_mostly_cloudy_night : &image_mostly_cloudy_night);
      break;
  }
  
  return result;
}

const lv_img_dsc_t* choose_icon(int wmo_code, int is_day) {
  return get_weather_image(wmo_code, is_day, ICON);
}

const lv_img_dsc_t* choose_image(int wmo_code, int is_day) {
  return get_weather_image(wmo_code, is_day, LARGE_IMAGE);
}

// Update all UI elements with the current language
void update_ui_language() {
  // If settings window is open, recreate it
  if (settings_win) {
    lv_obj_del(settings_win);
    settings_win = nullptr;
    create_settings_window();
  }
  
  // If location dialog is open, recreate it
  if (location_win) {
    lv_obj_del(location_win);
    location_win = nullptr;
    create_location_dialog();
  }
  
  // Update forecast title
  if (lv_obj_has_flag(box_daily, LV_OBJ_FLAG_HIDDEN)) {
    lv_label_set_text(lbl_forecast, tr(tr_forecast_hourly));
  } else {
    lv_label_set_text(lbl_forecast, tr(tr_forecast_7day));
  }
  
  // Update "Feels Like" text
  lv_label_set_text_fmt(lbl_today_feels_like, "%s %.0f°%c", 
                        tr(tr_feels_like), 
                        lv_label_get_text(lbl_today_temp)[0] == '-' ? 0 : // If temp is not set yet
                          convert_temp(atof(lv_label_get_text(lbl_today_temp))), 
                        use_fahrenheit ? 'F' : 'C');
  
  // Update daily forecast day names
  for (int i = 0; i < 7; i++) {
    const char* txt = lv_label_get_text(lbl_daily_day[i]);
    if (strcmp(txt, "Today") == 0 || strcmp(txt, tr_today.pt_br) == 0 || 
        strcmp(txt, tr_today.es) == 0 || strcmp(txt, tr_today.fr) == 0) {
      lv_label_set_text(lbl_daily_day[i], tr(tr_today));
    } else {
      // For other days, we need to find the index in the weekdays array
      int day_idx = -1;
      for (int j = 0; j < 7; j++) {
        if (strcmp(txt, weekdays[j]) == 0 || 
            strcmp(txt, weekdays_pt_br[j]) == 0 || 
            strcmp(txt, weekdays_es[j]) == 0 || 
            strcmp(txt, weekdays_fr[j]) == 0) {
          day_idx = j;
          break;
        }
      }
      if (day_idx >= 0) {
        lv_label_set_text(lbl_daily_day[i], get_weekday(day_idx));
      }
    }
  }
  
  // Update hourly forecast "Now" text
  const char* txt = lv_label_get_text(lbl_hourly[0]);
  if (strcmp(txt, "Now") == 0 || strcmp(txt, tr_now.pt_br) == 0 || 
      strcmp(txt, tr_now.es) == 0 || strcmp(txt, tr_now.fr) == 0) {
    lv_label_set_text(lbl_hourly[0], tr(tr_now));
  }
}