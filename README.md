# AGV — Veículo Autônomo Guiado

Sistema completo para controlar um veículo autônomo guiado (AGV) por linha, receber rotas por WebSocket e indicar a próxima estação em uma matriz de LEDs. A solução coordena um ESP32 do veículo, um ESP32 das estações e um servidor Node.js.

## Visão geral

O sistema é composto por três partes:

1. **AGV (`AGV.ino`)** — segue uma linha preta usando dois sensores infravermelhos, controla os motores e para nas estações solicitadas.
2. **Servidor (`esp-estacoes/server`)** — recebe rotas digitadas no terminal, envia comandos aos ESP32 e sincroniza o veículo com as estações.
3. **Indicador de estações (`esp-estacoes/esp`)** — acende o LED correspondente à próxima estação informada pelo servidor.

O percurso possui três estações numeradas e um quarto ponto físico reservado ao estoque. Toda rota termina no estoque.

## Arquitetura

```text
Terminal
  │ nova rota (ex.: 1,3)
  ▼
Servidor Node.js:3000
  ├── WebSocket ── ESP32 do AGV
  └── WebSocket ── ESP32 das estações/LEDs
```

## Funcionamento

1. O servidor aguarda a conexão dos dois ESP32.
2. O usuário informa no terminal uma rota com uma a três estações, em ordem crescente.
3. O servidor envia ao AGV uma mensagem no formato `ROTA:1,3`.
4. O AGV segue a linha e para nas marcações correspondentes.
5. A cada destino, o AGV envia `ESTACAO:<n>` ao servidor.
6. O servidor ativa o LED da estação atual no ESP32 indicador.
7. Após atender a rota, o AGV retorna ao estoque e informa o término da operação.

Mensagens WebSocket utilizadas:

| Origem | Mensagem | Finalidade |
|---|---|---|
| AGV → servidor | `ESP32_CONECTADO` | Identifica o cliente do veículo |
| AGV → servidor | `ROTA_RECEBIDA` | Confirma o recebimento da rota |
| AGV → servidor | `ESTACAO:<n>` | Informa a próxima estação, de 0 a 3 |
| AGV → servidor | `PARADA_REALIZADA:<n>` | Confirma uma parada |
| AGV → servidor | `ROTA_FINALIZADA` | Informa o término da rota |
| Servidor → AGV | `ROTA:<n>,<n>` | Envia os destinos selecionados |
| Servidor → LEDs | `ESTACAO:<n>` | Seleciona o LED da estação; `0` apaga todos |
| LEDs → servidor | `LED_ESP32_CONECTADO` | Identifica o cliente dos LEDs |

## Estrutura do projeto

```text
AGV/
├── AGV.ino                  # Firmware do veículo ESP32
├── esp-estacoes/
│   ├── esp/
│   │   └── esp.ino          # Firmware dos LEDs das estações
│   └── server/
│       ├── server.js        # Servidor WebSocket e interface de rota
│       ├── package.json
│       └── package-lock.json
└── .gitignore
```

## Tecnologias

- Arduino/C++ para o ESP32
- ESP32 Arduino Core
- Wi-Fi
- WebSockets
- Node.js
- `ws`
- Dois sensores infravermelhos
- Ponte H ou driver de motores compatível

## Hardware

### AGV

| Componente | GPIO |
|---|---:|
| Sensor IR direito | 33 |
| Sensor IR esquerdo | 32 |
| PWM motor direito | 27 |
| Motor direito IN1 | 25 |
| Motor direito IN2 | 26 |
| PWM motor esquerdo | 14 |
| Motor esquerdo IN1 | 18 |
| Motor esquerdo IN2 | 19 |

Velocidade padrão: `160/255`.

### ESP32 das estações

| LED | GPIO |
|---|---:|
| Estação 1 | 25 |
| Estação 2 | 26 |
| Estação 3 | 27 |

## Bibliotecas necessárias no Arduino IDE

Instale a biblioteca **WebSockets** de Markus Sattler (`WebSocketsClient`) e selecione uma placa ESP32 compatível.

Para o firmware das estações, crie o arquivo `esp-estacoes/esp/secrets.h` com:

```cpp
const char* ssid = "NOME_DA_REDE";
const char* password = "SENHA_DA_REDE";
```

> O firmware principal do AGV usa credenciais Wi-Fi diretamente no arquivo `AGV.ino`. Para evitar expor informações sensíveis, converta também essas constantes para configurações privadas antes de publicar ou distribuir o projeto.

## Configuração

### 1. Servidor Node.js

```bash
cd esp-estacoes/server
npm install
node server.js
```

O servidor escuta em `0.0.0.0:3000` e solicita uma rota no terminal.

Exemplos:

```text
Nova rota: 1,3
Nova rota: 2
Nova rota: 1,2,3
```

As estações devem estar separadas por vírgulas, sem repetição e em ordem crescente.

### 2. Firmware do AGV

1. Abra `AGV.ino` no Arduino IDE.
2. Configure `WIFI_SSID`, `WIFI_PASSWORD`, `SERVER_IP` e `SERVER_PORT`.
3. Conecte o ESP32 à rede.
4. Selecione o modelo correto da placa ESP32.
5. Faça o upload do firmware.

O IP configurado no firmware deve apontar para o computador que executa `server.js`.

### 3. Firmware dos LEDs

1. Crie `esp-estacoes/esp/secrets.h` com as credenciais da rede.
2. Ajuste `serverIP` e `serverPort` em `esp-estacoes/esp/esp.ino`, quando necessário.
3. Conecte LEDs ou módulos compatíveis às portas 25, 26 e 27.
4. Faça o upload do firmware para o ESP32 das estações.

## Estados do AGV

O firmware utiliza uma máquina de estados:

- `AGUARDANDO_ROTA`
- `SEGUINDO_LINHA`
- `PARADO_NA_ESTACAO`
- `SAINDO_DA_ESTACAO`

A perda da conexão Wi-Fi ou WebSocket interrompe o movimento. O veículo aguarda por 2 segundos em cada estação antes de continuar a linha.

## Segurança

Os códigos atuais incluem endereços IP e credenciais diretamente nos arquivos. Antes de usar o projeto em produção:

- Remova senhas e dados de rede dos arquivos versionados.
- Use variáveis de ambiente ou arquivos locais ignorados pelo Git.
- Atualize o `SERVER_IP` e confirme o IP atual do servidor na rede local.
- Confira a polaridade e a alimentação dos motores antes de energizar o AGV.

## Possíveis melhorias

- Trocar os dados fixos por arquivos de configuração.
- Implementar uma interface web para definir rotas.
- Persistir o estado da rota em caso de reconexão.
- Adicionar sensores de obstáculos.
- Registrar telemetria e histórico de viagens.
- Validar automaticamente os sensores e os motores na inicialização.
