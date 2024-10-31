#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
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

AsyncWebServer server(80);

#define MATRIXPIN 14
#define lEarPIN 5
#define rEarPIN 4
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
int breathingBrightness = 0;
int fadeDirection = 1; // 1 to fade in, -1 to fade out

int earBrightness = 150;
int matrixBrightness = 40;

uint32_t colorWipe1 = rEar.Color(0, 255, 0);
uint32_t colorWipe2 = rEar.Color(255, 0, 0);

String stripSelectedEffect = "";
bool stripIsColor = true;
bool colorWipeSecondColor = false;

void redirToMain(AsyncWebServerRequest *request){
  request->redirect("/");
}

void stripSolidColor(uint32_t color){}

void stripRainbowCycle(int wait) {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= wait) {
        previousMillis = currentMillis;

        // Set each LED color based on its position and the current "j" offset
        for (int p = 0; p < EARNUMPIXELS; p++) {
            uint32_t color = Wheel(((p * 256 / EARNUMPIXELS) + j) & 255);
            lEar.setPixelColor(p, color);
            rEar.setPixelColor(p, color);
        }

        // Show updated colors on both ears
        lEar.show();
        rEar.show();

        // Increment j to move through the rainbow cycle
        j = (j + 1) % 256;  // Loop j back to 0 after it completes a cycle

        // Optionally, reset i if needed
        i = 0;
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
      wipeIndex = 0;
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
 
    rEar.setBrightness(breathingBrightness);
    lEar.setBrightness(breathingBrightness);
    for (int i = 0; i < EARNUMPIXELS; i++) {
      rEar.setPixelColor(i, color);
      lEar.setPixelColor(i, color);
    }
    rEar.show();
    lEar.show();

    breathingBrightness += fadeDirection;

    if (breathingBrightness <= 0 || breathingBrightness >= 255) {
      fadeDirection = -fadeDirection;  // Reverse direction to continue the cycle
    }
  }
}

void handle_stripSetEffect(AsyncWebServerRequest *request) {
    stripSelectedEffect = request->arg("effect");
    stripIsColor = false;

    if (stripSelectedEffect == "colorWipe") {
        // Retrieve selected colors for Color Wipe effect
        if (request->hasArg("colorWipe1") && request->hasArg("colorWipe2")) {
            uint32_t color1 = strtol(request->arg("colorWipe1").substring(1).c_str(), NULL, 16);
            uint32_t color2 = strtol(request->arg("colorWipe2").substring(1).c_str(), NULL, 16);
            
            // Extract RGB components for each color
            colorWipe1 = rEar.Color((color1 >> 16) & 0xFF, (color1 >> 8) & 0xFF, color1 & 0xFF);
            colorWipe2 = rEar.Color((color2 >> 16) & 0xFF, (color2 >> 8) & 0xFF, color2 & 0xFF);
        }
    }
    redirToMain(request);
}

void handle_stripSetColor(AsyncWebServerRequest *request) {
  String colorStr = request->arg("color");
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
  redirToMain(request);
}

void handle_matrixSetText(AsyncWebServerRequest *request) {
  String text = request->arg("text");
  String colorStr = request->arg("textColor");

  uint32_t color = strtol(colorStr.substring(1).c_str(), NULL, 16);
  uint8_t red = (color >> 16) & 0xFF;
  uint8_t green = (color >> 8) & 0xFF;
  uint8_t blue = color & 0xFF;

  Serial.printf("Received matrix text: %s, Color: R=%d, G=%d, B=%d\n", text.c_str(), (int)red, (int)green, (int)blue);


  matrix.fillScreen(0);
  matrix.setCursor(0, 0);
  matrix.setTextColor(matrix.Color(red, green, blue));
  matrix.print(text);
  matrix.show();

  redirToMain(request); 
}

void handle_setBrightness(AsyncWebServerRequest *request) {
    if (request->hasArg("stripBrightness")) {
        earBrightness = request->arg("stripBrightness").toInt();
        lEar.setBrightness(earBrightness);
        rEar.setBrightness(earBrightness);
        lEar.show();
        rEar.show();
    }

    if (request->hasArg("matrixBrightness")) {
        matrixBrightness = request->arg("matrixBrightness").toInt();
        matrix.setBrightness(matrixBrightness);
        matrix.show();
    }

    Serial.printf("Strip Brightness: %d, Matrix Brightness: %d\n", earBrightness, matrixBrightness);
    redirToMain(request);
}

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Helmet LED Control</title>
    <style>
        body {
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
            background-color: #f0f0f0;
            font-family: Arial, sans-serif;
        }

        .container {
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px;
            width: 300px;
        }

        h2 {
            color: #4a90e2;
        }

        p {
            color: #333333;
        }

        .form-section {
            background-color: #e0f7fa;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 15px;
            width: 100%;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
        }

        label {
            margin: 10px 0 5px;
            display: block;
        }

        .option-group {
            display: flex;
            justify-content: space-between;
            width: 100%;
            margin-bottom: 10px;
        }

        input[type="color"],
        input[type="text"],
        select {
            padding: 5px;
            border: 1px solid #ccc;
            border-radius: 4px;
            width: 48%;
            box-sizing: border-box;
        }

        input[type="submit"] {
            background-color: #4caf50;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 10px;
            cursor: pointer;
            width: 100%;
            box-sizing: border-box;
            margin-top: 10px;
        }

        input[type="submit"]:hover {
            background-color: #45a049;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>Helmet LED Control</h2>
        <p>LED Strip Control</p>
        <div class="form-section">
            
            <form action="/stripSetColor" method="get">
                <label for="color">Choose strip color:</label>
                <div class="option-group">
                    <input type="color" id="color" name="color" value="#00d5ff">
                </div>
                <input type="submit" value="Set Color">
            </form>
        </div>

        <div class="form-section">
          <form action="/stripSetEffect" method="get">
              <label for="effect">Choose strip effect:</label>
              <div class="option-group">
                  <select id="effect" name="effect" onchange="toggleColorWipeColors()">
                      <option value="rainbow">Rainbow</option>
                      <option value="randColorWipe">Rand Color Wipe</option>
                      <option value="colorWipe">Color Wipe</option>
                      <option value="breathing">Breathing</option>
                  </select>
              </div>
              
              <!-- Additional Color Inputs for Color Wipe -->
              <div id="colorWipeColors" style="display: none;">
                  <label for="colorWipe1">Color Wipe Color 1:</label>
                  <input type="color" id="colorWipe1" name="colorWipe1" value="#00ff00">
                  <label for="colorWipe2">Color Wipe Color 2:</label>
                  <input type="color" id="colorWipe2" name="colorWipe2" value="#ff0000">
              </div>
              
              <input type="submit" value="Set Effect">
          </form>
      </div>

      <script>
          function toggleColorWipeColors() {
              const effectSelect = document.getElementById("effect");
              const colorWipeColors = document.getElementById("colorWipeColors");
              if (effectSelect.value === "colorWipe") {
                  colorWipeColors.style.display = "block";
              } else {
                  colorWipeColors.style.display = "none";
              }
          }
      </script>
        <p>LED Matrix Control</p>
        <div class="form-section">
            
            <form action="/matrixSetText" method="get">
                <label for="text">Custom matrix text:</label>
                <div class="option-group">
                    <input type="text" id="text" name="text" placeholder="text">
                    <input type="color" id="textColor" name="textColor" value="#00d5ff">
                </div>
                <input type="submit" value="Set Matrix Text & Color">
            </form>
        </div>

        <div class="form-section">
            <form action="/matrixSetText" method="get">
                <label for="presetText">Choose matrix text preset:</label>
                <div class="option-group">
                    <select id="text" name="text">
                        <option value="preset1">preset1</option>
                        <option value="preset2">preset2</option>
                        <option value="preset3">preset3</option>
                        <option value="preset4">preset4</option>
                    </select>
                    <input type="color" id="textColor" name="textColor" value="#00d5ff">
                </div>
                <input type="submit" value="Set Matrix Preset & Color">
            </form>
        </div>
        <div class="form-section">
    <h3>Brightness Controls</h3>

    
    <form action="/setBrightness" method="get">
        <label for="stripBrightness">Strip Brightness:</label>
        <input type="range" id="stripBrightness" name="stripBrightness" min="0" max="255" value="150" oninput="this.nextElementSibling.value = this.value">
        <output>150</output>
        <br>

        
        <label for="matrixBrightness">Matrix Brightness:</label>
        <input type="range" id="matrixBrightness" name="matrixBrightness" min="0" max="255" value="40" oninput="this.nextElementSibling.value = this.value">
        <output>40</output>
        <br>
        
        <input type="submit" value="Set Brightness">
    </form>
</div>

    </div>
</body>
</html>

  )rawliteral";

void handle_OnConnect(AsyncWebServerRequest *request) {
  request->send(200, "text/html", FPSTR(htmlPage));
}

void setup() {
  Serial.begin(115200);
  ESP.eraseConfig();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password, 6, false, 2);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100); 
  
  server.on("/", HTTP_GET, handle_OnConnect);
  server.on("/matrixSetText", HTTP_GET, handle_matrixSetText);
  server.on("/stripSetEffect", HTTP_GET, handle_stripSetEffect);
  server.on("/stripSetColor", HTTP_GET, handle_stripSetColor);
  server.on("/setBrightness", HTTP_GET, handle_setBrightness);

  
  server.begin();
  Serial.println("HTTP server started");
  
  matrix.begin();
  matrix.setTextWrap(false);
  
  Serial.println("Face initialized");
  
  lEar.begin();
  lEar.show();
  rEar.begin();
  rEar.show();
  Serial.println("Ears initialized");
}

void handleStripEffects() {
  if (stripSelectedEffect == "rainbow") {
    stripRainbowCycle(20);
    rEar.setBrightness(earBrightness);
    lEar.setBrightness(earBrightness);
  } else if (stripSelectedEffect == "randColorWipe") {
    stripRandColorWipe(50);
    rEar.setBrightness(earBrightness);
    lEar.setBrightness(earBrightness);
  } else if (stripSelectedEffect == "colorWipe") {
    stripColorWipe(colorWipe1, colorWipe2, 50);
    rEar.setBrightness(earBrightness);
    lEar.setBrightness(earBrightness);
  } else if (stripSelectedEffect == "breathing") {
    stripBreathingEffect(rEar.Color(0, 0, 255), 40);
  }
}

void loop() {
  if (!stripIsColor) {
    handleStripEffects();
  }
  matrix.setBrightness(matrixBrightness);
}
