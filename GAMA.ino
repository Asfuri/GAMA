// Condicional para incluir biblioteca adequada ao tipo de dispositivo
#ifdef ESP32
#include <WiFi.h>             // Inclui biblioteca de WiFi para ESP32
#else
#include <ESP8266WiFi.h>      // Inclui biblioteca de WiFi para ESP8266
#endif

// Inclui bibliotecas adicionais
#include <WiFiClientSecure.h> // Cliente HTTPS para conexão segura
#include <UniversalTelegramBot.h> // Biblioteca para usar o bot do Telegram
#include <ArduinoJson.h>      // Biblioteca para manipulação de JSON

// Informações da rede WiFi
const char* ssid = "SSID DA REDE";
const char* password = "SENHA DA REDE";

// Token de autenticação do bot do Telegram
#define BOTtoken "API KEY"

// Configuração de certificado para ESP8266
#ifdef ESP8266
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

// Cria uma instância do cliente seguro e do bot do Telegram
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Define o pino do sensor
#define SENSOR_PIN D15

// Variáveis para controle de temporização e estado das mensagens
int botRequestDelay = 100;
unsigned long lastTimeBotRan = 0;
unsigned long lastTimeNoWaterMessageSent = 0;
bool waterEmptyNotified = false;
bool waterAvailableNotified = false;
bool autoMessagesEnabled = true;

// Mensagens padrão enviadas pelo bot
const String WATER_EMPTY_MESSAGE = "(Mensagem Automática) Acabou a água no primeiro andar do Ci, realize a troca do galão.";
const String WATER_AVAILABLE_MESSAGE = "(Mensagem Automática) Há água, o galão do primeiro andar do Ci acabou de ser trocado!";

void setup() {
  Serial.begin(115200); // Inicia comunicação serial

  // Configurações específicas para ESP8266
#ifdef ESP8266
  configTime(0, 0, "pool.ntp.org"); // Configura o horário via NTP
  client.setTrustAnchors(&cert);    // Configura certificados SSL
#endif

  pinMode(SENSOR_PIN, INPUT_PULLUP); // Configura o pino do sensor como entrada com pull-up

  WiFi.mode(WIFI_STA);               // Configura o ESP para modo estação (cliente WiFi)
  WiFi.begin(ssid, password);        // Inicia conexão WiFi

  // Configurações específicas para ESP32
#ifdef ESP32
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Configura certificados SSL
#endif
  // Espera pela conexão WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println(WiFi.localIP()); // Mostra o IP local na serial
}

void handleAutoMessages(String chat_id, bool enable) {
  autoMessagesEnabled = enable; // Ativa ou desativa mensagens automáticas
  // Envia mensagem ao usuário sobre o estado das mensagens automáticas
  bot.sendMessage(chat_id, (enable ? "Mensagens automáticas ativadas." : "Mensagens automáticas desativadas."), "");
}

void processAutoMessages() {
  if (!autoMessagesEnabled) return; // Retorna se mensagens automáticas estão desativadas

  int sensorValue = digitalRead(SENSOR_PIN); // Lê o valor do sensor
  unsigned long currentTime = millis(); // Pega o tempo atual

  // Verifica se o sensor indica água disponível
  if (sensorValue == 0 && waterAvailableNotified) {
    waterAvailableNotified = false;
  }

  // Se não há água
  if (sensorValue == 0) {
    if (!waterEmptyNotified || (currentTime - lastTimeNoWaterMessageSent >= 600000)) {
      // Envia mensagem de água acabou
      bot.sendMessage(String(bot.messages[0].chat_id), WATER_EMPTY_MESSAGE, "");
      waterEmptyNotified = true;
      lastTimeNoWaterMessageSent = currentTime;
    }
  } else {
    // Se há água e ainda não notificado
    if (!waterAvailableNotified) {
      // Envia mensagem de água disponível
      bot.sendMessage(String(bot.messages[0].chat_id), WATER_AVAILABLE_MESSAGE, "");
      waterAvailableNotified = true;
      waterEmptyNotified = false; // Reseta a notificação de água acabou
    }
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    // Responde ao comando /start com uma mensagem de boas-vindas
    if (text == "/start") {
      String welcomeMessage = "Bem-vindo ao GAMA!\n";
      welcomeMessage += "Use o comando /verificar para checar o estado do sensor, /ativarauto para ativar as atualizações automáticas e /desativarauto para desativá-las.";
      bot.sendMessage(chat_id, welcomeMessage, "");
      continue;
    }

    // Verifica o estado do sensor e responde
    if (text == "/verificar") {
      int sensorValue = digitalRead(SENSOR_PIN);
      bot.sendMessage(chat_id, (sensorValue == 1 ? "Há água no primeiro andar do Ci, lembre-se de se hidratar!" : "Acabou a água no primeiro andar do Ci."), "");
    }

    // Ativa ou desativa mensagens automáticas com base no comando
    if (text == "/ativarauto") {
      handleAutoMessages(chat_id, true);
    }
    if (text == "/desativarauto") {
      handleAutoMessages(chat_id, false);
    }
  }
}

void loop() {
  if (millis() > lastTimeBotRan + botRequestDelay) {
    processAutoMessages(); // Processa mensagens automáticas

    // Busca novas mensagens desde a última verificada
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    if (numNewMessages) {
      handleNewMessages(numNewMessages); // Processa novas mensagens
    }

    lastTimeBotRan = millis(); // Atualiza o tempo da última execução
  }
  delay(100); // Pequeno delay para reduzir uso de CPU
}
