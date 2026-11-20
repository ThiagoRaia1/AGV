
const WebSocket = require("ws");
const readline = require("readline");

// =====================================================
// CONFIGURAÇÕES
// =====================================================

const PORT = 3000;

const server = new WebSocket.Server({
  host: "0.0.0.0",
  port: PORT,
});

let esp32 = null;
let rotaAtual = [];

// =====================================================
// INICIALIZAÇÃO
// =====================================================

console.log("================================");
console.log("SERVIDOR AGV");
console.log("================================");

server.on("listening", () => {
  console.log(`[OK] Servidor iniciado na porta ${PORT}`);
  console.log("[AGUARDANDO] Conexao do ESP32...");
});

server.on("error", (error) => {
  console.log("[ERRO] Servidor:", error.message);
});

// =====================================================
// ENVIO DA ROTA
// =====================================================

function enviarRota() {

  if (!esp32 || esp32.readyState !== WebSocket.OPEN) {
    console.log("[AGUARDANDO] ESP32 nao conectado. Rota armazenada.");
    return;
  }

  if (rotaAtual.length === 0) {
    console.log("[ERRO] Nenhuma rota definida.");
    return;
  }

  const mensagem = `ROTA:${rotaAtual.join(",")}`;

  esp32.send(mensagem);

  console.log("--------------------------------");
  console.log("[ENVIO] Rota enviada ao ESP32:", mensagem);
}

// =====================================================
// DEFINIÇÃO DA ROTA
// =====================================================

function definirRota(estacoes) {

  const valida =
    Array.isArray(estacoes) &&
    estacoes.length > 0 &&
    estacoes.length <= 3 &&
    estacoes.every((estacao, index) => {
      return (
        Number.isInteger(estacao) &&
        estacao >= 1 &&
        estacao <= 3 &&
        (index === 0 || estacao > estacoes[index - 1])
      );
    });

  if (!valida) {
    console.log("[ERRO] Rota invalida!");
    console.log("Utilize numeros de 1 a 3 em ordem crescente.");
    return;
  }

  rotaAtual = [...estacoes];

  console.log("--------------------------------");
  console.log("[OK] Nova rota definida:", rotaAtual);

  enviarRota();
}

// =====================================================
// CONEXÃO COM O ESP32
// =====================================================

server.on("connection", (socket) => {

  esp32 = socket;

  console.log("--------------------------------");
  console.log("[OK] ESP32 conectado ao servidor!");

  socket.on("message", (message) => {

    const mensagem = message.toString();

    console.log("[RECEBIDO]", mensagem);

    switch (mensagem) {

      case "ESP32_CONECTADO":

        console.log("[OK] ESP32 pronto para comunicacao!");

        // Se já existe uma rota, envia novamente
        if (rotaAtual.length > 0) {
          enviarRota();
        }

        break;

      case "ROTA_RECEBIDA":

        console.log("[OK] ESP32 confirmou o recebimento da rota!");
        console.log("[AGUARDANDO] Inicio e execucao do trajeto.");

        break;

      case "ROTA_FINALIZADA":

        console.log("[OK] AGV finalizou o trajeto!");

        break;

      default:

        if (mensagem.startsWith("PARADA_REALIZADA:")) {
          console.log("[OK] Parada confirmada pelo AGV:", mensagem);
        }

        break;
    }
  });

  socket.on("close", () => {

    console.log("[AVISO] ESP32 desconectado!");

    if (esp32 === socket) {
      esp32 = null;
    }
  });

  socket.on("error", (error) => {
    console.log("[ERRO] WebSocket:", error.message);
  });
});

// =====================================================
// TERMINAL PARA DEFINIR A ROTA
// =====================================================

const terminal = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
});

console.log("--------------------------------");
console.log("Digite as estacoes da rota.");
console.log("Exemplo: 1,3");
console.log("--------------------------------");

function solicitarRota() {

  terminal.question("Nova rota: ", (resposta) => {

    const estacoes = resposta
      .split(",")
      .map((item) => item.trim())
      .map((item) => /^\d+$/.test(item) ? Number(item) : NaN);

    definirRota(estacoes);

    solicitarRota();
  });
}

solicitarRota();
