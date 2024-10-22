#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

#ifdef __AVR__
 #include <avr/power.h>
#endif
#ifndef PSTR
 #define PSTR
#endif

const char* ssid = "NodeMCU"; 
const char* password = "12345678";

IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

ESP8266WebServer server(80);

#define MATRIXPIN 4
#define lEarPIN 5
#define rEarPIN 0
#define EARNUMPIXELS 23

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, MATRIXPIN,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

Adafruit_NeoPixel lEar(EARNUMPIXELS, lEarPIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rEar(EARNUMPIXELS, rEarPIN, NEO_GRB + NEO_KHZ800);


unsigned long previousMillis = 0;
unsigned long previousMillisColorWipe = 0;
unsigned long previousMillisBreathing = 0;
int j = 0;
int i = 0;
int wipeIndex = 0;
int brightness = 0;
int fadeDirection = 1; // 1 to fade in, -1 to fade out

String stripSelectedEffect = "";
bool stripIsColor = true;
bool colorWipeSecondColor = false;

void redirToMain(){
  server.sendHeader("Location", "/", true); 
  server.send(302, "text/plain", "");
}

void stripSolidColor(uint32_t color){}
void stripRainbowCycle(int wait) {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= wait) {
    previousMillis = currentMillis;

    lEar.setPixelColor(i, Wheel(((i * 256 / EARNUMPIXELS) + j) & 255));
    rEar.setPixelColor(i, Wheel(((i * 256 / EARNUMPIXELS) + j) & 255));
    i++;
    if (i >= EARNUMPIXELS) {
      i = 0;  // Reset i to start again
      j++;    // Keep j incrementing for continuous rainbow effect
    }

    lEar.show();
    rEar.show();
  }
}

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return rEar.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return rEar.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return rEar.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}


void stripRandColorWipe(int wait) {
  unsigned long currentMillis = millis();
  uint8_t r = random(0, 256);
  uint8_t g = random(0, 256);
  uint8_t b = random(0, 256);
  uint32_t color = rEar.Color(r, g, b);
  if (currentMillis - previousMillisColorWipe >= wait) {
    previousMillisColorWipe = currentMillis;
    
    rEar.setPixelColor(wipeIndex, color);
    rEar.show();
    lEar.setPixelColor(wipeIndex, color);
    lEar.show();
    
    wipeIndex++;
    if (wipeIndex-1 >= EARNUMPIXELS) {
      wipeIndex = 0;  // Reset wipeIndex after completing the wipe
      // Generate a new random color after a full cycle
      uint8_t r = random(0, 256);
      uint8_t g = random(0, 256);
      uint8_t b = random(0, 256);
      color = rEar.Color(r, g, b);
    }
  }
}

void stripColorWipe(uint32_t color1, uint32_t color2, int wait) {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillisColorWipe >= wait) {
    previousMillisColorWipe = currentMillis;
    if (colorWipeSecondColor){rEar.setPixelColor(wipeIndex, color2);lEar.setPixelColor(wipeIndex, color2);}
    else {rEar.setPixelColor(wipeIndex, color1);lEar.setPixelColor(wipeIndex, color1);}
    
    lEar.show();
    rEar.show();
    
    wipeIndex++;
    if (wipeIndex-1 >= EARNUMPIXELS) {
      wipeIndex = 0; 
      colorWipeSecondColor = !colorWipeSecondColor;
    }
  }
}


void stripBreathingEffect(uint32_t color, int fadeSpeed) {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillisBreathing >= fadeSpeed) {
    previousMillisBreathing = currentMillis;
 
    rEar.setBrightness(brightness);
    lEar.setBrightness(brightness);
    for (int i = 0; i < EARNUMPIXELS; i++) {
      rEar.setPixelColor(i, color);
      lEar.setPixelColor(i, color);
    }
    rEar.show();
    lEar.show();

    brightness += fadeDirection;

    if (brightness <= 0 || brightness >= 255) {
      fadeDirection = -fadeDirection;  // Reverse direction to continue the cycle
    }
  }
}


void handle_stripSetEffect() {
  stripSelectedEffect = server.arg("effect");
  stripIsColor = false;
  redirToMain();
}

void handle_stripSetColor() {
  String colorStr = server.arg("color");
  stripIsColor = true;
  uint32_t color = strtol(colorStr.substring(1).c_str(), NULL, 16);
  uint8_t red = (color >> 16) & 0xFF;
  uint8_t green = (color >> 8) & 0xFF;
  uint8_t blue = color & 0xFF;

  Serial.printf("Received color for strip: R=%d, G=%d, B=%d\n", red, green, blue);

  rEar.clear();
  lEar.clear();
  for (int i = 0; i < EARNUMPIXELS; i++) {
    rEar.setPixelColor(i, rEar.Color(red, green, blue));
    lEar.setPixelColor(i, rEar.Color(red, green, blue));
  }
  rEar.show();
  lEar.show();
  redirToMain();
}


void handle_matrixSetText() {
  String text = server.arg("text");
  String colorStr = server.arg("color");

  uint32_t color = strtol(colorStr.substring(1).c_str(), NULL, 16);
  uint8_t red = (color >> 16) & 0xFF;
  uint8_t green = (color >> 8) & 0xFF;
  uint8_t blue = color & 0xFF;

  Serial.printf("Received matrix text: %s, Color: #%02x%02x%02x\n", text.c_str(), red, green, blue);

  matrix.fillScreen(0);
  matrix.setCursor(0, 0);
  matrix.setTextColor(matrix.Color(red, green, blue));
  matrix.print(text);
  matrix.show();

  redirToMain(); 
}

void handle_OnConnect() {
  String htmlPage = "<h2>Helmet LED Control</h2>"
                    "<br><p>LED Strip Control</p>"
                    "<form action=\"/stripSetColor\" method=\"get\">"
                    "<label for=\"color\">Choose strip color:</label>"
                    "<input type=\"color\" id=\"color\" name=\"color\" value=\"#ff0000\">"
                    "<input type=\"submit\" value=\"Set Color\">"
                    "</form>"
                    "<form action=\"/stripSetEffect\" method=\"get\">"
                    "<label for=\"effect\">Choose strip effect:</label>"
                    "<select id=\"effect\" name=\"effect\">"
                    "<option value=\"rainbow\">Rainbow</option>"
                    "<option value=\"randColorWipe\">Rand Color Wipe</option>"
                    "<option value=\"colorWipe\">Color Wipe</option>"
                    "<option value=\"breathing\">Breathing</option>"
                    "</select>"
                    "<input type=\"submit\" value=\"Set Effect\">"
                    "</form>"
                    "<br><p>LED Matrix Control</p>"
                    "<form action=\"/matrixSetText\" method=\"get\">"
                    "<label for=\"text\">Enter matrix text:</label>"
                    "<input type=\"text\" id=\"text\" name=\"text\" value=\"Hello!\">"
                    "<label for=\"color\">Choose text color:</label>"
                    "<input type=\"color\" id=\"color\" name=\"color\" value=\"#00ff00\">"
                    "<input type=\"submit\" value=\"Set Matrix Text & Color\">"
                    "</form>";
  server.send(200, "text/html", htmlPage);
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100); 
  
  server.on("/", handle_OnConnect);
  server.on("/matrixSetText", handle_matrixSetText);
  server.on("/stripSetEffect", handle_stripSetEffect);
  server.on("/stripSetColor", handle_stripSetColor);
  
  server.begin();
  Serial.println("HTTP server started");
  
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(40);
  Serial.println("Face initialized");
  
  lEar.begin();
  lEar.show();
  rEar.begin();
  rEar.show();
  Serial.println("Ears initialized");
}

void handleStripEffects(){
  if (stripSelectedEffect == "rainbow" ) {
    stripRainbowCycle(20);  // Non-blocking rainbow effect
  } 
  else if (stripSelectedEffect == "randColorWipe") {
    stripRandColorWipe(50);  // Non-blocking color wipe
  } 
  else if (stripSelectedEffect == "colorWipe") {
    stripColorWipe(rEar.Color(0, 0, 255), rEar.Color(0, 255, 0), 50);  // Non-blocking color wipe
  } 
  else if (stripSelectedEffect == "breathing") {
    stripBreathingEffect(rEar.Color(0, 0, 255), 20);  // Non-blocking breathing effect
  }
}
void loop() {
  server.handleClient();  // Keep handling web requests
  if (!stripIsColor) {handleStripEffects();}
}
