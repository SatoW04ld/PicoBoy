#include "bus.h"
#include "cart.h"
#include "cpu.h"
#include "incbin.h"
#include "hardware/timer.h"
#include "hardware/vreg.h"
#include "pico/stdio.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "st7789.hpp"
#include "hardware/pll.h"
#include "hardware/gpio.h"
#include "cpu.cpp"
#include <chrono>
#include <thread>

extern "C" INCBIN(Rom, "gameboy.gb"); 

using namespace std::chrono_literals;
using namespace std::chrono;

bool frameDone = false;
unsigned long totalInstructions = 0;

using namespace pimoroni;

static cart* cartridge;
static ppu* PPU;
static bus* Bus;
static cpu* CPU;


bool CPU_active = false;
int totalFrames = 0;

#define PLL_SYS_KHZ (133 * 1000)

int main()
{     
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    set_sys_clock_khz(420000, true);

    clock_configure(
    clk_peri,
    0,                                                // No glitchless mux
    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
    PLL_SYS_KHZ * 1000,                               // Input frequency
    PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
    );

    stdio_init_all();

    int frames = 0;

    uint16_t* buffer = new uint16_t[240 * 240];

    cartridge = new cart( (uint8_t*)gRomData, (uint32_t)gRomSize);
   
    PPU = new ppu();

    Bus = new bus(cartridge, PPU);
    CPU = new cpu(Bus);
    ST7789 lcd(240, 240, buffer);

    PPU->connectBus(Bus);
    PPU->connectCPU(CPU);
    Bus->connectCPU(CPU);

    for(int i = 0; i < 57600; i++)
    {
        buffer[i] = 0;
    }

    lcd.init(true, false, 62500000UL, 40, 199, 48, 191);

    lcd.update();

    PPU->frameBuffer = buffer;

    cartridge->printCart();

    gpio_init(0); //directions
    gpio_init(1);
    gpio_init(2);
    gpio_init(3);
    
    gpio_set_function(12, GPIO_FUNC_SIO); //a on board / start
    gpio_set_function(13, GPIO_FUNC_SIO); //b on board / select
    gpio_set_function(14, GPIO_FUNC_SIO); //x on board / a
    gpio_set_function(15, GPIO_FUNC_SIO); //y on board / b

    gpio_set_dir(0, false);
    gpio_set_dir(1, false);
    gpio_set_dir(2, false);
    gpio_set_dir(3, false);

    gpio_set_dir(12, GPIO_IN);
    gpio_set_dir(13, GPIO_IN);
    gpio_set_dir(14, GPIO_IN);
    gpio_set_dir(15, GPIO_IN);

    gpio_pull_down(0);
    gpio_pull_down(1);
    gpio_pull_down(2);
    gpio_pull_down(3);

    gpio_pull_up(12);
    gpio_pull_up(13);
    gpio_pull_up(14);
    gpio_pull_up(15);

    while(true)
    {
        uint64_t start = time_us_64();
        while(!PPU->frameDone)
        {             
            CPU->execOP();
            CPU->updateTimers();
            PPU->tick();
    
            CPU->cycles = 0;

            totalInstructions++;
            if(!(totalInstructions % 1000))
            {
                if(gpio_get(0)) //up
                {
                    Bus->joypad_state &= 0b11111011;
                    CPU->IF |= 0b00010000;
                } else {
                    Bus->joypad_state |= 0b00000100;
                }

                if(gpio_get(1)) //right
                {
                    Bus->joypad_state &= 0b11111110;
                    CPU->IF |= 0b00010000;
                } else {
                    Bus->joypad_state |= 0b00000001;
                }

                if(gpio_get(2)) //down
                {
                    Bus->joypad_state &= 0b11110111;
                    CPU->IF |= 0b00010000;
                } else {
                    Bus->joypad_state |= 0b00001000;
                }

                if(gpio_get(3)) //left
                {
                    Bus->joypad_state &= 0b11111101;
                    CPU->IF |= 0b00010000;
                } else {
                    Bus->joypad_state |= 0b00000010;
                }

                if(!gpio_get(12))//start
                {
                    Bus->joypad_state &= 0b01111111;
                    CPU->IF |= 0b00010000;
                } else {
                    Bus->joypad_state |= 0b10000000;
                }

                if(!gpio_get(13))//select
                {
                    Bus->joypad_state &= 0b10111111;
                    CPU->IF |= 0b00010000;
                } else {
                    Bus->joypad_state |= 0b01000000;
                }

                if(!gpio_get(14))
                {
                    Bus->joypad_state &= 0b11101111;
                    CPU->IF |= 0b00010000;
                } else {
                    Bus->joypad_state |= 0b00010000;
                }

                if(!gpio_get(15))
                {
                    Bus->joypad_state &= 0b11011111;
                    CPU->IF |= 0b00010000;
                } else {
                    Bus->joypad_state |= 0b00100000;
                }

            }

        }
        uint64_t stop = time_us_64();    
        uint64_t length = stop - start;

        printf("Emulation: %llu\n", length);

        frameDone = true;
        totalFrames++;

        start = time_us_64();
            
        lcd.update();  

        stop = time_us_64();
        length = stop - start;

        printf("CLK_SYS: %u\n", clock_get_hz(clk_sys));
        printf("Screen: %llu\n", length);

        PPU->frameDone = false;
            
        frames++;    

    }

    return 0;   
}