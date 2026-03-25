// ==========================================================
// PROGRAMA DE PRUEBA: COMUNICACION DIRECTA CON MODEM SIM808
// ==========================================================
// Este código es exclusivo para diagnosticar si el SIM808 
// funciona correctamente, tiene señal y lee tu chip.

#include <HardwareSerial.h>

// Definir pines para UART1 - Ajustar a pines de Heltec que utilices
#define RX_PIN 4
#define TX_PIN 5

HardwareSerial modem(1); 

void setup() {
  // Inicializamos el puerto Serial del PC
  Serial.begin(115200);
  delay(10);
  
  Serial.println("");
  Serial.println("===============================");
  Serial.println("   MODEM DIAGNOSTICO SIM808    ");
  Serial.println("===============================");
  Serial.println("Iniciando comunicacion... (9600 baud)");
  
  // Inicializamos la comunicacion con el Modem
  modem.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); 
  delay(1000);

  // Comando AT basico para probar conexion
  Serial.println("Enviando 'AT' automático al modulo...");
  modem.println("AT");
}

void loop() {
  // LECTURA DESDE MODEM HACIA COMPUTADORA
  if (modem.available()) {
    String res = modem.readString(); // Lee todo lo que llega
    Serial.print(res);
  }

  // LECTURA DESDE COMPUTADORA (Consola Arduino) HACIA MODEM
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n'); // Enviar comandos escribiendo en el IDE (con 'Ambos NL & CR')
    cmd.trim(); 
    if(cmd.length() > 0) {
      Serial.print(">> Enviando: ");
      Serial.println(cmd);
      modem.println(cmd); 
    }
  }
}
