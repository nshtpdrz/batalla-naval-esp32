#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ======================================================
//  BATALLA NAVAL DIGITAL - 1 ESP32, 2 JUGADORES
//  El servo + buzzer son SOLO un efecto visual/sonoro.
//  El resultado (agua / impacto) lo decide el código,
//  NO un sensor ultrasónico real.
// ======================================================

// --- PINES ---
const int PinBuzzer = 5;
const int PinServo  = 18;
// (El sensor ultrasónico, si lo dejas conectado, es puramente
//  decorativo: este código nunca lo lee)

// --- WIFI LOCAL (Access Point) ---
const char* ssid     = "Radar_Robotico_Grupo2";
const char* password = "12345678password";

WebServer server(80);
Servo miServo;

// --- CONFIGURACIÓN DEL JUEGO ---
const int TOTAL_BARCOS = 5; // barcos de 1 casilla cada jugador

// Tableros: 1 = hay barco, 0 = vacío
int barcosJ1[5][5];
int barcosJ2[5][5];

// Disparos hechos por cada jugador sobre el tablero del otro
// 0 = sin disparar, 1 = agua, 2 = impacto
int disparosJ1[5][5]; // disparos de J1 sobre el tablero de J2
int disparosJ2[5][5]; // disparos de J2 sobre el tablero de J1

int barcosColocadosJ1 = 0;
int barcosColocadosJ2 = 0;
int barcosRestantesJ1 = TOTAL_BARCOS; // barcos de J1 que siguen sin hundir
int barcosRestantesJ2 = TOTAL_BARCOS;

// fase: 0 = colocando J1, 1 = colocando J2, 2 = jugando, 3 = terminado
int fase = 0;
int turno = 1;
int ganador = 0;

// ======================================================
//  PÁGINA WEB
// ======================================================
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
  h2 { color: #94a3b8; font-size: 16px; }
  .grid { display: grid; grid-template-columns: repeat(5, 55px); gap: 8px; justify-content: center; margin: 15px auto; }
  .btn { width: 55px; height: 55px; font-size: 14px; font-weight: bold; background-color: #0f172a; color: #38bdf8; border: 2px solid #38bdf8; border-radius: 8px; cursor: pointer; }
  .btn:hover { background-color: #38bdf8; color: #0f172a; }
  .barco { background-color: #38bdf8; color: #0f172a; }
  .agua { background-color: #0f172a; color: #64748b; border-color: #64748b; cursor: default; }
  .impacto { background-color: #ef4444; color: white; border-color: #ef4444; cursor: default; }
  .disabled { cursor: default; opacity: 0.6; }
  .log { margin-top: 15px; font-size: 20px; color: #4ade80; font-weight: bold; min-height: 30px; }
  .selector button { font-size: 18px; padding: 15px 25px; margin: 10px; border-radius: 8px; border: 2px solid #38bdf8; background: #0f172a; color: #38bdf8; cursor: pointer; }
  .oculto { display: none; }
  .seccion { margin-top: 25px; }
  #reiniciarBtn { margin-top: 20px; padding: 10px 20px; background: #ef4444; color: white; border: none; border-radius: 6px; cursor: pointer; font-weight: bold; }
</style>
</head>
<body>
  <h1>🛰️ BATALLA NAVAL: RADAR SONAR</h1>

  <div id='selector' class='selector'>
    <p>¿Quién eres?</p>
    <button onclick='elegirJugador(1)'>Jugador 1</button>
    <button onclick='elegirJugador(2)'>Jugador 2</button>
  </div>

  <div id='juego' class='oculto'>
    <div class='log' id='status'>Cargando...</div>

    <div id='zonaColocar' class='seccion oculto'>
      <h2>Coloca tus 5 barcos (toca las casillas)</h2>
      <div class='grid' id='gridColocar'></div>
      <p id='contadorBarcos'></p>
      <button class='btn' style='width:auto;padding:10px 20px;' id='btnListo' onclick='marcarListo()'>Listo</button>
    </div>

    <div id='zonaJuego' class='seccion oculto'>
      <h2>Tablero enemigo (dispara aquí)</h2>
      <div class='grid' id='gridEnemigo'></div>
      <h2>Tu tablero</h2>
      <div class='grid' id='gridPropio'></div>
    </div>

    <div id='zonaFin' class='seccion oculto'>
      <button id='reiniciarBtn' onclick='reiniciar()'>Jugar de nuevo</button>
    </div>
  </div>

<script>
  let miJugador = null;
  const LETRAS = ['A','B','C','D','E'];

  function elegirJugador(j) {
    miJugador = j;
    document.getElementById('selector').classList.add('oculto');
    document.getElementById('juego').classList.remove('oculto');
    actualizar();
    setInterval(actualizar, 1200);
  }

  function actualizar() {
    fetch('/estado?jugador=' + miJugador)
      .then(r => r.json())
      .then(renderizar);
  }

  function renderizar(d) {
    const status = document.getElementById('status');
    const zonaColocar = document.getElementById('zonaColocar');
    const zonaJuego = document.getElementById('zonaJuego');
    const zonaFin = document.getElementById('zonaFin');
    zonaColocar.classList.add('oculto');
    zonaJuego.classList.add('oculto');
    zonaFin.classList.add('oculto');

    if (d.fase === 0 || d.fase === 1) {
      const activo = d.fase === 0 ? 1 : 2;
      const colocados = d.fase === 0 ? d.barcosColocadosJ1 : d.barcosColocadosJ2;
      if (miJugador === activo) {
        zonaColocar.classList.remove('oculto');
        status.innerText = 'Coloca tus barcos';
        document.getElementById('contadorBarcos').innerText = colocados + ' / ' + d.totalBarcos + ' barcos colocados';
        document.getElementById('btnListo').disabled = colocados !== d.totalBarcos;
        dibujarGridColocar(d.miBarcos);
      } else {
        status.innerText = 'Esperando a que el Jugador ' + activo + ' coloque sus barcos...';
      }
    } else if (d.fase === 2) {
      zonaJuego.classList.remove('oculto');
      status.innerText = (d.turno === miJugador) ? '¡Tu turno! Dispara al tablero enemigo' : 'Turno del Jugador ' + d.turno + ', espera...';
      dibujarGridEnemigo(d.misDisparos, d.turno === miJugador);
      dibujarGridPropio(d.miBarcos, d.misImpactosRecibidos);
    } else if (d.fase === 3) {
      zonaFin.classList.remove('oculto');
      status.innerText = (d.ganador === miJugador) ? '🏆 ¡Ganaste la partida!' : '💀 El Jugador ' + d.ganador + ' ganó la partida';
    }
  }

  function dibujarGridColocar(miBarcos) {
    const grid = document.getElementById('gridColocar');
    grid.innerHTML = '';
    for (let f = 0; f < 5; f++) {
      for (let c = 0; c < 5; c++) {
        const btn = document.createElement('button');
        btn.className = 'btn' + (miBarcos[f][c] ? ' barco' : '');
        btn.innerText = LETRAS[f] + (c + 1);
        btn.onclick = () => colocar(f, c);
        grid.appendChild(btn);
      }
    }
  }

  function dibujarGridEnemigo(misDisparos, esMiTurno) {
    const grid = document.getElementById('gridEnemigo');
    grid.innerHTML = '';
    for (let f = 0; f < 5; f++) {
      for (let c = 0; c < 5; c++) {
        const val = misDisparos[f][c];
        const btn = document.createElement('button');
        let clase = 'btn';
        if (val === 1) clase += ' agua';
        if (val === 2) clase += ' impacto';
        if (!esMiTurno || val !== 0) clase += ' disabled';
        btn.className = clase;
        btn.innerText = val === 1 ? '🟢' : (val === 2 ? '💥' : LETRAS[f] + (c + 1));
        if (esMiTurno && val === 0) btn.onclick = () => disparar(f, c);
        grid.appendChild(btn);
      }
    }
  }

  function dibujarGridPropio(miBarcos, impactosRecibidos) {
    const grid = document.getElementById('gridPropio');
    grid.innerHTML = '';
    for (let f = 0; f < 5; f++) {
      for (let c = 0; c < 5; c++) {
        const golpe = impactosRecibidos[f][c];
        const btn = document.createElement('button');
        let clase = 'btn disabled';
        if (golpe === 2) clase += ' impacto';
        else if (golpe === 1) clase += ' agua';
        else if (miBarcos[f][c]) clase += ' barco';
        btn.className = clase;
        btn.innerText = LETRAS[f] + (c + 1);
        grid.appendChild(btn);
      }
    }
  }

  function colocar(f, c) {
    fetch('/colocar?jugador=' + miJugador + '&f=' + f + '&c=' + c).then(actualizar);
  }

  function marcarListo() {
    fetch('/listo?jugador=' + miJugador).then(actualizar);
  }

  function disparar(f, c) {
    fetch('/disparar?jugador=' + miJugador + '&f=' + f + '&c=' + c).then(actualizar);
  }

  function reiniciar() {
    fetch('/reiniciar').then(actualizar);
  }
</script>
</body>
</html>
)rawliteral";

// ======================================================
//  EFECTOS VISUALES / SONOROS (no afectan el resultado)
// ======================================================
void moverRadarVisual(int columna) {
  int angulos[5] = {45, 65, 90, 115, 135};
  miServo.attach(PinServo);
  delay(50);
  miServo.write(angulos[columna]);
  delay(500);
  miServo.detach();
}

void sonidoImpacto() {
  for (int i = 1800; i > 600; i -= 120) {
    tone(PinBuzzer, i);
    delay(25);
  }
  noTone(PinBuzzer);
}

void sonidoAgua() {
  tone(PinBuzzer, 350);
  delay(200);
  noTone(PinBuzzer);
}

// ======================================================
//  UTILIDAD: convertir un tablero 5x5 a JSON
// ======================================================
String gridToJSON(int grid[5][5]) {
  String s = "[";
  for (int i = 0; i < 5; i++) {
    s += "[";
    for (int j = 0; j < 5; j++) {
      s += String(grid[i][j]);
      if (j < 4) s += ",";
    }
    s += "]";
    if (i < 4) s += ",";
  }
  s += "]";
  return s;
}

// ======================================================
//  ENDPOINTS
// ======================================================
void manejarEstado() {
  int jugador = server.hasArg("jugador") ? server.arg("jugador").toInt() : 0;
  if (jugador != 1 && jugador != 2) {
    server.send(400, "text/plain", "jugador invalido");
    return;
  }

  String json = "{";
  json += "\"fase\":" + String(fase) + ",";
  json += "\"turno\":" + String(turno) + ",";
  json += "\"ganador\":" + String(ganador) + ",";
  json += "\"barcosColocadosJ1\":" + String(barcosColocadosJ1) + ",";
  json += "\"barcosColocadosJ2\":" + String(barcosColocadosJ2) + ",";
  json += "\"totalBarcos\":" + String(TOTAL_BARCOS) + ",";

  int (*misBarcos)[5] = (jugador == 1) ? barcosJ1 : barcosJ2;
  json += "\"miBarcos\":" + gridToJSON(misBarcos) + ",";

  if (fase == 2 || fase == 3) {
    int (*misImpactosRecibidos)[5] = (jugador == 1) ? disparosJ2 : disparosJ1;
    int (*misDisparos)[5] = (jugador == 1) ? disparosJ1 : disparosJ2;
    json += "\"misImpactosRecibidos\":" + gridToJSON(misImpactosRecibidos) + ",";
    json += "\"misDisparos\":" + gridToJSON(misDisparos);
  } else {
    json += "\"misImpactosRecibidos\":[],\"misDisparos\":[]";
  }
  json += "}";

  server.send(200, "application/json", json);
}

void manejarColocar() {
  if (!server.hasArg("jugador") || !server.hasArg("f") || !server.hasArg("c")) {
    server.send(400, "text/plain", "Faltan parametros");
    return;
  }
  int jugador = server.arg("jugador").toInt();
  int f = server.arg("f").toInt();
  int c = server.arg("c").toInt();
  if (f < 0 || f > 4 || c < 0 || c > 4) {
    server.send(400, "text/plain", "Coordenadas invalidas");
    return;
  }

  if (jugador == 1 && fase == 0) {
    if (barcosJ1[f][c] == 1) { barcosJ1[f][c] = 0; barcosColocadosJ1--; }
    else if (barcosColocadosJ1 < TOTAL_BARCOS) { barcosJ1[f][c] = 1; barcosColocadosJ1++; }
  } else if (jugador == 2 && fase == 1) {
    if (barcosJ2[f][c] == 1) { barcosJ2[f][c] = 0; barcosColocadosJ2--; }
    else if (barcosColocadosJ2 < TOTAL_BARCOS) { barcosJ2[f][c] = 1; barcosColocadosJ2++; }
  }
  server.send(200, "text/plain", "ok");
}

void manejarListo() {
  if (!server.hasArg("jugador")) {
    server.send(400, "text/plain", "Falta jugador");
    return;
  }
  int jugador = server.arg("jugador").toInt();
  if (jugador == 1 && fase == 0 && barcosColocadosJ1 == TOTAL_BARCOS) {
    fase = 1;
  } else if (jugador == 2 && fase == 1 && barcosColocadosJ2 == TOTAL_BARCOS) {
    fase = 2;
    turno = 1;
  }
  server.send(200, "text/plain", "ok");
}

void manejarDisparar() {
  if (!server.hasArg("jugador") || !server.hasArg("f") || !server.hasArg("c")) {
    server.send(400, "text/plain", "Faltan parametros");
    return;
  }
  int jugador = server.arg("jugador").toInt();
  int f = server.arg("f").toInt();
  int c = server.arg("c").toInt();
  if (f < 0 || f > 4 || c < 0 || c > 4) {
    server.send(400, "text/plain", "Coordenadas invalidas");
    return;
  }
  if (fase != 2) {
    server.send(400, "text/plain", "No es fase de juego");
    return;
  }
  if (jugador != turno) {
    server.send(400, "text/plain", "No es tu turno");
    return;
  }

  int (*tableroEnemigo)[5] = (jugador == 1) ? barcosJ2 : barcosJ1;
  int (*misDisparos)[5] = (jugador == 1) ? disparosJ1 : disparosJ2;

  if (misDisparos[f][c] != 0) {
    server.send(400, "text/plain", "Ya disparaste ahi");
    return;
  }

  // Efecto visual: el servo apunta hacia la columna disparada.
  // Esto NO influye en si es agua o impacto.
  moverRadarVisual(c);

  String resultado;
  if (tableroEnemigo[f][c] == 1) {
    misDisparos[f][c] = 2; // impacto
    sonidoImpacto();
    if (jugador == 1) {
      barcosRestantesJ2--;
      if (barcosRestantesJ2 <= 0) { fase = 3; ganador = 1; }
    } else {
      barcosRestantesJ1--;
      if (barcosRestantesJ1 <= 0) { fase = 3; ganador = 2; }
    }
    resultado = "impacto";
  } else {
    misDisparos[f][c] = 1; // agua
    sonidoAgua();
    resultado = "agua";
  }

  if (fase != 3) {
    turno = (jugador == 1) ? 2 : 1;
  }

  server.send(200, "text/plain", resultado);
}

void manejarReiniciar() {
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      barcosJ1[i][j] = 0;
      barcosJ2[i][j] = 0;
      disparosJ1[i][j] = 0;
      disparosJ2[i][j] = 0;
    }
  }
  barcosColocadosJ1 = 0;
  barcosColocadosJ2 = 0;
  barcosRestantesJ1 = TOTAL_BARCOS;
  barcosRestantesJ2 = TOTAL_BARCOS;
  fase = 0;
  turno = 1;
  ganador = 0;
  server.send(200, "text/plain", "reiniciado");
}

// ======================================================
//  SETUP / LOOP
// ======================================================
void setup() {
  Serial.begin(115200);

  pinMode(PinBuzzer, OUTPUT);

  // Calibración inicial del servo al centro
  miServo.attach(PinServo);
  miServo.write(90);
  delay(500);
  miServo.detach();

  WiFi.softAP(ssid, password);
  Serial.println("\n--- BATALLA NAVAL (2 JUGADORES, 1 ESP32) ---");
  Serial.print("Red WiFi: "); Serial.println(ssid);
  Serial.print("Contraseña: "); Serial.println(password);
  Serial.print("Abre en el navegador: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() {
    server.send(200, "text/html", paginaHTML);
  });
  server.on("/estado", manejarEstado);
  server.on("/colocar", manejarColocar);
  server.on("/listo", manejarListo);
  server.on("/disparar", manejarDisparar);
  server.on("/reiniciar", manejarReiniciar);

  server.begin();
}

void loop() {
  server.handleClient();
}
