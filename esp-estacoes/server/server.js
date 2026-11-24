
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

// Conexões dos ESP32
let esp32AGV = null;
let esp32LEDs = null;

// Controle da rota
let rotaAtual = [];

// Última estação informada pelo AGV
let ultimaEstacao = null;

// =====================================================
// INICIALIZAÇÃO
// =====================================================

console.log("================================");
console.log("SERVIDOR AGV");
console.log("================================");

server.on("listening", () => {
    console.log(`[OK] Servidor iniciado na porta ${PORT}`);
    console.log("[AGUARDANDO] Conexoes dos ESP32...");
});

server.on("error", (error) => {
    console.log("[ERRO] Servidor:", error.message);
});

// =====================================================
// ENVIO DA ROTA PARA O AGV
// =====================================================

function enviarRota() {

    if (!esp32AGV || esp32AGV.readyState !== WebSocket.OPEN) {
        console.log("[AGUARDANDO] AGV nao conectado. Rota armazenada.");
        return;
    }

    if (rotaAtual.length === 0) {
        console.log("[ERRO] Nenhuma rota definida.");
        return;
    }

    const mensagem = `ROTA:${rotaAtual.join(",")}`;

    esp32AGV.send(mensagem);

    console.log("--------------------------------");
    console.log("[ENVIO AGV]", mensagem);
}

// =====================================================
// ENCAMINHAR ESTAÇÃO PARA O ESP32 DOS LEDs
// =====================================================

function enviarEstacaoParaLEDs(estacao) {

    const mensagem = `ESTACAO:${estacao}`;

    // Guarda a última estação para atualizar os LEDs
    // caso o ESP32 dos LEDs reconecte.
    ultimaEstacao = estacao;

    if (!esp32LEDs || esp32LEDs.readyState !== WebSocket.OPEN) {
        console.log("[AGUARDANDO] ESP32 dos LEDs nao conectado.");
        console.log("[ARMAZENADO]", mensagem);
        return;
    }

    esp32LEDs.send(mensagem);

    console.log("[ENVIO LEDs]", mensagem);
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

    // Uma nova rota ainda não possui estação atual.
    ultimaEstacao = null;

    console.log("--------------------------------");
    console.log("[OK] Nova rota definida:", rotaAtual);

    enviarRota();

    // Apaga os LEDs enquanto o AGV ainda não informou
    // qual será a próxima estação.
    enviarEstacaoParaLEDs(0);
}

// =====================================================
// IDENTIFICAÇÃO E COMUNICAÇÃO DOS ESP32
// =====================================================

server.on("connection", (socket) => {

    console.log("--------------------------------");
    console.log("[CONEXAO] Novo dispositivo conectado.");

    socket.on("message", (message) => {

        const mensagem = message.toString();

        console.log("[RECEBIDO]", mensagem);

        // ---------------------------------------------
        // IDENTIFICAÇÃO DO AGV
        // ---------------------------------------------

        if (mensagem === "ESP32_CONECTADO") {

            esp32AGV = socket;

            console.log("[OK] ESP32 AGV identificado!");

            if (rotaAtual.length > 0) {
                enviarRota();
            }

            return;
        }

        // ---------------------------------------------
        // IDENTIFICAÇÃO DO ESP32 DOS LEDs
        // ---------------------------------------------

        if (mensagem === "LED_ESP32_CONECTADO") {

            esp32LEDs = socket;

            console.log("[OK] ESP32 dos LEDs identificado!");

            // Recupera a última estação informada pelo AGV.
            if (ultimaEstacao !== null) {
                enviarEstacaoParaLEDs(ultimaEstacao);
            }

            return;
        }

        // ---------------------------------------------
        // MENSAGENS DO AGV
        // ---------------------------------------------

        if (socket === esp32AGV) {

            if (mensagem === "ROTA_RECEBIDA") {

                console.log("[OK] AGV confirmou o recebimento da rota!");
                console.log("[AGUARDANDO] Inicio e execucao do trajeto.");

                return;
            }

            if (mensagem === "ROTA_FINALIZADA") {

                console.log("[OK] AGV finalizou o trajeto!");

                // Estoque: apaga todos os LEDs.
                enviarEstacaoParaLEDs(0);

                return;
            }

            if (mensagem.startsWith("PARADA_REALIZADA:")) {

                console.log("[OK] Parada confirmada pelo AGV:", mensagem);

                return;
            }

            // -----------------------------------------
            // PRÓXIMA ESTAÇÃO
            // -----------------------------------------

            const matchEstacao = mensagem.match(/^ESTACAO:([0-3])$/);

            if (matchEstacao) {

                const estacao = Number(matchEstacao[1]);

                console.log("[AGV] Proxima estacao:", estacao);

                enviarEstacaoParaLEDs(estacao);

                return;
            }

            console.log("[AVISO] Mensagem desconhecida do AGV.");

            return;
        }

        // ---------------------------------------------
        // MENSAGENS DO ESP32 DOS LEDs
        // ---------------------------------------------

        if (socket === esp32LEDs) {

            console.log("[LEDs]", mensagem);

            return;
        }

        // ---------------------------------------------
        // DISPOSITIVO NÃO IDENTIFICADO
        // ---------------------------------------------

        console.log("[AVISO] Dispositivo ainda nao identificado.");
    });

    // =================================================
    // DESCONECTAR
    // =================================================

    socket.on("close", () => {

        console.log("--------------------------------");
        console.log("[AVISO] Dispositivo desconectado.");

        if (esp32AGV === socket) {
            esp32AGV = null;
            console.log("[AVISO] AGV desconectado!");
        }

        if (esp32LEDs === socket) {
            esp32LEDs = null;
            console.log("[AVISO] ESP32 dos LEDs desconectado!");
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
