#include <stdio.h> // Biblioteca padrão de C
#include "hardware/adc.h" // Biblioteca do ambiente Raspberry Pi Pico para trabalhar com ADC
#include "hardware/pwm.h"// Biblioteca do ambiente Raspberry Pi Pico para trabalhar com PWM
#include "pico/stdlib.h" // Biblioteca do ambiente Raspberry Pi Pico que é responsável por funções básicas, como entrada/saída, dalys, GPIO
#include "ws2818b.pio.h" // Arquivo reponsável pela inicialização de dispositivos PIO
#include "hardware/clocks.h" // Biblioteca do ambiente Raspberry Pi Pico que controla os clocks
#include <stdlib.h> // Biblioteca padrão de C que fornece funções de memória dinâmica
#include <time.h> // Biblioteca padrão de C que lida com funções relacionadas a tempo

// Definição dos pinos 
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

// Definição de pixel GRB
struct pixel_t {
    uint8_t G, R, B;
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t;

// Declaração do buffer de pixels
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO
PIO np_pio;
uint sm;

// Inicializa a máquina PIO para controle da matriz de LEDs
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

// Função para configurar o joystick
void setup_joystick() {
    adc_init();
    adc_gpio_init(VRX);
    adc_gpio_init(VRY);
    gpio_init(botaoA);
    gpio_set_dir(botaoA, GPIO_IN);
    gpio_pull_up(botaoA);
}

// Função para ler os valores dos eixos do joystick
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value) {
    adc_select_input(ADC_CHANNEL_0);
    sleep_us(2);
    *vrx_value = adc_read();
    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    *vry_value = adc_read();
}

void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); // Ajusta divisor de clock
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente
}

// Atribui uma cor RGB a um LED
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

// Limpa o buffer de pixels
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

// Escreve os dados do buffer nos LEDs
void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100);
}

void turn_on_all_leds() {
    printf("Todos os LEDs devem acender!\n");
    for (int i = 0; i < LED_COUNT; i++) {
        npSetLED(i, 255, 0, 0); // Vermelho
    }
    npWrite(); // Garante que todos os LEDs são atualizados após a alteração
}

// Função para mapear valores do ADC para o intervalo de 0 a 4
int map_adc_to_matrix(uint16_t adc_value) {
    return (adc_value * 5) / 4096;
}

// Função para converter coordenadas (x, y) em índice do LED
int converte_coord_led(int x, int y) {
    if (x >= 0 && x < 5 && y >= 0 && y < 5) {
        return led_index[x][y];
    } else {
        return -1; // Retorna -1 para coordenadas inválidas
    }
}

int escolhe_bomba(){
    int x = rand() % 5;
    int y = rand() % 5;
    sleep_ms(1000);
    printf("Bomba: %d (x=%d, y=%d)\n", led_index[x][y], x, y);
    return led_index[x][y];
}

void init_random_seed() {
    adc_init();
    adc_select_input(0); // Usa um dos canais do ADC
    uint16_t random_seed = adc_read();
    srand(random_seed);
}

// Toca uma nota com a frequência e duração especificadas
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


void jogo(uint16_t vrx_value, uint16_t vry_value, int bomba) {
    static int last_index = -1;  // Armazena o último índice selecionado pelo joystick
    int periodo_ms = 500; // Tempo de piscar em ms
    int x = 4 - map_adc_to_matrix(vrx_value);
    int y = map_adc_to_matrix(vry_value);
    int index = converte_coord_led(x, y);
    
    if (bomba_explodiu) {
        return;  // Sai da função se a bomba já explodiu
    }

    if (index != -1) {
        // Se o LED anterior não for permanente, restaura o estado sem cor
        if (last_index != -1 && last_index != index && !permanent_leds[last_index]) {
            npSetLED(last_index, 0, 0, 0);  // Volta a estar desligado
        }

        // Apenas pisca se não for permanente
        if (!permanent_leds[index]) {
            static bool estado = false;
            estado = !estado;

            if (estado) {
                npSetLED(index, 0, 0, 1); // Azul piscando
            } else {
                npSetLED(index, 0, 5, 0); // Verde piscando
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
                    npSetLED(index, 0, 5, 0); // Verde (LED fixo)
                    play_tone(BUZZER_PIN, 50, 100);
                    npWrite();
                    
                    if (index == bomba) {
                        bomba_explodiu = true;
                        printf("Bomba ativada! Todos os LEDs devem ficar vermelhos.\n");
                        turn_on_all_leds();
                        play_tone(BUZZER_PIN, 150, 1000);
                    }
                }
            }
        }

        // Atualiza o último índice selecionado
        last_index = index;
    } else {
        printf("Coordenadas inválidas!\n");
    }
}


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