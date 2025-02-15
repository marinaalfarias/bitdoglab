# Campo Minado
Esse projeto consiste em um jogo de Campo Minado na placa BitDogLab. Ele foi desenvolvido na linguagem C.
## Funcionamento do jogo
O jogo funciona como um clássico jogo de campo minado.
1. Aleatoriamente, o sistema escolhe um LED da matriz 5x5 para ser a bomba;
2. Com o joystick, o usuário consegue navegar pelos LEDs da matriz;
3. Quando o usuário quiser selecionar um LED, ele deve pressionar o botão A;
 
   3.1. Se o LED selecionado for a bomba, todo o teclado fica vermelho e o buzzer emite um som com frequência de 150Hz por 1s. O jogo acaba.
   
   3.2. Se o LED selecionado não for a bomba, o buzzer emite um som de 50Hz por 0,1s e o LED fica verde

## Pseudocódigo
![Fluxograma do projeto](https://github.com/user-attachments/assets/587db904-bf03-4fa2-a501-0757adad3f8a)
Acima, temos um fluxograma que mostra como está estruturada a lógica do programa e como o pseudocódigo foi pensado.

- **Configuração de Inicialização:** Nessa parte inicial do código, as bibliotecas essenciais são incluídas e os pinos dos componentes utilizados são definidos.
  Essencialmente, usaremos `#include`, `#define`, `const int` e `bool`
- **Definição de funções:** Para que a organização do código seja a melhor possível, antes do loop principal, as funções que irão desempenhar as principais ações durante o código.
  Assim, a visualização do código e da lógica empregada é facilitada
- **Loop principal:** Na parte princial do código (`int main()`), as funções são chamadas de modo que a lógica principal do programa seja cumprida e o jogo funcione como já explicado.

## Código explicado

**Inclusão de bibliotecas:** nessa parte do código, as bibliotecas que serão utilizadas são adicionadas.
```c
#include <stdio.h> // Biblioteca padrão de C
#include "hardware/adc.h" // Biblioteca do ambiente Raspberry Pi Pico para trabalhar com ADC
#include "hardware/pwm.h"// Biblioteca do ambiente Raspberry Pi Pico para trabalhar com PWM
#include "pico/stdlib.h" // Biblioteca do ambiente Raspberry Pi Pico que é responsável por funções básicas, como entrada/saída, dalys, GPIO
#include "ws2818b.pio.h" // Arquivo reponsável pela inicialização de dispositivos PIO
#include "hardware/clocks.h" // Biblioteca do ambiente Raspberry Pi Pico que controla os clocks
#include <stdlib.h> // Biblioteca padrão de C que fornece funções de memória dinâmica
#include <time.h> // Biblioteca padrão de C que lida com funções relacionadas a tempo
```

**Definição dos pinos:** aqui iremos definir os pinos que serão utilizados ao longo do código  
```c
const int VRX = 26;          // Pino de leitura do eixo X do joystick (conectado ao ADC)
const int VRY = 27;          // Pino de leitura do eixo Y do joystick (conectado ao ADC)
const int ADC_CHANNEL_0 = 0; // Canal ADC para o eixo X do joystick
const int ADC_CHANNEL_1 = 1; // Canal ADC para o eixo Y do joystick
const int botaoA = 5;           // Pino de leitura do botão A
const int led_index[5][5] = { // Leitura da matriz de LEDs
    {24, 23, 22, 21, 20},   // Linha 4
    {15, 16, 17, 18, 19},   // Linha 3
    {14, 13, 12, 11, 10},   // Linha 2
    {5, 6, 7, 8, 9},        // Linha 1
    {4, 3, 2, 1, 0}         // Linha 0
};
const int BUZZER_PIN = 21; // Pino do buzzer
#define LED_COUNT 25 // Número de LEDs que existem na matriz
#define LED_PIN 7 // Pino de entrada da matriz
#define PWM_WRAP 255  // Valor máximo do PWM (8 bits)
bool permanent_leds[LED_COUNT] = {false}; // Array para rastrear LEDs permanentes
bool bomba_explodiu = false; // Variável que verifica se a bomba explodiu ou não
```

**Configuração inicial da matriz de LED:** as partes a seguir servem para inicializar todos os LEDs da matriz da forma ideal.

Definição do que é um pixel dentro do código
```c
struct pixel_t {
    uint8_t G, R, B;
};
```
Renomeando `struct pixel_t` para `npLED_t`
```c
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t;
```
Declaração dos LEDs que formam a matriz e controle da máquina PIO
```c
// Declaração do buffer de pixels
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO
PIO np_pio;
uint sm;
```
Inicialização da máquina PIO e controle da matriz de LED
```c
void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}
```
**Inicialização do joystick:** nessa parte do código, duas funções são definidas para que seja possível usar o joystick.

Essa função tem o objetivo de inicializar o joystick.
```c
void setup_joystick() {
    adc_init();
    adc_gpio_init(VRX);
    adc_gpio_init(VRY);
    gpio_init(botaoA);
    gpio_set_dir(botaoA, GPIO_IN);
    gpio_pull_up(botaoA);
}
```

Essa função lê os valores dos eixos (x ou y) do joystick. Ela armezena os valores nas variáveis ```vrx_value``` (para o eixo x) e ```vry_value``` (para o eixo y).
```c
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value) {
    adc_select_input(ADC_CHANNEL_0);
    sleep_us(2);
    *vrx_value = adc_read();
    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    *vry_value = adc_read();
}
```
**Inicialização do PWM:** aqui, iremos inicializar o PWM que será utilizado no funcionamento do buzzer.
```c
void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); // Ajusta divisor de clock
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente
}
```
**Funções que configuram a matriz de LED**

Função para converter o sinal ADC para o intervalo 0 a 4, de forma a ser possível mapear a matriz de LEDs
```c
int map_adc_to_matrix(uint16_t adc_value) {
    return (adc_value * 5) / 4096;
}
```
Função para converter as coordenadas (x,y) em um índice do LED 
```c
int converte_coord_led(int x, int y) {
    if (x >= 0 && x < 5 && y >= 0 && y < 5) {
        return led_index[x][y];
    } else {
        return -1; // Retorna -1 para coordenadas inválidas
    }
}
```
**Função para escolher a bomba:** responsável por escolher aleatoriamente o led que será a bomba.

```c
int escolhe_bomba(){
    int x = rand() % 5;
    int y = rand() % 5;
    sleep_ms(1000);
    printf("Bomba: %d (x=%d, y=%d)\n", led_index[x][y], x, y);
    return led_index[x][y];
}
```

"Semente" para que a bomba seja realmente aleatória. Ela lê o canal ADC0 e armazena essa valor na variável ```random_seed```. Dessa forma, é garantido que o número será realmente aleatório.
```c
void init_random_seed() {
    adc_init();
    adc_select_input(0); // Usa um dos canais do ADC
    uint16_t random_seed = adc_read();
    srand(random_seed);
}
```
**Função para tocar som:** essa função utiliza o PWM para tocar um som específico por um tempo determinado. Ela recebe três parâmetros: o pino que irá tocar o som, a frequência e a duração do som.
```c
void play_tone(uint pin, uint frequency, uint duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1;
 
    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, top / 2); // 50% de duty cycle
 
    sleep_ms(duration_ms);
 
    pwm_set_gpio_level(pin, 0); // Desliga o som após a duração
    sleep_ms(50); // Pausa entre notas
}
```
**Funções LED:** aqui serão mostradas um conjunto de funções que são responsáveis por todas as movimentações que serão feitas com os LEDs da matriz.

A primeira função é a ```npClear()```. Essa função deixa todos os LEDs sem cor;
```c
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}
```
A segunda função é a ```npSetLED()´´´ que atribui uma cor ao LED RGB.
```c
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}
```
A terceira função (```npWrite()```) ativa a cor escolhida na função anterior no LED correspondente.
```c
void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100);
}
```
A última função é responsável por ligar todos os LEDs da matriz ao mesmo tempo. É utilizada ao final do jogo, quando todos os LEDs ficam vermelhos.
```c
void turn_on_all_leds() {
    printf("Todos os LEDs devem acender!\n");
    for (int i = 0; i < LED_COUNT; i++) {
        npSetLED(i, 255, 0, 0); // Vermelho
    }
    npWrite(); // Garante que todos os LEDs são atualizados após a alteração
}
```  

**Função jogo:** essa função é a que contém a lógica principal do jogo. Por ser grande, a explicação irá ser por partes.

A primeira parte da função é responsável por definir valores importantes que irão ser utilizados no restante do código. 
```c
    static int last_index = -1;  // Armazena o último índice selecionado pelo joystick
    int periodo_ms = 500; // Tempo de piscar em ms
    int x = 4 - map_adc_to_matrix(vrx_value); // valor subtraído de 4 para evitar erro de inversão de direção
    int y = map_adc_to_matrix(vry_value);
    int index = converte_coord_led(x, y);
```
Logo após, o código verifica se a variável ```bomba_explodiu``` é falsa ou verdadeira. Caso seja verdadeira, a função finaliza e, consequentemente, o jogo também.
```c
  if (bomba_explodiu) {
        return;  // Sai da função se a bomba já explodiu
    }
```
Depois, caso o valor de ```ìndex``` seja válido, o código apaga os LEDs que não são permanentes. Essa parte garante que o joystick possa selecionar um LED por vez e depois, 
parte para outras verificações. O código dá o comando de piscar os LEDs selecionados que não sejam permanentes, ou seja, somente o selecionado pelo joystick. Depois, o código verifica
se o botão A foi pressionado, comando responsável por deixar um LED permanente. Aqui, temos a principal lógica do programa: se o LED que foi selecionado para ser permanente não for 
a bomba, ele fica permanentemente verde; se ele for a bomba, todo o teclado fica vermelho. 

Alguns avisos sonoros também foram adicionados. Caso o LED não seja a bomba, o aviso sonoro serve como uma confirmação de que o LED já foi registrado como permanente. Já no cenário 
em que a bomba é selecionada, o aviso serve como um alerta de que o jogo acabou.
```c
    if (index != -1) {
        if (last_index != -1 && last_index != index && !permanent_leds[last_index]) {
            npSetLED(last_index, 0, 0, 0); 
        }

        // Apenas pisca se não for permanente
        if (!permanent_leds[index]) {
            static bool estado = false;
            estado = !estado;

            if (estado) {
                npSetLED(index, 0, 0, 1); // Azul piscando
            } else {
                npSetLED(index, 0, 5, 0); // Verde
            }
            npWrite();
            sleep_ms(periodo_ms);
        }

        // Verifica se o botão foi pressionado para tornar o LED permanente
        if (gpio_get(botaoA) == 0) { 
            sleep_ms(50); // Debounce
            if (gpio_get(botaoA) == 0) {
                printf("Botão pressionado no LED %d\n", index);
                if (!permanent_leds[index]) { // Apenas torna permanente se não for já permanente
                    permanent_leds[index] = true;
                    npSetLED(index, 0, 5, 0); // Deixa o LED verde
                    play_tone(BUZZER_PIN, 50, 100); // Aviso sonoro para confirmar a seleção do LED
                    npWrite();
                    
                    if (index == bomba) {
                        bomba_explodiu = true;
                        printf("Bomba ativada! Todos os LEDs devem ficar vermelhos.\n");
                        turn_on_all_leds();
                        play_tone(BUZZER_PIN, 150, 1000); // Som do jogo finalizando
                    }
                }
```

**Loop principal:** ao final do código, temos a parte principal que une todas as funções já definidas anteriormente.
```c
int main() {
    // Inicializa a biblioteca padrão do Pico
    stdio_init_all();
    
    init_random_seed();
    setup_joystick();
    stdio_init_all();
    npInit(LED_PIN);
    npClear();
    pwm_init_buzzer(BUZZER_PIN);
    sleep_ms(2000);
    int bomba = escolhe_bomba();


    while (1) {
        uint16_t vrx_value, vry_value;
        joystick_read_axis(&vrx_value, &vry_value);  // Lê os valores do joystick
        jogo(vrx_value, vry_value, bomba);     // Chama a função de piscar o LED
        sleep_ms(100); // Pequeno delay para evitar instabilidades na leitura do joystick
    }

    return 0;
}
```
