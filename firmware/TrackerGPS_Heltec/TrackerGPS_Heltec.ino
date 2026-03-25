#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <esp_bt.h>

// -------------------------------------------------------------------
// 1. CONFIGURACION Y EEPROM (Preferences)
// -------------------------------------------------------------------
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;

const byte DNS_PORT = 53;
const char* AP_SSID = "Heltec_GPS_Config";
bool isAPMode = false;
bool shouldCloseAP = false;
unsigned long closeAPTime = 0;

struct AppConfig {
  String apn;
  String simUser;
  String simPass;
  String deviceName;
  String apiUrl;
  bool isConfigured;
} configData;

void loadConfig() {
  preferences.begin("tracker_cfg", false);
  configData.apn = preferences.getString("apn", "");
  configData.simUser = preferences.getString("simUser", "");
  configData.simPass = preferences.getString("simPass", "");
  configData.deviceName = preferences.getString("devName", "GPS_Device_01");
  configData.apiUrl = preferences.getString("apiUrl", "http://tu-api.com/api/track");
  configData.isConfigured = preferences.getBool("configured", false);
  preferences.end();
}

void saveConfig(String apn, String user, String pass, String name, String url) {
  preferences.begin("tracker_cfg", false);
  preferences.putString("apn", apn);
  preferences.putString("simUser", user);
  preferences.putString("simPass", pass);
  preferences.putString("devName", name);
  preferences.putString("apiUrl", url);
  preferences.putBool("configured", true);
  preferences.end();
  loadConfig();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0f172a; color: #fff; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }";
  html += ".card { background: rgba(255, 255, 255, 0.05); backdrop-filter: blur(10px); padding: 30px; border-radius: 15px; border: 1px solid rgba(255,255,255,0.1); width: 90%; max-width: 400px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }";
  html += "h1 { text-align: center; color: #00d2ff; margin-top: 0; font-size: 1.5rem; }";
  html += "label { display: block; margin-top: 15px; font-size: 0.9rem; color: #94a3b8; }";
  html += "input[type=text], input[type=password] { width: 100%; box-sizing: border-box; padding: 12px; margin-top: 5px; background: rgba(0,0,0,0.2); border: 1px solid rgba(255,255,255,0.2); border-radius: 8px; color: #fff; font-size: 1rem; outline: none; transition: 0.3s; }";
  html += "input[type=text]:focus, input[type=password]:focus { border-color: #00d2ff; background: rgba(0,0,0,0.4); }";
  html += "input[type=submit] { margin-top: 25px; width: 100%; padding: 14px; background: linear-gradient(135deg, #3a7bd5, #00d2ff); color: white; border: none; border-radius: 8px; font-size: 1.1rem; font-weight: bold; cursor: pointer; transition: 0.3s; }";
  html += "input[type=submit]:hover { opacity: 0.9; transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,210,255,0.3); }";
  html += "</style></head><body>";
  html += "<div class='card'><h1>Tracker Config</h1>";
  html += "<form action='/save' method='POST'>";
  html += "<label>Nombre del Vehículo:</label><input type='text' name='devName' value='" + configData.deviceName + "'>";
  html += "<label>URL del Servidor API:</label><input type='text' name='apiUrl' value='" + configData.apiUrl + "'>";
  html += "<label>APN Celular:</label><input type='text' name='apn' value='" + configData.apn + "'>";
  html += "<label>Usuario APN (opcional):</label><input type='text' name='simUser' value='" + configData.simUser + "'>";
  html += "<label>Contrasena APN (opcional):</label><input type='text' name='simPass' value='" + configData.simPass + "'>";
  html += "<input type='submit' value='Guardar e Iniciar'>";
  html += "</form></div></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  saveConfig(server.arg("apn"), server.arg("simUser"), server.arg("simPass"), server.arg("devName"), server.arg("apiUrl"));
  server.send(200, "text/html", "<html><body><h2>Guardado!</h2><p>La red WiFi del dispositivo se apagara en unos segundos para ahorrar energia...</p></body></html>");
  shouldCloseAP = true;
  closeAPTime = millis();
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound([]() {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });
  server.begin();
  isAPMode = true;
}

void stopAPMode() {
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  isAPMode = false;
}

// -------------------------------------------------------------------
// 2. DISPLAY ESTADO Y UI
// -------------------------------------------------------------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ 21, /* clock=*/ 18, /* data=*/ 17);

enum ScreenState { SCREEN_BOOT, SCREEN_NO_CONFIG, SCREEN_AP_MODE, SCREEN_GENERAL, SCREEN_GPS, SCREEN_GSM };
ScreenState currentScreen = SCREEN_BOOT;

bool isScreenOn = true;

struct DisplayData {
  float batVolts = 0.0;
  int batPercent = 0;
  bool gpsValid = false;
  int gpsSats = 0;
  float lat = 0.0, lng = 0.0, speed = 0.0;
  int gsmSignal = 0;
  bool dbConnected = false;
  bool dataSentOk = false;
  String gsmStatus = "Iniciando";
} dispData;

void initDisplay() {
  pinMode(36, OUTPUT); digitalWrite(36, LOW); // Encender Vext display en Heltec
  delay(50);
  u8g2.begin(); u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr); u8g2.drawStr(20, 30, "Tracker GPS");
  u8g2.setFont(u8g2_font_ncenB08_tr); u8g2.drawStr(30, 50, "Iniciando...");
  u8g2.sendBuffer();
}

void setScreenPower(bool state) {
  if (isScreenOn == state) return;
  isScreenOn = state;
  if (!state) {
    u8g2.clearBuffer();
    u8g2.sendBuffer(); // Fuerzo pantalla negra para clones que ignoran PowerSave
  }
  u8g2.setPowerSave(state ? 0 : 1);
}

void drawBattery(int x, int y) {
  u8g2.drawFrame(x, y, 20, 10); u8g2.drawBox(x+20, y+3, 3, 4);
  int w = map(dispData.batPercent, 0, 100, 0, 18);
  u8g2.drawBox(x+1, y+1, w, 8);
}

void drawSignalBars(int x, int y, int percentage) {
  int bars = map(percentage, 0, 100, 0, 5); // 0 to 4 bars
  for(int i=0; i<4; i++) {
    int h = 3 + (i*2);  
    if(i < bars) u8g2.drawBox(x + (i*4), y + (9-h), 3, h);
    else u8g2.drawFrame(x + (i*4), y + (9-h), 3, h);
  }
}

void updateDisplay() {
  if (!isScreenOn) return; // No gasta tiempo de CPU si esta apagada
  
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_helvR08_tf);
  
  if (currentScreen == SCREEN_BOOT || currentScreen == SCREEN_NO_CONFIG || currentScreen == SCREEN_AP_MODE) {
    u8g2.setFont(u8g2_font_ncenB10_tr);
    if(currentScreen == SCREEN_BOOT) u8g2.drawStr(15, 30, "Iniciando...");
    else if(currentScreen == SCREEN_NO_CONFIG) { u8g2.drawStr(10, 20, "Sin Configurar!"); u8g2.setFont(u8g2_font_helvR08_tf); u8g2.drawStr(5, 40, "Manten PRG 4s presionado"); }
    else { u8g2.drawStr(20, 20, "Modo APP WiFi"); u8g2.setFont(u8g2_font_helvR08_tf); u8g2.drawStr(0, 40, "Red: Heltec_GPS_Config"); }
  } else {
    u8g2.setCursor(0, 10); u8g2.print(dispData.gpsValid ? "GPS Ok" : "No GPS");
    drawSignalBars(65, 3, dispData.gsmSignal); 
    u8g2.setCursor(85, 12); u8g2.print(dispData.gsmSignal); u8g2.print("%");
    drawBattery(106, 3); u8g2.drawLine(0, 15, 128, 15);
    
    if (currentScreen == SCREEN_GENERAL) {
      u8g2.setFont(u8g2_font_helvB10_tf); u8g2.setCursor(0, 30); u8g2.print("Vel: "); u8g2.print(dispData.speed, 1); u8g2.print(" km/h");
      u8g2.setFont(u8g2_font_helvR08_tf); u8g2.setCursor(0, 45); u8g2.print("Lat: "); u8g2.print(dispData.lat, 6);
      u8g2.setCursor(0, 58); u8g2.print("Lon: "); u8g2.print(dispData.lng, 6);
    }
    else if (currentScreen == SCREEN_GPS) {
      u8g2.setFont(u8g2_font_helvB08_tf); u8g2.drawStr(0, 25, "Datos GPS");
      u8g2.setFont(u8g2_font_helvR08_tf); u8g2.setCursor(0, 40); u8g2.print("Sats: "); u8g2.print(dispData.gpsSats);
      u8g2.setCursor(0, 55); u8g2.print(dispData.gpsValid ? "Senal Fijada OK" : "Buscando senal...");
    }
    else if (currentScreen == SCREEN_GSM) {
      u8g2.setFont(u8g2_font_helvB08_tf); u8g2.drawStr(0, 25, "Estado Conexion Web");
      u8g2.setFont(u8g2_font_helvR08_tf); u8g2.setCursor(0, 40); u8g2.print("API: "); u8g2.print(dispData.dbConnected ? "Ok" : "Fallo");
      u8g2.setCursor(0, 55); u8g2.print("Status: "); u8g2.print(dispData.gsmStatus);
    }
    
    u8g2.drawBox(currentScreen == SCREEN_GENERAL ? 44 : 45, 61, currentScreen == SCREEN_GENERAL ? 6 : 4, currentScreen == SCREEN_GENERAL ? 6 : 4);
    u8g2.drawBox(currentScreen == SCREEN_GPS ? 62 : 63, 61, currentScreen == SCREEN_GPS ? 6 : 4, currentScreen == SCREEN_GPS ? 6 : 4);
    u8g2.drawBox(currentScreen == SCREEN_GSM ? 80 : 81, 61, currentScreen == SCREEN_GSM ? 6 : 4, currentScreen == SCREEN_GSM ? 6 : 4);
  }
  u8g2.sendBuffer();
}

// -------------------------------------------------------------------
// 3. COMUNICACION SIM808
// -------------------------------------------------------------------
HardwareSerial modem(1); // UART1 (RX=4, TX=5)
unsigned long lastModemCheck = 0;

String sendAT(String cmd, uint32_t waitMs = 1000) {
  modem.println(cmd); String res = "";
  unsigned long s = millis();
  while(millis() - s < waitMs) { if(modem.available()) res += (char)modem.read(); }
  return res;
}

void initModem() {
  modem.begin(9600, SERIAL_8N1, 4, 5); delay(1000);
  
  // Probar conexion basica del hardware
  String test = sendAT("AT", 1000);
  if (test.indexOf("OK") == -1) {
    dispData.gsmStatus = "Modulo Desconectado";
    dispData.dbConnected = false;
    return; // Evita seguir mandando comandos si no hay modulo
  }

  modem.println("AT+CGNSPWR=1"); delay(100);
  modem.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\""); delay(100);
}

void initGPRS(String apn, String user, String pass) {
  if (dispData.gsmStatus == "Modulo Desconectado") return; // Freno seguridad
  
  dispData.gsmStatus = "Conectando red...";
  sendAT("AT+SAPBR=3,1,\"APN\",\"" + apn + "\"");
  sendAT("AT+SAPBR=1,1", 3000); 
  String ip = sendAT("AT+SAPBR=2,1");
  
  // Condicion ESTRICTA: la cadena debe tener el prefijo valido y NO ser nula (0.0.0.0)
  if(ip.indexOf("+SAPBR: 1,1,\"") != -1 && ip.indexOf("0.0.0.0") == -1) { 
    dispData.gsmStatus = "GPRS Ok"; 
    dispData.dbConnected = true; 
  } else { 
    dispData.gsmStatus = "Error GPRS o Sin SIM"; 
    dispData.dbConnected = false; 
  }
}

void checkGSM() {
  String res = sendAT("AT+CSQ", 500);
  int idx = res.indexOf("+CSQ: ");
  if (idx != -1) {
    int comma = res.indexOf(',', idx);
    int csq = res.substring(idx + 6, comma).toInt();
    if (csq == 99 || csq == 0) dispData.gsmSignal = 0;
    else dispData.gsmSignal = constrain(map(csq, 0, 31, 0, 100), 0, 100);
  }
}

void checkGPS() {
  String res = sendAT("AT+CGNSINF", 500);
  if(res.indexOf("+CGNSINF: 1,1") != -1) {
    int c[15]; int idx = 0;
    for(int i=0; i<15; i++) { idx = res.indexOf(',', idx+1); c[i] = idx; }
    if(c[3] > 0 && c[4] > 0) {
      dispData.lat = res.substring(c[3]+1, c[4]).toFloat();
      dispData.lng = res.substring(c[4]+1, c[5]).toFloat();
      dispData.speed = res.substring(c[6]+1, c[7]).toFloat() * 1.852;
      dispData.gpsValid = true; dispData.gpsSats = 6;
    }
  } else { dispData.gpsValid = false; }
}

void sendDataToAPI(String devName, String url) {
  if (!dispData.dbConnected || !dispData.gpsValid || url.length() < 5) return;
  dispData.gsmStatus = "Enviando...";
  sendAT("AT+HTTPINIT"); sendAT("AT+HTTPPARA=\"CID\",1"); 
  sendAT("AT+HTTPPARA=\"URL\",\"" + url + "\"");
  sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  String json = "{\"name\":\"" + devName + "\",\"lat\":" + String(dispData.lat, 6) + ",\"lng\":" + String(dispData.lng, 6) + ",\"speed\":" + String(dispData.speed, 2) + "}";
  sendAT("AT+HTTPDATA=" + String(json.length()) + ",10000"); sendAT(json);
  String act = sendAT("AT+HTTPACTION=1", 5000);
  if (act.indexOf("1,200") != -1) { dispData.gsmStatus = "Datos Enviados"; dispData.dataSentOk = true; }
  else { dispData.gsmStatus = "Error Envio API"; dispData.dataSentOk = false; }
  sendAT("AT+HTTPTERM");
}

// -------------------------------------------------------------------
// 4. MAIN LOOP Y CONTROL PRINCIPAL
// -------------------------------------------------------------------
const int BTN_PRG = 0;
const int BAT_PIN = 1;
unsigned long lastInteractionTime = 0;

void setup() {
  Serial.begin(115200); 
  
  // AHORRO DE ENERGIA DE INICIO:
  btStop(); // Apaga bluetooth completamente para ahorrar ~30mA
  WiFi.mode(WIFI_OFF); // Nos aseguramos que el WiFi este apagado
  
  pinMode(BTN_PRG, INPUT_PULLUP); 
  analogReadResolution(12);
  
  initDisplay(); loadConfig(); delay(1000);
  lastInteractionTime = millis();
  
  if (!configData.isConfigured) { currentScreen = SCREEN_NO_CONFIG; updateDisplay(); } 
  else { currentScreen = SCREEN_GENERAL; initModem(); initGPRS(configData.apn, configData.simUser, configData.simPass); updateDisplay(); }
}

void processButton() {
  static unsigned long btnPressTime = 0;
  static bool btnPrev = HIGH, isLongPressDone = false;
  bool btnState = digitalRead(BTN_PRG);
  
  if (btnState == LOW && btnPrev == HIGH) { 
    btnPressTime = millis(); 
    isLongPressDone = false; 
    lastInteractionTime = millis(); 
  }
  
  long heldTime = millis() - btnPressTime;
  
  if (btnState == LOW) {
    // Si se presiona mas de 4 segundos activa/desactiva portal WEB
    if (heldTime > 4000 && !isLongPressDone) {
      isLongPressDone = true;
      if (!isAPMode) { 
        currentScreen = SCREEN_AP_MODE; setScreenPower(true); startAPMode(); updateDisplay(); 
      } else { 
        stopAPMode(); currentScreen = configData.isConfigured ? SCREEN_GENERAL : SCREEN_NO_CONFIG; updateDisplay(); 
      }
    }
  }
  
  if (btnState == HIGH && btnPrev == LOW) {
    if (!isLongPressDone) {
      if (heldTime >= 2000) {
        // Presion media (2 segundos): Apagar pantalla manualmente / encenderla
        setScreenPower(!isScreenOn);
        if(isScreenOn) updateDisplay();
      } 
      else if (heldTime > 50) {
        // Presion Corta:
        if (!isScreenOn) {
          // Si la pantalla estaba apagada, un toque corto solo la enciende (no cambia de pagina)
          setScreenPower(true);
          updateDisplay();
        } else if (!isAPMode && configData.isConfigured) {
          // Cambiar de pantallas si ya estaba encendida
          if (currentScreen == SCREEN_GENERAL) currentScreen = SCREEN_GPS;
          else if (currentScreen == SCREEN_GPS) currentScreen = SCREEN_GSM;
          else if (currentScreen == SCREEN_GSM) currentScreen = SCREEN_GENERAL;
          updateDisplay();
        }
      }
    }
    lastInteractionTime = millis();
  }
  btnPrev = btnState;
}

void loop() {
  if (shouldCloseAP && millis() - closeAPTime > 2000) {
    stopAPMode();
    shouldCloseAP = false;
    currentScreen = configData.isConfigured ? SCREEN_GENERAL : SCREEN_NO_CONFIG;
    updateDisplay();
  }

  if(isAPMode) { 
    dnsServer.processNextRequest(); server.handleClient(); 
  }
  
  processButton();
  
  // Apagado automatico de la pantalla por inactividad (5 minutos = 300,000 ms)
  if (isScreenOn && millis() - lastInteractionTime > 300000UL) {
    setScreenPower(false);
  }
  
  if (isAPMode) return;
  
  if (configData.isConfigured) {
    if(millis() % 5000 < 50) { 
      float v = analogRead(BAT_PIN) / 4096.0 * 3.3 * 5.0; 
      dispData.batPercent = constrain(map(v*100, 320, 420, 0, 100),0,100); 
    }
    
    if (millis() - lastModemCheck > 10000) {
      lastModemCheck = millis();
      checkGSM();
      checkGPS();
      if(dispData.gpsValid) sendDataToAPI(configData.deviceName, configData.apiUrl);
    }
    
    static unsigned long lastUi = 0;
    if(millis() - lastUi > 1000) { lastUi = millis(); updateDisplay(); }
  }
}
