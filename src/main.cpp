// Inclui as funções de Wi-Fi usadas para conectar o ESP8266 à rede.
#include <ESP8266WiFi.h>
// Inclui a implementação do servidor Modbus TCP para ESP8266.
#include <ModbusIP_ESP8266.h>
#include "DHT.h"

// Nome (SSID) da rede Wi-Fi à qual o ESP8266 tentará se conectar.
const char* ssid     = "Fatec-P2-Terreo";

// Senha da rede Wi-Fi; mantenha este valor privado e não o publique.
const char* password = "FatecJdi!";

#define PINO_DHT 14
#define BUZZER 5
#define LED 4
#define LDR A0
#define LED_REATIVO 12
const int LIMITE_LUMINOSIDADE = 250;

float leitura_temperatura = 0;
// Cria o objeto que armazena os registradores e atende às requisições Modbus.
ModbusIP mb;

// Endereços (offsets) dos dados Modbus; normalmente o cliente usa esses offsets
// diretamente, mas a forma de exibição pode variar conforme o software SCADA.
const int HREG_CONTADOR = 0;   // Holding Register no offset 0: contador crescente.
const int HREG_TEMP     = 1;   // Holding Register no offset 1: temperatura simulada.
const int HREG_LDR      = 2;   // Holding Register no offset 2: leitura analógica do LDR (0 a 1023).
const int COIL_LED      = 0;   // Coil no offset 0: estado comandado para o LED integrado.
const int COIL_BUZZER      = 1;   // Coil no offset 1: estado comandado para o Buzzer.
const int ISTS_STATUS_LED_REATIVO = 3; // Entrada discreta no offset 3: estado do LED reativo.

int NIVEL_LUMINOSIDADE = 0;  // Variável global para armazenar o nível de luminosidade lido do LDR.

// Guarda o instante da última atualização periódica, em milissegundos.
unsigned long ultimo = 0;
// Contador de 16 bits sem sinal; aumenta uma unidade a cada segundo.
uint16_t contador = 0;
DHT dht(PINO_DHT, DHT22);

// Executada uma vez após o ESP8266 iniciar ou reiniciar.
void setup() {
  // Inicia a porta serial para exibir mensagens no Monitor Serial a 115200 baud.
  Serial.begin(115200);
  // Configura o pino do LED integrado como saída digital.
  pinMode(LED, OUTPUT);
  // No ESP8266 o LED integrado normalmente acende em LOW; HIGH o deixa apagado.
  digitalWrite(LED, HIGH);  // apagado

  // Usa o LED integrado como indicador de conexão Wi-Fi.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // apagado no ESP8266

  pinMode(PINO_DHT, INPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(LED, HIGH);
  pinMode(LED, OUTPUT);
  pinMode(LED_REATIVO, OUTPUT);
  // Inicia a conexão Wi-Fi e o servidor Modbus TCP, que escuta na porta 502.
  mb.config(ssid, password);
  // Imprime o início do processo de conexão no Monitor Serial.
  Serial.print("Conectando");
  // Aguarda até que o ESP8266 informe que a conexão Wi-Fi foi estabelecida.
  while (WiFi.status() != WL_CONNECTED) {
    // O LED integrado é ativo em LOW no ESP8266.
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(500);
    // Imprime um ponto para indicar que a conexão ainda está sendo aguardada.
    Serial.print(".");
  }
  // Mantém o LED integrado aceso após a conexão ser estabelecida.
  digitalWrite(LED_BUILTIN, LOW);
  // Declara a variável
  NIVEL_LUMINOSIDADE = analogRead(LDR);

  // Termina a linha dos pontos impressos durante a conexão.
  Serial.println();
  // Identifica a mensagem com o endereço IP local do ESP8266.
  Serial.print("IP: ");
  // Obtém e imprime o endereço IP atribuído ao ESP8266 pelo roteador.
  Serial.println(WiFi.localIP());
  // Informa que o servidor Modbus TCP está aguardando na porta definida pela biblioteca.
  Serial.printf("Servidor Modbus TCP aguardando na porta %u\n", MODBUSIP_PORT);
  // Registra a leitura analógica atual do LDR no Holding Register.
  mb.addHreg(HREG_LDR, NIVEL_LUMINOSIDADE);
  // Cria o Holding Register do contador e define seu valor inicial como zero.
  mb.addHreg(HREG_CONTADOR, 0);
  mb.addHreg(HREG_TEMP, dht.readTemperature());
  // Cria o Holding Register da temperatura; 250 representa 25,0 se o cliente
  // interpretar o valor com uma casa decimal (multiplicador 0,1).
  mb.addHreg(HREG_TEMP, dht.readTemperature());        // 250 -> 25,0 com multiplicador 0,1
  // Cria a Coil de controle do LED e inicia seu estado como desligado (false).
  mb.addCoil(COIL_LED, false);
  mb.addCoil(COIL_BUZZER, false);
  bool ledReativoAtivo = NIVEL_LUMINOSIDADE < LIMITE_LUMINOSIDADE;
  mb.addIsts(ISTS_STATUS_LED_REATIVO, ledReativoAtivo);
  digitalWrite(LED_REATIVO, ledReativoAtivo ? HIGH : LOW);
  dht.begin();
}

// Executada repetidamente enquanto o ESP8266 estiver ligado.
void loop() {
  // Verifica e processa conexões e requisições Modbus recebidas pelo servidor.
  mb.task();

  // Lê a Coil do LED e atualiza o pino; a saída LOW acende o LED integrado,
  // enquanto HIGH o apaga.
  
  digitalWrite(LED, mb.Coil(COIL_LED) ? LOW : HIGH);
  digitalWrite(BUZZER, mb.Coil(COIL_BUZZER) ? LOW : HIGH);
  bool ledReativoAtivo = NIVEL_LUMINOSIDADE < LIMITE_LUMINOSIDADE;
  digitalWrite(LED_REATIVO, ledReativoAtivo ? HIGH : LOW);
  mb.Ists(ISTS_STATUS_LED_REATIVO, ledReativoAtivo);


  // Verifica se já passou pelo menos um segundo desde a atualização anterior.
  // A subtração funciona também quando millis() transborda, desde que o
  // intervalo comparado seja menor que o período total de transbordamento.
  if (millis() - ultimo >= 3000) {
    // Registra o instante atual para contar o próximo intervalo de um segundo.
    ultimo = millis();
    // Avança o contador; por ser uint16_t, ele volta a zero após 65535.
    contador++;
    // Atualiza o Holding Register do contador com o valor atual.
    mb.Hreg(HREG_CONTADOR, contador);
    mb.Hreg(HREG_TEMP, dht.readTemperature());
    // Atualiza a temperatura simulada entre 25,0 e 26,9 (escala de décimos).
    // Lê o valor analógico do LDR e atualiza seu Holding Register.
    NIVEL_LUMINOSIDADE = analogRead(LDR);
    mb.Hreg(HREG_LDR, NIVEL_LUMINOSIDADE);
    Serial.printf("Valor do LDR: %d\n", NIVEL_LUMINOSIDADE);
  }
}