
/*
******************************AREA DESTINADA A PROGRAMA DE DEBUG DO CÓDIGO*****************************************************
*/
#define pinBotaoDebug 18
#define habilitaDebugSerial false  //define se envia informações do funcionamento para o monitor serial. "true" envia e "false" não envia. Utilizado apenas para identificar problemas de funcionamento atraves do monitor serial do IDE Arduino. Em situações normais, definir este parametro como "false". Quando usar o monitor, ele deve ser configurado para a velocidade de 115200.

#if habilitaDebugSerial == true
void debug(int pontoParada, String nomeVariavel, String valorVariavel, int tempoParada = -1) {  //TempoParada faz delay. Com -1, para até porta 13 mudar de nível

  Serial.print("(");
  Serial.print(pontoParada);
  Serial.print(") ");

  Serial.print(nomeVariavel);
  Serial.print(":");
  Serial.print(valorVariavel);
  Serial.println();

  if (tempoParada == -1) {

    static bool estadoBotaoAnt = digitalRead(pinBotaoDebug);
    static unsigned long delayDebounce;
    bool estadoBotao;
    bool aguarda = true;
    while (aguarda) {
      estadoBotao = digitalRead(pinBotaoDebug);
      if ((estadoBotao != estadoBotaoAnt) && !estadoBotao) {
        if ((millis() - delayDebounce) > 100) {
          aguarda = false;
          delayDebounce = millis();
        }
      } else if (estadoBotao != estadoBotaoAnt) {
        delayDebounce = millis();
      }
      estadoBotaoAnt = estadoBotao;
    }
  } else if (tempoParada > 0) {
    delay(tempoParada);
  }
}
#endif
/*
******************************FIM DA AREA DESTINADA A PROGRAMA DE DEBUG DO CÓDIGO*****************************************************
*/

#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"

/*
TESTE CAFETEIRA
  RESERVATÓRIO CHEIO:
    - TEMPO PARA ESVAZIAR - 8,40 min
    - LIGA DESLIGA RELÉ - 9,12 - 9,22 - 9,32 - 9,51 - 
*/
#define pinRele 17


void paginaHTML(WiFiClient client);
void analisarProgramacao(int horaAtual, int minutoAtual, int diaDaSemanaAtual);
void capturaDadosURL();
//void capturaInfoURL(String currentLine, int *programacao, int diasem[], int *hora1, int *hora2, int *hora3, int *aux);

const char* ssid = "Fbnet-fibra 2G";
const char* password = "Fb16net00";
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = -3 * 60 * 60;
const int   daylightOffset_sec = 0;
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

const int tempoMaxLigada = 2;   //tempo máximo que a cafeteira vai permanecer ligada em minutos

  /*************************definição de variáveis Globais***********************************/
    String currentLine = "";        // make a String to hold incoming data from the client
    int hora1 = 0, hora2 = 0, hora3 = 0;  // Horario que o café será feito
    int min1 = 0, min2= 0, min3 = 0;      //VARIÁVEIS PARA ARMAZENAR OS MINUTOS
    int programacao = 0;              // Armazena a quantidade de vezes que fará café
    int diasem[7] = {};            // Informa quais os dias será feito o café
    int aux = 0, minTemp = 0, i = 0, tamLine = 0, mostraMsg = 0;       // Variáveis auxiliares para armazenamento temporário
    bool estadoCafeteira = 0;          //Variável usada localmente para saber se a cafeteira está ligada ou não.
  /***********************************************************************************/

WiFiServer server(80);

void setup() {

  #if habilitaDebugSerial == true
      Serial.begin(115200);
      pinMode(pinBotaoDebug, INPUT_PULLUP); 
  #endif

  Serial.begin(115200);
  pinMode(pinRele, OUTPUT);

  // set notification call-back function
  //sntp_set_time_sync_notification_cb( timeavailable );
  sntp_servermode_dhcp(1);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  pinMode(17, OUTPUT);  // set the LED pin mode

  delay(10);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {

  /*
  ********************ÁREA PARA SINCRONIZAÇÃO DE HORA****************************
  */
  struct tm *timeinfo;
  time_t now;
  now = time(nullptr);
  timeinfo = localtime(&now);

  // Acessar as informações de hora, minutos e dia da semana
  int horaAtual = timeinfo->tm_hour;
  int minutoAtual = timeinfo->tm_min;
  int diaDaSemanaAtual = timeinfo->tm_wday; // 0 = domingo, 1 = segunda-feira, ...

  /*
  ********************************************************************************
  */
  WiFiClient client = server.available();  // listen for incoming clients

  if (client) {                     // if you get a client,
    Serial.println("New Client.");  // print a message out the serial port

    while (client.connected()) {  // loop while the client's connected

      if (client.available()) {  // if there's bytes to read from the client,
        char c = client.read();  // read a byte, then
        Serial.write(c);         // print it out the serial monitor

        if (c == '\n') {  // if the byte is a newline character
          //AQUI
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            
            paginaHTML(client);  //Mostra a pagina HTML
            break;    // break out of the while loop:

          } else {  // if you got a newline, then clear currentLine:
            capturaDadosURL();
          }

        }else if (c != '\r'){   // if you got anything else but a carriage return character,
          currentLine += c;  // add it to the end of the currentLine
        }  
      }
    }
    client.stop();
    Serial.println("Client Disconnected.");
  }
  // verifica se Há programação para executar o café
  analisarProgramacao(horaAtual, minutoAtual, diaDaSemanaAtual);

  if(estadoCafeteira == 1){
    if(minutoAtual > (minTemp + tempoMaxLigada)){
      digitalWrite(pinRele, LOW);
      estadoCafeteira = 0;
      minTemp = 0;
      Serial.println("Cafeteira DESLIGADA");
    }
  }
}

void capturaDadosURL(){
  
  if (currentLine.startsWith("Referer")) {  //Verifica se a linha atual é a que contem a informação do link
    tamLine = currentLine.length();
    while (aux <= tamLine) {
      if (currentLine[aux] == '?') {
        // capturaInfoURL(currentLine, &programacao, diasem, &hora1, &time2, &time3, &aux);
        programacao = atoi(&currentLine[aux+8]); //Armazena a quantidade de vezes que o café vai ser feito por dia

        //CAPTURA AS INFORMAÇÕES DA SEMANA
        diasem[0] = atoi(&currentLine[aux + 14]);
        diasem[1] = atoi(&currentLine[aux + 20]);
        diasem[2] = atoi(&currentLine[aux + 26]);
        diasem[3] = atoi(&currentLine[aux + 32]);
        diasem[4] = atoi(&currentLine[aux + 38]);
        diasem[5] = atoi(&currentLine[aux + 44]);
        diasem[6] = atoi(&currentLine[aux + 50]);
        
        //CAPTURA TIMER 1
        if(currentLine[aux + 58] != '0'){
          hora1 = atoi(&currentLine[aux + 58]);
        }else{
          hora1 = atoi(&currentLine[aux + 59]);
        }
        
        if(currentLine[aux + 63] != '0'){
          min1 = atoi(&currentLine[aux + 63]);
        }else{
          min1 = atoi(&currentLine[aux + 64]);
        }

        //CAPTURA TIMER2
        if(currentLine[aux + 72] != '0'){
          hora2 = atoi(&currentLine[aux + 72]);
        }
        else{
          hora2 = atoi(&currentLine[aux + 73]);
        }
        
        if(currentLine[aux + 77] != '0'){
          min2 = atoi(&currentLine[aux + 77]);
        }else{
          min2 = atoi(&currentLine[aux + 78]);
        } 

        //CAPTURA TIMER2
        if(currentLine[aux + 86] != '0'){
          hora3 = atoi(&currentLine[aux + 86]);
        }else{
          hora3 = atoi(&currentLine[aux + 87]);
        }

        if(currentLine[aux + 91] != '0'){
          min3 = atoi(&currentLine[aux + 91]);
        }else{
          min3 = atoi(&currentLine[aux + 92]);
        }

        aux = currentLine.length();
        mostraMsg = 1;  //VARIÁVEL USADA PARA MOSTRA MENSAGEM DE CAFETEIRA NÃO PROGRAMADA.
      }
      aux++;
    }
    aux = 0;
  }
  currentLine = "";
}


void analisarProgramacao(int horaAtual, int minutoAtual, int diaDaSemanaAtual){
  if (programacao == 0 && mostraMsg == 1) {         //HÁ PROGRAMAÇÃO PARA FAZER CAFÉ?
    Serial.println("Cafeteira NÃO programada!");    //NÃO, ENTÃO IMPRIME A MSG
    mostraMsg = 0;
  }else{                                            //SIM, EXECUTA A FUNÇÃO ABAIXO
    switch (programacao)
    {
      case 1:
      if(diasem[diaDaSemanaAtual] == diaDaSemanaAtual){                           //DIA PARA FAZER CAFÉ É HOJE?
        if(hora1 != 0 && (hora1 == horaAtual && min1 == minutoAtual))  {     //SIM, A HORA PARA FAZER CAFÉ É MAIOR QUE A HORA ATUAL
          digitalWrite(pinRele, HIGH);
          estadoCafeteira = 1;
          minTemp = minutoAtual;
        }else{
          if(hora2 != 0 && (hora2 == horaAtual && min2 == minutoAtual)){
            digitalWrite(pinRele, HIGH);
            estadoCafeteira = 1;
            minTemp = minutoAtual;
          }else{
            if(hora3 != 0 && (hora3 == horaAtual && min3 == minutoAtual)){
              digitalWrite(pinRele, HIGH);
              estadoCafeteira = 1;
              minTemp = minutoAtual;
            }
          }
        }                                  
      }
      break;

      case 2:
      if(diasem[diaDaSemanaAtual] == diaDaSemanaAtual){                           //DIA PARA FAZER CAFÉ É HOJE?
        if(hora1 != 0 && (hora1 == horaAtual && min1 == minutoAtual))  {     //SIM, A HORA PARA FAZER CAFÉ É MAIOR QUE A HORA ATUAL
          digitalWrite(pinRele, HIGH);
          estadoCafeteira = 1;
          minTemp = minutoAtual;
        }else{
          if(hora2 != 0 && (hora2 == horaAtual && min2 == minutoAtual)){
            digitalWrite(pinRele, HIGH);
            estadoCafeteira = 1;
            minTemp = minutoAtual;
          }else{
            if(hora3 != 0 && (hora3 == horaAtual && min3 == minutoAtual)){
              digitalWrite(pinRele, HIGH);
              estadoCafeteira = 1;
              minTemp = minutoAtual;
            }
          }
        }                                  
      }
      break;

      case 3:
      if(diasem[diaDaSemanaAtual] == diaDaSemanaAtual){                           //DIA PARA FAZER CAFÉ É HOJE?
        if(hora1 != 0 && hora1 == horaAtual && min1 == minutoAtual)  {     //SIM, A HORA PARA FAZER CAFÉ É MAIOR QUE A HORA ATUAL
          digitalWrite(pinRele, HIGH);
          estadoCafeteira = 1;
          minTemp = minutoAtual;
          Serial.println("Cafeteira Ligada timer 1");
          delay(1000);
        }else{
          if(hora2 != 0 && hora2 == horaAtual && min2 == minutoAtual){
            digitalWrite(pinRele, HIGH);
            estadoCafeteira = 1;
            minTemp = minutoAtual;
            Serial.println("Cafeteira Ligada timer 2");
            delay(1000);
          }else{
            if(hora3 != 0 && hora3 == horaAtual && min3 == minutoAtual){
              digitalWrite(pinRele, HIGH);
              estadoCafeteira = 1;
              minTemp = minutoAtual;
              Serial.println("Cafeteira Ligada timer 3");
              delay(1000);
            }
          }
        }                                  
      }
      break;

      default :
      programacao = 0;
      break;

    }
  }
}

void paginaHTML(WiFiClient client){
  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
    // and a content-type so the client knows what's coming, then a blank line:
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println();

    // the content of the HTTP response follows the header:

    client.print("<html><head><title>TCC WIFI</title></head>\n");
    client.print("<body>");
    client.print("<form method=get action=/dados>\n");

    client.print("<label for=\"repdia\">PROGRAMAR:</label>");
    client.print("<select name=\"repdia\" id=\"repdia\">");
    client.print("<option value=\"0\" selected>Nao programar</option>");
    client.print("<option value=\"1\">Uma Vez ao dia</option>");
    client.print("<option value=\"2\">Duas Vezes ao dia</option>");
    client.print("<option value=\"3\">Tres Vezes ao dia</option></select>");

    client.print("<br>");

    client.print("<label for=\"dom\">Domingo:</label>");
    client.print("<select name=\"dom\" id=\"dom\">");
    client.print("<option value=\"0\">sim</option>");
    client.print("<option value=\"8\" selected>não</option>");
    client.print("</select><br>");

    client.print("<label for=\"seg\">Segunda-Feira	:</label>");
    client.print("<select name=\"seg\" id=\"seg\">");
    client.print("<option value=\"1\">sim</option>");
    client.print("<option value=\"0\" selected>não</option>");
    client.print("</select><br>");

    client.print("<label for=\"ter\">Terça-Feira:</label>");
    client.print("<select name=\"ter\" id=\"ter\">");
    client.print("<option value=\"2\">sim</option>");
    client.print("<option value=\"0\" selected>não</option>");
    client.print("</select><br>");

    client.print("<label for=\"qua\">Quarta-Feira:</label>");
    client.print("<select name=\"qua\" id=\"qua\">");
    client.print("<option value=\"3\">sim</option>");
    client.print("<option value=\"0\" selected>não</option>");
    client.print("</select><br>");

    client.print("<label for=\"qui\">Quinta-Feira:</label>");
    client.print("<select name=\"qui\" id=\"qui\">");
    client.print("<option value=\"4\">sim</option>");
    client.print("<option value=\"0\" selected>não</option>");
    client.print("</select><br>");

    client.print("<label for=\"sex\">Sexta-Feira:</label>");
    client.print("<select name=\"sex\" id=\"sex\">");
    client.print("<option value=\"5\">sim</option>");
    client.print("<option value=\"0\" selected>não</option>");
    client.print("</select><br>");

    client.print("<label for=\"sab\">Sabado:</label>");
    client.print("<select name=\"sab\" id=\"sab\">");
    client.print("<option value=\"6\">sim</option>");
    client.print("<option value=\"0\" selected>não</option>");
    client.print("</select><br>");



    client.print("<label for=\"time1\">Horario1:</label>");
    client.print("<input type=\"time\" id=\"time1\" name=\"time1\" value=\"00:00\">");

    client.print("<label for=\"time2\">Horario2:</label>");
    client.print("<input type=\"time\" id=\"time2\" name=\"time2\" value=\"00:00\">");

    client.print("<label for=\"time3\">Horario2:</label>");
    client.print("<input type=\"time\" id=\"time3\" name=\"time3\" value=\"00:00\">");

    client.print("<br><br>");

    client.print("<input type=\"submit\" value=\"Submit\">");
    client.print("</form></body></html>");
    // The HTTP response ends with another blank line:
    client.println();
}
/*
void capturaInfoURL(String currentLine, int *programacao, int diasem[], int *hora1, int *time2, int *time3, int *aux){ 
  int aux2 = *aux;
  programacao = atoi(currentLine[aux2+8]); //Armazena a quantidade de vezes que o café vai ser feito por dia
  
  //CAPTURA AS INFORMAÇÕES DA SEMANA
  diasem[0] = atoi(currentLine[aux2 + 14]);
  diasem[1] = atoi(currentLine[aux2 + 20]);
  diasem[2] = atoi(currentLine[aux2 + 26]);
  diasem[3] = atoi(currentLine[aux2 + 32]);
  diasem[4] = atoi(currentLine[aux2 + 38]);
  diasem[5] = atoi(currentLine[aux2 + 44]);
  diasem[6] = atoi(currentLine[aux2 + 50]);
  
  //CAPTURA TIMER 1
  hora1 += currentLine[aux2 + 58];
  hora1 += currentLine[aux2 + 59];
  hora1 += currentLine[aux2 + 63];
  hora1 += currentLine[aux2 + 64];
  //CAPTURA TIMER2
  time2 += currentLine[aux2 + 72];
  time2 += currentLine[aux2 + 73];
  time2 += currentLine[aux2 + 77];
  time2 += currentLine[aux2 + 78];
  //CAPTURA TIMER2
  time3 += currentLine[aux2 + 86];
  time3 += currentLine[aux2 + 87];
  time3 += currentLine[aux2 + 91];
  time3 += currentLine[aux2 + 92];

  aux = currentLine.length();

}
*/

/*
#define pinBotaoDebug 18
#define habilitaDebugSerial false  //define se envia informações do funcionamento para o monitor serial. "true" envia e "false" não envia. Utilizado apenas para identificar problemas de funcionamento atraves do monitor serial do IDE Arduino. Em situações normais, definir este parametro como "false". Quando usar o monitor, ele deve ser configurado para a velocidade de 115200.

#if habilitaDebugSerial == true
void debug(int pontoParada, String nomeVariavel, String valorVariavel, int tempoParada = -1) {  //TempoParada faz delay. Com -1, para até porta 13 mudar de nível

  Serial.print("(");
  Serial.print(pontoParada);
  Serial.print(") ");

  Serial.print(nomeVariavel);
  Serial.print(":");
  Serial.print(valorVariavel);
  Serial.println();

  if (tempoParada == -1) {

    static bool estadoBotaoAnt = digitalRead(pinBotaoDebug);
    static unsigned long delayDebounce;
    bool estadoBotao;
    bool aguarda = true;
    while (aguarda) {
      estadoBotao = digitalRead(pinBotaoDebug);
      if ((estadoBotao != estadoBotaoAnt) && !estadoBotao) {
        if ((millis() - delayDebounce) > 100) {
          aguarda = false;
          delayDebounce = millis();
        }
      } else if (estadoBotao != estadoBotaoAnt) {
        delayDebounce = millis();
      }
      estadoBotaoAnt = estadoBotao;
    }
  } else if (tempoParada > 0) {
    delay(tempoParada);
  }
}
#endif
*/
