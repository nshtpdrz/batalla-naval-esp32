#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// --- CONFIGURACIÓN DE PINES ---
const int PinTrigger = 2; 
const int PinEcho = 4;    
const int PinBuzzer = 5;  
const int PinServo = 18;  

// --- CONFIGURACIÓN DE RED WI-FI LOCAL ---
const char* ssid = "Radar_Robotico_Grupo1";
const char* password = "12345678password";

WebServer server(80);
Servo miServo;

// --- PÁGINA WEB EN HTML ---
// Columnas (Números): 1=45°(Izq), 2=65°, 3=90°(Centro), 4=115°, 5=135°(Der)
// Filas (Letras): A=Cerca, B=Medio-Cerca, C=Medio, D=Medio-Lejos, E=Lejos
String paginaHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>Batalla Naval IoT</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; background-color: #1e293b; color: white; padding: 20px; }
        h1 { color: #38bdf8; }
        .grid { display: grid; grid-template-columns: repeat(5, 60px); gap: 10px; justify-content: center; margin-top: 20px; }
        .btn { width: 60px; height: 60px; font-size: 16px; font-weight: bold; background-color: #0f172a; color: #38bdf8; border: 2px solid #38bdf8; border-radius: 8px; cursor: pointer; }
        .btn:hover { background-color: #38bdf8; color: #0f172a; }
        .log { margin-top: 25px; font-size: 22px; color: #4ade80; font-weight: bold; }
    </style>
</head>
<body>
    <h1>🛰️ BATALLA NAVAL: RADAR SONAR</h1>
    <p>Selecciona una casilla para disparar el sonar:</p>
    
    <div class='grid'>
        <button class='btn' onclick='disparar(45, "A", 1)'>A1</button>
        <button class='btn' onclick='disparar(65, "A", 2)'>A2</button>
        <button class='btn' onclick='disparar(90, "A", 3)'>A3</button>
        <button class='btn' onclick='disparar(115, "A", 4)'>A4</button>
        <button class='btn' onclick='disparar(135, "A", 5)'>A5</button>
        
        <button class='btn' onclick='disparar(45, "B", 1)'>B1</button>
        <button class='btn' onclick='disparar(65, "B", 2)'>B2</button>
        <button class='btn' onclick='disparar(90, "B", 3)'>B3</button>
        <button class='btn' onclick='disparar(115, "B", 4)'>B4</button>
        <button class='btn' onclick='disparar(135, "B", 5)'>B5</button>
        
        <button class='btn' onclick='disparar(45, "C", 1)'>C1</button>
        <button class='btn' onclick='disparar(65, "C", 2)'>C2</button>
        <button class='btn' onclick='disparar(90, "C", 3)'>C3</button>
        <button class='btn' onclick='disparar(115, "C", 4)'>C4</button>
        <button class='btn' onclick='disparar(135, "C", 5)'>C5</button>
        
        <button class='btn' onclick='disparar(45, "D", 1)'>D1</button>
        <button class='btn' onclick='disparar(65, "D", 2)'>D2</button>
        <button class='btn' onclick='disparar(90, "D", 3)'>D3</button>
        <button class='btn' onclick='disparar(115, "D", 4)'>D4</button>
        <button class='btn' onclick='disparar(135, "D", 5)'>D5</button>
        
        <button class='btn' onclick='disparar(45, "E", 1)'>E1</button>
        <button class='btn' onclick='disparar(65, "E", 2)'>E2</button>
        <button class='btn' onclick='disparar(90, "E", 3)'>E3</button>
        <button class='btn' onclick='disparar(115, "E", 4)'>E4</button>
        <button class='btn' onclick='disparar(135, "E", 5)'>E5</button>
    </div>

    <div class='log' id='status'>Radar Listo. Esperando coordenadas...</div>

    <script>
        function disparar(angulo, letra, numero) {
            document.getElementById('status').style.color = '#eab308';
            document.getElementById('status').innerText = 'Apuntando radar a ' + letra + numero + '...';
            
            fetch('/radar?ang=' + angulo + '&letra=' + letra + '&num=' + numero)
                .then(response => response.text())
                .then(data => {
                    let statusDiv = document.getElementById('status');
                    statusDiv.innerText = data;
                    if(data.includes("💥")) {
                        statusDiv.style.color = '#ef4444';
                    } else {
                        statusDiv.style.color = '#38bdf8';
                    }
                });
        }
    </script>
</body>
</html>
)rawliteral";

// --- FUNCIÓN OPERATIVA ---
void manejarRadar() {
  if (server.hasArg("ang") && server.hasArg("letra") && server.hasArg("num")) {
    int anguloDestino = server.arg("ang").toInt();
    String letraObjetivo = server.arg("letra");
    int numeroObjetivo = server.arg("num").toInt();
    
    // 1. Mover servo según el NÚMERO (1=Izq, 3=Centro, 5=Der)
    miServo.attach(PinServo);
    delay(50);
    miServo.write(anguloDestino);
    delay(600); 
    miServo.detach(); 
    
    // 2. Medir distancia con el sensor ultrasónico
    digitalWrite(PinTrigger, LOW);
    delayMicroseconds(2);
    digitalWrite(PinTrigger, HIGH);
    delayMicroseconds(10);
    digitalWrite(PinTrigger, LOW);
    
    long tiempo = pulseIn(PinEcho, HIGH, 30000); 
    String respuesta = "";
    
    if (tiempo == 0) {
      respuesta = "⚠️ ERROR: Falso contacto en el sensor sonar.";
    } else {
      int distancia = tiempo * 0.0343 / 2;
      
      // Calibrar las LETRAS (profundidad en cm)
      String letraDetectada = "NINGUNA";
      if (distancia >= 6  && distancia <= 11)  letraDetectada = "A"; // Cerca
      if (distancia >= 12 && distancia <= 17) letraDetectada = "B";
      if (distancia >= 18 && distancia <= 23) letraDetectada = "C"; // Medio
      if (distancia >= 24 && distancia <= 29) letraDetectada = "D";
      if (distancia >= 30 && distancia <= 35) letraDetectada = "E"; // Lejos
      
      // 3. VALIDACIÓN DE IMPACTO (¿Coincide la letra medida con la letra presionada?)
      if (letraDetectada == letraObjetivo) {
        respuesta = "💥 ¡IMPACTO EN " + letraObjetivo + String(numeroObjetivo) + "! Objeto destruido a " + String(distancia) + " cm.";
        
        // Alarma de impacto
        for(int i = 1800; i > 600; i -= 120){
          tone(PinBuzzer, i);
          delay(25);
        }
        noTone(PinBuzzer);
      } else {
        if (letraDetectada != "NINGUNA") {
          respuesta = "🟢 AGUA... El sonar detectó un objeto en " + letraDetectada + String(numeroObjetivo) + " (Distancia: " + String(distancia) + " cm).";
        } else {
          respuesta = "🟢 AGUA... No hay objetivos en la línea del número " + String(numeroObjetivo) + ".";
        }
        
        // Sonido de fallo
        tone(PinBuzzer, 350);
        delay(200);
        noTone(PinBuzzer);
      }
    }
    
    server.send(200, "text/plain", respuesta);
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PinTrigger, OUTPUT);
  pinMode(PinEcho, INPUT);
  pinMode(PinBuzzer, OUTPUT);
  
  // Calibración inicial en el centro (Columna 3)
  miServo.attach(PinServo);
  miServo.write(90); 
  delay(600);
  miServo.detach();
  
  WiFi.softAP(ssid, password);
  Serial.println("\n--- RADAR DE COORDENADAS CONFIGURADO ---");
  Serial.print("Web: http://");
  Serial.println(WiFi.softAPIP());
  
  server.on("/", []() {
    server.send(200, "text/html", paginaHTML);
  });
  server.on("/radar", manejarRadar);
  
  server.begin();
}

void loop() {
  server.handleClient();
}