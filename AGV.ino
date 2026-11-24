#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>

// =====================================================
// CONFIGURACOES DE REDE
// =====================================================

const char* WIFI_SSID = "S25 de Thiago";
const char* WIFI_PASSWORD = "thiagoraia";

const char* SERVER_IP = "10.130.52.125";
const int SERVER_PORT = 3000;

WebSocketsClient webSocket;

// =====================================================
// SENSORES
// =====================================================

#define IR_SENSOR_RIGHT 33
#define IR_SENSOR_LEFT  32

#define MOTOR_SPEED 160

// =====================================================
// CONFIGURACAO DAS ESTACOES
// =====================================================

// Estacoes fisicas:
// 1 = Estacao 1
// 2 = Estacao 2
// 3 = Estacao 3
// 0 = Estoque (sempre destino final)
//
// O estoque corresponde a quarta marcacao fisica.

const int MAX_ESTACOES = 4;
const int MARCACAO_ESTOQUE = 4;

// =====================================================
// LEDS DE STATUS DO AGV
// =====================================================

const int parado = 2;
const int virando = 4;
const int reto = 13;

// =====================================================
// MOTOR DIREITO
// =====================================================

int enableRightMotor = 27;
int rightMotorPin1 = 25;
int rightMotorPin2 = 26;

// =====================================================
// MOTOR ESQUERDO
// =====================================================

int enableLeftMotor = 14;
int leftMotorPin1 = 18;
int leftMotorPin2 = 19;

// =====================================================
// CONTROLE DA ROTA
// =====================================================

int rota[MAX_ESTACOES];

int quantidadeEstacoes = 0;
int indiceRota = 0;

// Conta todas as marcacoes fisicas encontradas.
int marcacoesDetectadas = 0;

// Autoriza movimentacao somente depois de receber rota.
bool rotaRecebida = false;

// Impede contar a mesma marcacao mais de uma vez.
bool emMarcacao = false;

// =====================================================
// ESTADOS DO AGV
// =====================================================

enum EstadoAGV {
  AGUARDANDO_ROTA,
  SEGUINDO_LINHA,
  PARADO_NA_ESTACAO,
  SAINDO_DA_ESTACAO
};

EstadoAGV estadoAGV = AGUARDANDO_ROTA;

unsigned long tempoInicioParada = 0;

const unsigned long TEMPO_PARADA = 2000;

// =====================================================
// PROTOTIPOS
// =====================================================

void rotateMotor(int rightMotorSpeed, int leftMotorSpeed);

void apagarLeds();

void receberRota(uint8_t* payload, size_t length);

void enviarProximaEstacao();

void webSocketEvent(
  WStype_t type,
  uint8_t* payload,
  size_t length
);

void verificarEstacao(int rightSensor, int leftSensor);

void controlarAGV();

// =====================================================
// CONTROLE DOS LEDS DO AGV
// =====================================================

void apagarLeds() {
  digitalWrite(parado, LOW);
  digitalWrite(virando, LOW);
  digitalWrite(reto, LOW);
}

// =====================================================
// ENVIA A PROXIMA ESTACAO PARA O SERVIDOR
// =====================================================

void enviarProximaEstacao() {

  int proximaEstacao;

  if (indiceRota >= quantidadeEstacoes) {
    proximaEstacao = 0;
  } else {
    proximaEstacao = rota[indiceRota];
  }

  String mensagem = "ESTACAO:" + String(proximaEstacao);

  webSocket.sendTXT(mensagem);

  Serial.print("[WEBSOCKET] Proxima estacao enviada: ");
  Serial.println(mensagem);
}

// =====================================================
// RECEBIMENTO DA ROTA
// =====================================================

void receberRota(uint8_t* payload, size_t length) {

  String mensagem;

  for (size_t i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  if (!mensagem.startsWith("ROTA:")) {
    Serial.println("[ERRO] Mensagem invalida!");
    return;
  }

  mensagem.remove(0, 5);
  mensagem.trim();

  int novaRota[3];
  int novaQuantidade = 0;

  int inicio = 0;

  // =================================================
  // INTERPRETA A ROTA RECEBIDA
  // =================================================

  while (inicio < mensagem.length()) {

    if (novaQuantidade >= 3) {
      Serial.println("[ERRO] Maximo de 3 estacoes selecionadas!");
      return;
    }

    int virgula = mensagem.indexOf(',', inicio);

    String valor = virgula == -1
                     ? mensagem.substring(inicio)
                     : mensagem.substring(inicio, virgula);

    valor.trim();

    if (
      valor.length() != 1 ||
      valor[0] < '1' ||
      valor[0] > '3'
    ) {
      Serial.println("[ERRO] Estacao invalida! Utilize 1, 2 ou 3.");
      return;
    }

    int estacao = valor.toInt();

    // Exige ordem crescente e nao permite repeticoes.
    if (
      novaQuantidade > 0 &&
      estacao <= novaRota[novaQuantidade - 1]
    ) {
      Serial.println("[ERRO] Ordem das estacoes invalida!");
      return;
    }

    novaRota[novaQuantidade] = estacao;
    novaQuantidade++;

    if (virgula == -1) {
      break;
    }

    inicio = virgula + 1;

    if (inicio >= mensagem.length()) {
      Serial.println("[ERRO] Rota com formato invalido!");
      return;
    }
  }

  if (novaQuantidade == 0) {
    Serial.println("[ERRO] Rota vazia!");
    return;
  }

  // =================================================
  // MONTA A ROTA COMPLETA
  // =================================================

  for (int i = 0; i < novaQuantidade; i++) {
    rota[i] = novaRota[i];
  }

  // Estoque sempre sera o ultimo destino.
  rota[novaQuantidade] = 0;

  quantidadeEstacoes = novaQuantidade + 1;

  // =================================================
  // REINICIALIZA OS CONTADORES
  // =================================================

  indiceRota = 0;
  marcacoesDetectadas = 0;
  emMarcacao = false;

  tempoInicioParada = 0;

  // =================================================
  // LIBERA O AGV
  // =================================================

  rotateMotor(0, 0);
  apagarLeds();

  rotaRecebida = true;
  estadoAGV = SEGUINDO_LINHA;

  // =================================================
  // LOGS
  // =================================================

  Serial.println();
  Serial.println("================================");
  Serial.println("[OK] ROTA RECEBIDA!");
  Serial.println("================================");

  Serial.println("[ROTA] Destinos programados:");

  for (int i = 0; i < quantidadeEstacoes; i++) {

    Serial.print("Destino ");
    Serial.print(i + 1);
    Serial.print(": ");

    if (rota[i] == 0) {
      Serial.println("0 - ESTOQUE");
    } else {
      Serial.println(rota[i]);
    }
  }

  Serial.println("--------------------------------");

  Serial.print("[INFO] Total de destinos: ");
  Serial.println(quantidadeEstacoes);

  Serial.println("[INFO] Estoque definido como destino final.");
  Serial.println("[OK] AGV LIBERADO PARA MOVIMENTACAO.");

  Serial.println("================================");

  // Confirma recebimento da rota.
  webSocket.sendTXT("ROTA_RECEBIDA");

  // Informa ao servidor a primeira estacao.
  enviarProximaEstacao();
}

// =====================================================
// EVENTOS WEBSOCKET
// =====================================================

void webSocketEvent(
  WStype_t type,
  uint8_t* payload,
  size_t length
) {

  switch (type) {

    case WStype_DISCONNECTED:

      Serial.println("[AVISO] WebSocket desconectado!");

      rotaRecebida = false;
      estadoAGV = AGUARDANDO_ROTA;

      rotateMotor(0, 0);
      apagarLeds();

      break;

    case WStype_CONNECTED:

      Serial.println("[OK] WebSocket conectado ao notebook!");

      webSocket.sendTXT("ESP32_CONECTADO");

      Serial.println("[AGUARDANDO] Envio do trajeto pelo servidor...");

      break;

    case WStype_TEXT:

      Serial.println("[RECEBIDO] Nova mensagem do servidor!");

      receberRota(payload, length);

      break;

    default:
      break;
  }
}

// =====================================================
// VERIFICACAO DAS ESTACOES
// =====================================================

void verificarEstacao(int rightSensor, int leftSensor) {

  bool ambosPretos =
    rightSensor == HIGH && leftSensor == HIGH;

  if (!ambosPretos || emMarcacao) {
    return;
  }

  // =================================================
  // REGISTRA A MARCACAO
  // =================================================

  emMarcacao = true;

  marcacoesDetectadas++;

  Serial.println();
  Serial.println("--------------------------------");

  Serial.print("[MARCACAO] Numero detectado: ");
  Serial.println(marcacoesDetectadas);

  // =================================================
  // VERIFICA SE AINDA EXISTE DESTINO PROGRAMADO
  // =================================================

  if (indiceRota >= quantidadeEstacoes) {

    Serial.println("[AVISO] Todos os destinos ja foram atendidos.");

    rotateMotor(0, 0);

    return;
  }

  // =================================================
  // IDENTIFICA A MARCACAO DO DESTINO ATUAL
  // =================================================

  int destinoAtual = rota[indiceRota];

  // O destino 0 corresponde a quarta marcacao fisica.
  int marcacaoDestino = destinoAtual == 0
                          ? MARCACAO_ESTOQUE
                          : destinoAtual;

  Serial.print("[INFO] Proximo destino: ");
  Serial.println(destinoAtual);

  // =================================================
  // VERIFICA SE DEVE PARAR
  // =================================================

  if (marcacoesDetectadas == marcacaoDestino) {

    Serial.println("[PARADA] DESTINO ENCONTRADO!");

    if (destinoAtual == 0) {
      Serial.println("[DESTINO] ESTOQUE");
    } else {
      Serial.print("[DESTINO] ESTACAO ");
      Serial.println(destinoAtual);
    }

    // Avanca para o proximo destino da rota.
    indiceRota++;

    // Informa ao servidor qual sera o proximo destino.
    enviarProximaEstacao();

    // Inicia o tempo de parada.
    tempoInicioParada = millis();

    estadoAGV = PARADO_NA_ESTACAO;

    rotateMotor(0, 0);

    apagarLeds();
    digitalWrite(parado, HIGH);

    webSocket.sendTXT(
      "PARADA_REALIZADA:" + String(destinoAtual)
    );

  } else {

    // =================================================
    // IGNORA A MARCACAO
    // =================================================

    Serial.println("[INFO] Marcacao nao solicitada.");
    Serial.println("[INFO] Continuando percurso...");

    rotateMotor(MOTOR_SPEED, MOTOR_SPEED);
  }

  Serial.println("--------------------------------");
}

// =====================================================
// CONTROLE PRINCIPAL DO AGV
// =====================================================

void controlarAGV() {

  int rightSensor = digitalRead(IR_SENSOR_RIGHT);
  int leftSensor = digitalRead(IR_SENSOR_LEFT);

  bool ambosPretos =
    rightSensor == HIGH && leftSensor == HIGH;

  bool ambosBrancos =
    rightSensor == LOW && leftSensor == LOW;

  // =================================================
  // AGUARDANDO ROTA
  // =================================================

  if (!rotaRecebida || estadoAGV == AGUARDANDO_ROTA) {

    rotateMotor(0, 0);
    apagarLeds();

    return;
  }

  // =================================================
  // PARADO NA ESTACAO
  // =================================================

  if (estadoAGV == PARADO_NA_ESTACAO) {

    rotateMotor(0, 0);

    digitalWrite(parado, HIGH);

    if (millis() - tempoInicioParada >= TEMPO_PARADA) {

      Serial.println("[INFO] Tempo de parada concluido.");

      // Verifica se todos os destinos foram atendidos.
      if (indiceRota >= quantidadeEstacoes) {

        Serial.println();
        Serial.println("================================");
        Serial.println("[OK] ROTA FINALIZADA!");
        Serial.println("[OK] AGV RETORNOU AO ESTOQUE.");
        Serial.println("[AGUARDANDO] Novo trajeto...");
        Serial.println("================================");

        webSocket.sendTXT("ROTA_FINALIZADA");

        rotaRecebida = false;
        estadoAGV = AGUARDANDO_ROTA;

        rotateMotor(0, 0);
        apagarLeds();

        return;
      }

      Serial.println("[INFO] Saindo da estacao...");

      apagarLeds();

      estadoAGV = SAINDO_DA_ESTACAO;
    }

    return;
  }

  // =================================================
  // SAINDO DA ESTACAO
  // =================================================

  if (estadoAGV == SAINDO_DA_ESTACAO) {

    rotateMotor(MOTOR_SPEED, MOTOR_SPEED);

    // Aguarda os dois sensores detectarem branco.
    if (ambosBrancos) {

      emMarcacao = false;

      estadoAGV = SEGUINDO_LINHA;

      Serial.println("[OK] Estacao liberada.");
      Serial.println("[OK] Retomando trajeto.");
    }

    return;
  }

  // =================================================
  // SEGUINDO A LINHA
  // =================================================

  if (estadoAGV == SEGUINDO_LINHA) {

    // -----------------------------------------------
    // AMBOS OS SENSORES DETECTAM PRETO
    // -----------------------------------------------

    if (ambosPretos) {

      verificarEstacao(rightSensor, leftSensor);

    }

    // -----------------------------------------------
    // AMBOS OS SENSORES DETECTAM BRANCO
    // -----------------------------------------------

    else if (ambosBrancos) {

      emMarcacao = false;

      rotateMotor(MOTOR_SPEED, MOTOR_SPEED);

      digitalWrite(parado, LOW);
      digitalWrite(virando, LOW);
      digitalWrite(reto, HIGH);
    }

    // -----------------------------------------------
    // SENSOR DIREITO DETECTA PRETO
    // -----------------------------------------------

    else if (rightSensor == HIGH && leftSensor == LOW) {

      rotateMotor(MOTOR_SPEED, -MOTOR_SPEED);

      digitalWrite(parado, LOW);
      digitalWrite(virando, HIGH);
      digitalWrite(reto, LOW);
    }

    // -----------------------------------------------
    // SENSOR ESQUERDO DETECTA PRETO
    // -----------------------------------------------

    else if (rightSensor == LOW && leftSensor == HIGH) {

      rotateMotor(-MOTOR_SPEED, MOTOR_SPEED);

      digitalWrite(parado, LOW);
      digitalWrite(virando, HIGH);
      digitalWrite(reto, LOW);
    }
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("================================");
  Serial.println("INICIALIZANDO AGV");
  Serial.println("================================");

  // =================================================
  // CONFIGURACAO DOS PINOS
  // =================================================

  pinMode(enableRightMotor, OUTPUT);
  pinMode(rightMotorPin1, OUTPUT);
  pinMode(rightMotorPin2, OUTPUT);

  pinMode(enableLeftMotor, OUTPUT);
  pinMode(leftMotorPin1, OUTPUT);
  pinMode(leftMotorPin2, OUTPUT);

  pinMode(IR_SENSOR_RIGHT, INPUT);
  pinMode(IR_SENSOR_LEFT, INPUT);

  pinMode(parado, OUTPUT);
  pinMode(virando, OUTPUT);
  pinMode(reto, OUTPUT);

  apagarLeds();

  // =================================================
  // PWM
  // =================================================

  analogWriteResolution(enableRightMotor, 8);
  analogWriteResolution(enableLeftMotor, 8);

  analogWriteFrequency(enableRightMotor, 5000);
  analogWriteFrequency(enableLeftMotor, 5000);

  // Motores desligados na inicializacao.
  rotateMotor(0, 0);

  Serial.println("[OK] Pinos configurados.");
  Serial.println("[OK] Motores desligados.");

  // =================================================
  // WI-FI
  // =================================================

  Serial.println("[INFO] Conectando ao Wi-Fi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {

    rotateMotor(0, 0);

    delay(500);

    Serial.print(".");
  }

  Serial.println();

  Serial.println("[OK] Wi-Fi conectado!");

  Serial.print("[INFO] IP do ESP32: ");
  Serial.println(WiFi.localIP());

  // =================================================
  // WEBSOCKET
  // =================================================

  Serial.println("[INFO] Conectando ao servidor WebSocket...");

  webSocket.begin(
    SERVER_IP,
    SERVER_PORT,
    "/"
  );

  webSocket.onEvent(webSocketEvent);

  webSocket.setReconnectInterval(5000);

  Serial.println("[OK] Cliente WebSocket inicializado.");

  Serial.println("--------------------------------");
  Serial.println("[AGUARDANDO] Conexao com o servidor...");
  Serial.println("[AGUARDANDO] Envio da rota...");
  Serial.println("[SEGURANCA] Motores permanecem desligados.");
  Serial.println("================================");
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  webSocket.loop();

  // =================================================
  // VERIFICA CONEXAO WI-FI
  // =================================================

  if (WiFi.status() != WL_CONNECTED) {

    rotaRecebida = false;
    estadoAGV = AGUARDANDO_ROTA;

    rotateMotor(0, 0);
    apagarLeds();

    return;
  }

  controlarAGV();

  delay(10);
}

// =====================================================
// CONTROLE DOS MOTORES
// =====================================================

void rotateMotor(int rightMotorSpeed, int leftMotorSpeed) {

  // =================================================
  // MOTOR DIREITO
  // =================================================

  if (rightMotorSpeed < 0) {

    digitalWrite(rightMotorPin1, LOW);
    digitalWrite(rightMotorPin2, HIGH);

  } else if (rightMotorSpeed > 0) {

    digitalWrite(rightMotorPin1, HIGH);
    digitalWrite(rightMotorPin2, LOW);

  } else {

    digitalWrite(rightMotorPin1, LOW);
    digitalWrite(rightMotorPin2, LOW);
  }

  // =================================================
  // MOTOR ESQUERDO
  // =================================================

  if (leftMotorSpeed < 0) {

    digitalWrite(leftMotorPin1, LOW);
    digitalWrite(leftMotorPin2, HIGH);

  } else if (leftMotorSpeed > 0) {

    digitalWrite(leftMotorPin1, HIGH);
    digitalWrite(leftMotorPin2, LOW);

  } else {

    digitalWrite(leftMotorPin1, LOW);
    digitalWrite(leftMotorPin2, LOW);
  }

  // =================================================
  // PWM
  // =================================================

  analogWrite(
    enableRightMotor,
    constrain(abs(rightMotorSpeed), 0, 255)
  );

  analogWrite(
    enableLeftMotor,
    constrain(abs(leftMotorSpeed), 0, 255)
  );
}
