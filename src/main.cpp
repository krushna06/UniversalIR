#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Preferences.h>
#include <U8g2lib.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <ir_Toshiba.h>
#include <IRac.h>

// ============================
// CONFIGURATION
// ============================
#define IR_LED_PIN     23
#define IR_RECV_PIN     4
#define OLED_SDA_PIN   21
#define OLED_SCL_PIN   22

const char* SSID = "";
const char* PASSWORD = "";

IRrecv irrecv(IR_RECV_PIN, 2600, 50, true);
decode_results results;
IRsend irsend(IR_LED_PIN);
IRToshibaAC ac(IR_LED_PIN);

WebServer server(80);
Preferences prefs;

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

void showOLED(String msg) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_7x14_tf);
  int tw = oled.getUTF8Width(msg.c_str());
  int x = max(0, (128 - tw) / 2);
  oled.drawUTF8(x, 22, msg.c_str());
  oled.sendBuffer();
}

String css = R"(
<style>
body { font-family:Arial; background:#f0f0f0; text-align:center; }
.section { width:340px; background:white; margin:20px auto; padding:20px;
           border-radius:15px; box-shadow:0 0 10px #ccc; }
.btn { display:block; margin:10px auto; width:80%; padding:12px;
       background:#4A90E2; color:white; border-radius:8px; text-decoration:none; font-size:18px; }
.btn-red { background:#D9534F; }
.btn-green { background:#5CB85C; }
.btn-small { width:45%; display:inline-block; }
.title { font-size:22px; margin-bottom:10px; }
textarea { width:95%; height:100px; }
</style>
)";

String pageHome() {
  return css + R"(
  <div class='section'>
     <div class='title'>Universal Remote</div>
     <a class='btn' href='/ac'>Toshiba AC Remote</a>
     <a class='btn' href='/rgb'>RGB Strip Remote</a>
     <a class='btn' href='/learn'>Learn IR Button</a>
     <a class='btn' href='/learned'>My Learned Buttons</a>
  </div>
)";
}

String pageAC() {
  return css + R"(
  <div class='section'>
     <div class='title'>Toshiba AC</div>

     <a class='btn-green btn' href='/ac_on'>POWER ON</a>
     <a class='btn-red btn'   href='/ac_off'>POWER OFF</a>

     <a class='btn-small btn' href='/ac_temp_up'>TEMP +</a>
     <a class='btn-small btn' href='/ac_temp_down'>TEMP -</a>

     <a class='btn' href='/ac_mode'>MODE COOL</a>
     <a class='btn' href='/ac_fan'>FAN AUTO</a>
     <a class='btn' href='/ac_swing'>SWING</a>

     <a class='btn' style='background:#777' href='/'>Back</a>
  </div>
)";
}

String pageRGB() {
  return css + R"(
  <div class='section'>
    <div class='title'>RGB Strip Remote</div>

    <a class='btn' href='/rgb_on'>ON</a>
    <a class='btn-red btn' href='/rgb_off'>OFF</a>

    <a class='btn-small btn' href='/rgb_r'>RED</a>
    <a class='btn-small btn' href='/rgb_g'>GREEN</a>
    <a class='btn-small btn' href='/rgb_b'>BLUE</a>

    <a class='btn' href='/rgb_white'>WHITE</a>

    <a class='btn' href='/rgb_flash'>FLASH</a>
    <a class='btn' href='/rgb_strobe'>STROBE</a>
    <a class='btn' href='/rgb_fade'>FADE</a>
    <a class='btn' href='/rgb_smooth'>SMOOTH</a>

    <a class='btn' style='background:#777' href='/'>Back</a>
  </div>
)";
}

String pageLearn() {
  return css + R"(
  <div class='section'>
    <div class='title'>Learn IR Button</div>
    <p>Point your remote at the receiver<br>and press a button…</p>
    <a class='btn' href='/start_learn'>Start Learning</a>
    <a class='btn' style='background:#777' href='/'>Back</a>
  </div>
)";
}

String pageLearned() {
  String html = css + "<div class='section'><div class='title'>My Learned Buttons</div>";

  prefs.begin("irlearn", true);
  size_t count = prefs.getUInt("count", 0);
  prefs.end();

  if (count == 0) html += "<p>No saved buttons</p>";
  else {
    prefs.begin("irlearn", true);
    for (int i = 0; i < count; i++) {
      String key = "btn" + String(i);
      String name = prefs.getString((key + "_name").c_str(), "");
      html += "<p><b>" + name + "</b><br>";
      html += "<a class='btn-small btn' href='/send_learned?id=" + String(i) + "'>Send</a>";
      html += "<a class='btn-small btn-red btn' href='/delete_learned?id=" + String(i) + "'>Delete</a></p>";
    }
    prefs.end();
  }

  html += "<a class='btn' style='background:#777' href='/'>Back</a></div>";
  return html;
}

void startLearning() {
  showOLED("Learning...");
  irrecv.enableIRIn();
  irrecv.setTolerance(40);

  unsigned long start = millis();

  while (millis() - start < 8000) {
    if (irrecv.decode(&results)) {
      Serial.println(resultToHumanReadableBasic(&results));

      String rawData = "";
      for (uint16_t i = 1; i < results.rawlen; i++) {
        rawData += String(results.rawbuf[i] * kRawTick);
        if (i < results.rawlen - 1) rawData += ",";
      }

      irrecv.resume();
      showOLED("Captured");

      String html = css;
      html += "<div class='section'><div class='title'>New Code Captured</div>";
      html += "<p>Raw Signal:</p><textarea>" + rawData + "</textarea>";
      html += "<form action='/save_learned'>";
      html += "<input name='name' placeholder='Button Name'><br>";
      html += "<input type='hidden' name='code' value='" + rawData + "'>";
      html += "<input type='submit' class='btn' value='Save'>";
      html += "</form>";
      html += "<a class='btn' style='background:#777' href='/'>Cancel</a></div>";

      server.send(200, "text/html", html);
      return;
    }
  }

  server.send(200, "text/html", "<h1>Timeout - No IR detected</h1><a href='/'>Back</a>");
}

void saveLearned(String name, String code) {
  prefs.begin("irlearn", false);

  size_t count = prefs.getUInt("count", 0);
  prefs.putString(("btn" + String(count) + "_name").c_str(), name);
  prefs.putString(("btn" + String(count) + "_code").c_str(), code);
  prefs.putUInt("count", count + 1);

  prefs.end();

  showOLED("Saved");
  server.sendHeader("Location", "/learned");
  server.send(303);
}

void sendLearned(int id) {
  prefs.begin("irlearn", true);

  String key = "btn" + String(id);
  String name = prefs.getString((key + "_name").c_str(), "");
  String raw = prefs.getString((key + "_code").c_str(), "");

  prefs.end();

  showOLED(name);

  std::vector<uint16_t> pulses;
  String temp = "";
  for (size_t i = 0; i < raw.length(); i++) {
    if (raw[i] == ',') {
      pulses.push_back(temp.toInt());
      temp = "";
    } else temp += raw[i];
  }
  if (temp.length() > 0) pulses.push_back(temp.toInt());

  irsend.sendRaw(pulses.data(), pulses.size(), 38);

  server.sendHeader("Location", "/learned");
  server.send(303);
}

void deleteLearned(int id) {
  prefs.begin("irlearn", false);

  size_t count = prefs.getUInt("count", 0);

  for (int i = id; i < count - 1; i++) {
    prefs.putString(("btn" + String(i) + "_name").c_str(),
                    prefs.getString(("btn" + String(i + 1) + "_name").c_str(), ""));
    prefs.putString(("btn" + String(i) + "_code").c_str(),
                    prefs.getString(("btn" + String(i + 1) + "_code").c_str(), ""));
  }

  prefs.putUInt("count", count - 1);
  prefs.end();

  server.sendHeader("Location", "/learned");
  server.send(303);
}

void acSend(String msg) {
  showOLED(msg);
  Serial.println(ac.toString());
  ac.send();
}

void setupRoutes() {

  server.on("/", [](){ server.send(200, "text/html", pageHome()); });
  server.on("/ac", [](){ server.send(200, "text/html", pageAC()); });

  server.on("/ac_on", [](){ ac.on(); acSend("AC ON"); server.sendHeader("Location","/ac"); server.send(303); });
  server.on("/ac_off", [](){ ac.off(); acSend("AC OFF"); server.sendHeader("Location","/ac"); server.send(303); });

  server.on("/ac_temp_up", [](){ ac.setTemp(ac.getTemp()+1); acSend("TEMP +"); server.sendHeader("Location","/ac"); server.send(303); });
  server.on("/ac_temp_down", [](){ ac.setTemp(ac.getTemp()-1); acSend("TEMP -"); server.sendHeader("Location","/ac"); server.send(303); });

  server.on("/ac_mode", [](){ ac.setMode(kToshibaAcCool); acSend("MODE COOL"); server.sendHeader("Location","/ac"); server.send(303); });
  server.on("/ac_fan", [](){ ac.setFan(kToshibaAcFanAuto); acSend("FAN AUTO"); server.sendHeader("Location","/ac"); server.send(303); });
  server.on("/ac_swing", [](){ ac.setSwing(true); acSend("SWING"); server.sendHeader("Location","/ac"); server.send(303); });

  server.on("/rgb", [](){ server.send(200, "text/html", pageRGB()); });

  server.on("/rgb_on", [](){ irsend.sendNEC(0x00F7C03F,32); showOLED("RGB ON"); server.sendHeader("Location","/rgb"); server.send(303); });
  server.on("/rgb_off", [](){ irsend.sendNEC(0x00F740BF,32); showOLED("RGB OFF"); server.sendHeader("Location","/rgb"); server.send(303); });

  server.on("/rgb_r", [](){ irsend.sendNEC(0x00F720DF,32); showOLED("RED"); server.sendHeader("Location","/rgb"); server.send(303); });
  server.on("/rgb_g", [](){ irsend.sendNEC(0x00F7A05F,32); showOLED("GREEN"); server.sendHeader("Location","/rgb"); server.send(303); });
  server.on("/rgb_b", [](){ irsend.sendNEC(0x00F7609F,32); showOLED("BLUE"); server.sendHeader("Location","/rgb"); server.send(303); });

  server.on("/rgb_white", [](){ irsend.sendNEC(0x00F7E01F,32); showOLED("WHITE"); server.sendHeader("Location","/rgb"); server.send(303); });

  server.on("/rgb_flash", [](){ irsend.sendNEC(0x00F7D02F,32); showOLED("FLASH"); server.sendHeader("Location","/rgb"); server.send(303); });
  server.on("/rgb_strobe", [](){ irsend.sendNEC(0x00F7F00F,32); showOLED("STROBE"); server.sendHeader("Location","/rgb"); server.send(303); });
  server.on("/rgb_fade", [](){ irsend.sendNEC(0x00F7C837,32); showOLED("FADE"); server.sendHeader("Location","/rgb"); server.send(303); });
  server.on("/rgb_smooth", [](){ irsend.sendNEC(0x00F7E817,32); showOLED("SMOOTH"); server.sendHeader("Location","/rgb"); server.send(303); });

  server.on("/learn", [](){ server.send(200, "text/html", pageLearn()); });
  server.on("/start_learn", [](){ startLearning(); });

  server.on("/save_learned", [](){
    saveLearned(server.arg("name"), server.arg("code"));
  });

  server.on("/learned", [](){ server.send(200, "text/html", pageLearned()); });

  server.on("/send_learned", [](){
    sendLearned(server.arg("id").toInt());
  });

  server.on("/delete_learned", [](){
    deleteLearned(server.arg("id").toInt());
  });
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  oled.begin();
  showOLED("Booting...");

  irsend.begin();
  irrecv.enableIRIn();
  irrecv.setTolerance(40);

  ac.begin();
  ac.setMode(kToshibaAcCool);
  ac.setTemp(24);

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(200);

  showOLED("WiFi Ready");

  setupRoutes();
  server.begin();
}

void loop() {
  server.handleClient();
}