/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Miles Bollin, Jason Lam
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include <stdint.h>

// mode registers
#define GPIOA_MODER         (*(volatile unsigned int *)0x50000000)		// set/config I/O modes
#define GPIOA_AFRH          (*(volatile unsigned int *)0x50000024)		// alt function (low(0-7)/high(8-16))
#define GPIOA_ODR           (*(volatile unsigned int *)0x50000014)  // output data reg A
#define GPIOA_OTYPER        (*(volatile unsigned int *)0x50000004)		// output type (push-pull/open drain)
#define GPIOA_PUPDR         (*(volatile unsigned int *)0x5000000C)   // pull-up, pull-down register
#define GPIOA_IDR           (*(volatile unsigned int *)0x50000010)  // Input data register Port A

#define GPIOB_MODER			(*(volatile unsigned int *)0x50000400) // set and configure pins to I/O (port B)
#define GPIOB_PUPDR			(*(volatile unsigned int *)0x5000040C) // set and configure pins to I/O (port B)
#define GPIOB_ODR           (*(volatile unsigned int *)0x50000414)  // output data reg B
#define GPIOB_IDR       	(*(volatile unsigned int *)0x50000410)  // Input data register
#define GPIOB_OTYPER        (*(volatile unsigned int *)0x50000404)		// output type (push-pull/open drain)
#define GPIOB_AFRH          (*(volatile unsigned int *)0x50000424)		// alt function (low(0-7)/high(8-16))
#define GPIOB_AFRL          (*(volatile unsigned int *)0x50000420)


#define RCC_IOPENR          (*(volatile unsigned int *)0x40021034)		// enable GPIO ports
#define RCC_APBENR1         (*(volatile unsigned int *)0x4002103C)	// enable peripherals
#define RCC_APBENR2         (*(volatile unsigned int *)0x40021040) 	// offset 0x40

//i2c regs
#define I2C1_CR1            (*(volatile unsigned int *)0x40005400)		// CR1 & CR2 are control registers (start/stop/read/write)
#define I2C1_CR2            (*(volatile unsigned int *)0x40005404)
#define I2C1_TIMINGR        (*(volatile unsigned int *)0x40005410)		// timing register (speed settings)
#define I2C1_ISR            (*(volatile unsigned int *)0x40005418)		// status register (flags for read/write complete etc)
#define I2C1_ICR            (*(volatile unsigned int *)0x4000541C)		// interrupt clear register
#define I2C1_RXDR           (*(volatile unsigned int *)0x40005424)		// receive data register
#define I2C1_TXDR           (*(volatile unsigned int *)0x40005428)		// transmit data register

// timer registers
#define TIM2_CR1			(*(volatile unsigned int *)0x40000000)	// enable counter base address 0x4000000
#define TIM2_ARR			(*(volatile unsigned int *)0x4000002C)	// offset 0x2C PWM clock 16MHz / N
#define TIM2_CCER			(*(volatile unsigned int *)0x40000020)	// offset 0x20 Enable TIM2_CHx
#define TIM2_CCMR1			(*(volatile unsigned int *)0x40000018)	// offset = 0x18	select PWM mode
#define TIM2_CCR2			(*(volatile unsigned int *)0x40000038)	// offset 0x38  capture and compare channel 2
#define TIM2_PSC            (*(volatile unsigned int *)0x40000028)  // offset 0x28 clock prescalar
#define TIM2_CNT        	(*(volatile unsigned int *)0x40000024)  // Offset 0x24 - counter value
#define TIM2_EGR			(*(volatile unsigned int *)0x40000014)

// ADC registers
#define ADC_CR              (*(volatile unsigned int *)0x40012408)      // offset 0x08
#define ADC_CFGR1           (*(volatile unsigned int *)0x4001240C)      // offset 0x0C
#define ADC_CHSELR          (*(volatile unsigned int *)0x40012428)      // offset 0x28 channel selection
#define ADC_DR              (*(volatile unsigned int *)0x40012440)      // offset 0x40 data reg (reads converted data)
#define ADC_ISR             (*(volatile unsigned int *)0x40012400)      // BASE 0x4001 2400

// data mask
#define ADSTART (1<<2)  // start ADC conversion
#define ADEN    (1<<0)  // enable ADC
#define EOC (1<<2)      // flag f/end of conversion

// i2c Flags
#define ISR_TXIS (1 << 1)	// wait until TXDR is empty
#define ISR_TC (1<<6)		// wait for transfer complete
#define ISR_RXNE (1<<2)		// wait until RXDR not empty
#define ISR_STOPF (1<<5)	// wait for stop flag
#define ICR_STOPCF (1<<5)	// clear stop flag
#define CR2_START (1<<13)	// start i2c
#define CR2_STOP (1<<14)	// stop i2c
#define CR2_RNW (1 << 10)	// read not write
#define CR2_ASTOP (1<<25)	// auto stop after read

// CHAR LCD
#define LCD_RS (1<<0)   // GPB0
#define LCD_EN (1<<1)   // GPB1
#define LCD_BL (1 << 7) //back light?
int lcd_ctrl = 0x00;

// MCP23017 registers
#define IO_IODIRA 0x00      // set port as in/out
#define IO_IODIRB 0x01
#define IO_GPIOA 0x12   // used to read from/write to pin
#define IO_GPIOB 0x13

#define ioADDR 0x20		// MCP23017 address


//OHMMETER CONSTANTS
#define V_IN            3.308f   // Input voltage
#define R_REF           985.0f   // Reference resistor value in ohms

//CAPACITANCE METER CONSTANTS
#define R_CAP           33000.0f    // Use your measured value!
#define CHARGE_PIN      8            // PA8, or change to free pin
#define V_THR           (0.632f * V_IN)

// mode
// Equipment operating modes
enum equipMode {
    ohmmeter = 1,
    capacitance_meter = 2,
    powerSupply = 3
};

// enable the timer for capacitance meter
void enableTIM() {
	TIM2_EGR |= (1<<0);                       // set timer to 0
    TIM2_PSC = 16 - 1;                        // Prescaler: 16MHz / 16 = 1MHz (1µs per tick)
    TIM2_ARR = 0xFFFFFFFF;                    // Maximum count (32-bit) for capacitance meter
    TIM2_CR1 = 1;                             // Enable counter
}

//keypad variables and defs

//define and map keypad characters
const char key_map[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

void wait_ms(int ms) {	// custom wait function in mS, reworked to be more efficient
	int i,j;
	for(i=0;i<ms;i++) for(j=0;j<1340;j++);
}

// i2c functions

// int addr: device address; int reg: specific location wihtin device; int data: data to write
void i2c_write(int addr, int reg, int data) {
	I2C1_CR2 = (addr << 1) | (2 << 16) | CR2_START;     // currently set up to always write two bytes
	while(!(I2C1_ISR & ISR_TXIS));                      // wait for TXIS to clear
	I2C1_TXDR = reg;
	while(!(I2C1_ISR & ISR_TXIS));
	I2C1_TXDR = data;
	while(!(I2C1_ISR & ISR_TC));
	I2C1_CR2 |= CR2_STOP;
	while(!(I2C1_ISR & ISR_STOPF));                     // what for stop flag
	I2C1_ICR = ICR_STOPCF;
}

// LCD helper functions

// turn on lcd and prepare for writing
void lcd_pulse_en() {
    lcd_ctrl |= LCD_EN;
    i2c_write(ioADDR, IO_GPIOB, lcd_ctrl);  //enable high
    wait_ms(1);
    lcd_ctrl &= ~LCD_EN;
    i2c_write(ioADDR, IO_GPIOB, lcd_ctrl);  //enable low
    wait_ms(1);
}

// send commands to LCD - int cmd: hex value for LCD commands
void lcd_cmd(int cmd) {
    lcd_ctrl &= ~LCD_RS;
    i2c_write(ioADDR, IO_GPIOB, lcd_ctrl);
    i2c_write(ioADDR, IO_GPIOA, cmd);
    lcd_pulse_en();
    wait_ms(2);
}

// write a single char to LCD
void lcd_writeChar(char c) {
    lcd_ctrl |= LCD_RS;
    i2c_write(ioADDR, IO_GPIOB, lcd_ctrl);
    i2c_write(ioADDR, IO_GPIOA, c);
    lcd_pulse_en();
}

// setup registers to run LCD and set it up for 2 line display w/auto cursor
void lcd_init() {
    i2c_write(ioADDR, IO_IODIRA, 0x00); // IO extender port A to output
    i2c_write(ioADDR, IO_IODIRB, 0x00); // IO extender port B to output

    lcd_ctrl = LCD_BL;  // turn on backlight
    i2c_write(ioADDR, IO_GPIOB, lcd_ctrl);

    wait_ms(50);

    lcd_cmd(0x38);  // 2-line mode
    lcd_cmd(0x0F);  // Disply on, blinking cursor (testing)
    lcd_cmd(0x01);  // Clear display
    wait_ms(5);
    lcd_cmd(0x06);  // enable data in, cursor should automove
}

// Clear LCD screen w/ a long enough wait time to finish
void lcd_clear() {
    lcd_cmd(0x01);
    wait_ms(5);
}

// move cursor to a specified location - int row: select what row to move to; int col: select which column to goto
void lcd_goto(int row, int col) {   //move cursor
    int addr = (row == 0) ? (0x00 + col): (0x40 + col);
    lcd_cmd(0x80 | addr);
}

// write a char array to LCD - const char *s: char array to print a string of alpha-numeric characters
void lcd_writeString(const char *s) {    
    while (*s) {
        lcd_writeChar(*s++);    // iterate char array to next position
    }
}

//test to ensure all keys are properly read - char key: single char to print
void lcd_testKey(char key) {
    lcd_clear();
    lcd_goto(0, 0);
    lcd_writeString("key pressed: ");
    lcd_goto(1, 0);
    lcd_writeChar(key);
}

// read in key strokes from keypad
char keypadScanner() {
    for (int row = 0; row < 4; row++) {
        GPIOA_ODR |= ((1 << 6) | (1 << 7) | (1 << 11));  //drive rows high
        GPIOB_ODR |= (1 << 0);

        switch(row) {
        case 0: {
        	GPIOA_ODR &= ~(1<<6);
        	break;
        }
        case 1: {
        	GPIOA_ODR &= ~(1<<7);
        	break;
        }
        case 2:  {
        	GPIOA_ODR &= ~(1<<11);
        	break;
        }
        case 3:  {
        	GPIOB_ODR &= ~(1<<0);
        	break;
        }
        }
        // set up IDR
        int cols = (((GPIOA_IDR >> 5) & 1) << 0) | (((GPIOA_IDR >> 4) & 1) << 1) | (((GPIOA_IDR >> 1) & 1) << 2) | (((GPIOA_IDR >> 12) & 1) << 3); // extract columns

        if (cols != 0x0F) {
            for (int col = 0; col < 4; col++) {
                if ((cols & (1 << col)) == 0 ) {
                    return key_map[row][col];
                }
            }
        }
    }
    return 0;   //no press
}

// button logic, including debounce
char keypadRead() {
	static char last_key = 0;
	static int counter = 0;
	static int ready = 1;

	char key = keypadScanner();

    if (key == 0){
        counter = 0;
        ready = 1;
        last_key = 0;
        return 0;
    }

	if(key != last_key) {
		counter = 0;
		last_key = key;
		ready = 1;
		return 0;
	}

	if(counter < 1000) counter++;

	if(counter > 5) {
		if(ready && key != 0) {
			ready = 0;
			return key;
		}
	}
	return 0;
}

// analog in/volt convert helper
float readVolt() {
    //ADC_CHSELR = (1 << channel);              // Select channel
    ADC_CR |= ADSTART;                        // Start conversion
    while(!(ADC_ISR & EOC));                  // Wait for conversion complete
    float raw = (ADC_DR & 0xFFFF);              // Read raw value
    return (raw / 4095.0) * V_IN;            // Convert to voltage
}

// extract tens and ones place for temp then writes them to the display - float Ohms: calculated resistance to print
void dispOhm(float Ohms) {    // modifiable write code
	char buffer[16];    //line buffer
    int whole = (int) Ohms;                 // integer left of the decimal
    int dec = (int)((Ohms - whole) * 100);  // extracts two decimal places

    if (dec < 0) dec = -dec;    // prevent negative decimals

    buffer[0] = 0;
    int pos = 0;

    if (whole == 0) {
        buffer[pos++] = '0';
    }
    else {
        char temp[10];
        int tpos = 0;
        while (whole > 0)  {
            temp[tpos++] = '0' + (whole % 10);
            whole = whole / 10;
        }
        while (tpos > 0) buffer[pos++] = temp[--tpos];
    }
    buffer[pos++] = '.';

    // decimal conversion
    buffer[pos++] = '0' + (dec / 10);
    buffer[pos++] = '0' + (dec % 10);

    buffer[pos++] = ' ';
    buffer[pos++] = 0xF4;   // should be the omega symbol   poten 0x4F
    buffer[pos++] = 0;

    lcd_clear();
    lcd_goto(0,0);
    lcd_writeString("Ohmmeter: ");

    lcd_goto(1,0);
    lcd_writeString(buffer);
}

/**
 * @brief Calculate resistance from voltage divider
 * @param vout Measured voltage across test resistor
 * @return Resistance in ohms
 */
float calcOhm(float vout) {
    // Error checks
    if(vout < 0.01f) return 0.0f;             
    if(vout >= (V_IN - 0.01f)) return 0.0f;   

    // Voltage divider formula: R_test = (Vout * R_ref) / (Vin - Vout)
    return (vout * R_REF) / (V_IN - vout);
}

// run ohmmeter subprogram
void runOhm() {
    float vout = readVolt(0);	// PA 0 = channel 0
    float rTest = calcOhm(vout);

    dispOhm(rTest);
}

// set PWM output to a specific duty cycle - int cycle: calculated duty cycle (percentage) for desired output
void setDuty (int cycle) {
    if (cycle > TIM2_ARR) cycle = TIM2_ARR;
    TIM2_CCR2 = cycle;
}

// calculate duty cycle from user input; includes rounding logic to ensure 0.05 step size - float input: user input from keypad
float setPwr(float input) {
	if (input < 0.0f) input = 0.0f;
	if (input > 3.3f) input = 3.3f;

	int steps = (int)((input / 0.05f) + 0.5f);
	float voltage = steps * 0.05f;

	int dutyCycle = (int)((voltage / 3.3f) * 255);	//255
	setDuty(dutyCycle);

	return voltage;
}

// configure PA8 for charging the capacitor
void cap_initChargePin() {
    GPIOA_MODER &= ~(3 << (CHARGE_PIN*2));	//pa8
    GPIOA_MODER |=  (1 << (CHARGE_PIN*2));
}

// configure capacitor pins for discharge. Give enough time to propelry dischare
void cap_discharge() {
    // drive PA8 (output) low
    GPIOA_ODR &= ~(1 << CHARGE_PIN);

    //set ADC pin (PB7) low to discharge
    GPIOB_MODER &= ~(3 << (2*2));
    GPIOB_MODER |=  (1 << (2*2));
    GPIOB_ODR &= ~(1 << 2);
    wait_ms(500);
}

// charge the capacitor
void cap_startCharging() {
    GPIOA_ODR |= (1 << CHARGE_PIN);
    GPIOB_MODER &= ~(3 << (2*2));
    GPIOB_MODER |=  (3 << (2*2));
}

uint32_t time_us() {
    return TIM2_CNT;
}

// measure capacitor by reading time to charge
float measureCap() {
    cap_discharge();

    cap_startCharging();

    TIM2_CNT = 0;

    float v = 0;
    uint32_t t = 0;
    int exitCount = 0;

    while(1) {
        v = readVolt();
        t = TIM2_CNT;
        if(v >= V_THR) break;   // safety measure to prevent capacitor over charge
        if(t > 500000) break;   // exit out if timer is too long

        exitCount++;
        if(exitCount > 500000) break;   // backup exit timer
    }

    if(t >= 500000 || exitCount > 50000) return -1.0f;

    float sec = t / 1000000.0;
    return sec / R_CAP;
}

/*
 * @brief Display capacitance value on LCD in microfarads
 * @param x Capacitance in farads
 */
void dispFloatUF(float x) {
    char buf[16];
    float uF = x * 1e6f;                      // Convert to microfarads

    int whole = (int)uF;
    int dec = (int)((uF - whole) * 100);
    if(dec < 0) dec = -dec;

    int pos = 0, tpos = 0;
    char tmp[10];

    // Convert integer part
    if(whole == 0) {
        buf[pos++] = '0';
    }
    else {
        while(whole > 0) {
            tmp[tpos++] = '0' + (whole % 10);
            whole /= 10;
        }
        while(tpos > 0) {
            buf[pos++] = tmp[--tpos];
        }
    }

    // Add decimal and unit
    buf[pos++] = '.';
    buf[pos++] = '0' + (dec / 10);
    buf[pos++] = '0' + (dec % 10);
    buf[pos++] = ' ';
    buf[pos++] = 0xE4;                        // Mu symbol (µ)
    buf[pos++] = 'F';
    buf[pos] = 0;

    // Display on second line
    lcd_goto(1, 0);
    lcd_writeString(buf);
}

/**
 * @brief Run one capacitance measurement cycle
 */
void runCap() {
    float C = measureCap();                   // Measure capacitance

    lcd_clear();
    lcd_goto(0, 0);
    lcd_writeString("Capacitance:");

    dispFloatUF(C);
}

// logic for power supply subprogram
// takes in 3 digits and places a decimal point after the first entry
void runPwrSup() {
    char key;
    int digitCount = 0;
    int inputValue = 0;
    float previewVoltage = 0.0f;
    float targetVoltage = 0.0f;
    char buffer[16];

    setPwr(0.0f);

    lcd_clear();
    lcd_writeString("Power Supply:");
    lcd_goto(1,0);
    lcd_writeString("0.00 V");

    while (1) {
        key = keypadRead();

        if (key =='#') {    // exit logic
            setPwr(0.0f);
            return;
        }

        // Only digits allowed for voltage input
        if (key >= '0' && key <= '9') {
            inputValue = inputValue * 10 + (key - '0');
            digitCount++;

            // show user input
            if (digitCount == 1) {
                // Example: entering "1" -> preview = 1.00
                int d1 = inputValue;
                previewVoltage = d1 * 1.0f;
            }
            else if (digitCount == 2) {
                // Example: "15" -> preview = 1.5
                int d1 = inputValue / 10;
                int d2 = inputValue % 10;
                previewVoltage = d1 + (d2 / 10.0f);
            }
            else if (digitCount == 3) {
                // Example: "158" -> preview = 1.58
                int d1 = inputValue / 100;
                int d2 = (inputValue / 10) % 10;
                int d3 = inputValue % 10;
                previewVoltage = d1 + ((d2 * 10 + d3) / 100.0f);
            }

            // Display preview
            int whole = (int)previewVoltage;
            int dec = (int)((previewVoltage - whole) * 100 + 0.5f);

            buffer[0] = '0' + whole;
            buffer[1] = '.';
            buffer[2] = '0' + (dec / 10);
            buffer[3] = '0' + (dec % 10);
            buffer[4] = ' ';
            buffer[5] = 'V';
            buffer[6] = 0;

            lcd_goto(1,0);
            lcd_writeString(buffer);

            if (digitCount == 3) {
                // Round to nearest 0.05V
                float rounded = ((int)(previewVoltage / 0.05f + 0.5f)) * 0.05f;
                if (rounded > 3.3f) rounded = 3.3f;

                targetVoltage = setPwr(rounded);

                // Display final applied voltage
                whole = (int)targetVoltage;
                dec = (int)((targetVoltage - whole) * 100 + 0.5f);

                buffer[0] = '0' + whole;
                buffer[1] = '.';
                buffer[2] = '0' + (dec / 10);
                buffer[3] = '0' + (dec % 10);
                buffer[4] = ' ';
                buffer[5] = 'V';
                buffer[6] = 0;

                lcd_goto(1,0);
                lcd_writeString(buffer);

                // Reset for next input
                digitCount = 0;
                inputValue = 0;
            }
        }
        wait_ms(10);
    }
}

// setup keypad pins
void enableKeys() {
    //  (rows)
    GPIOA_MODER &= ~(3 << (7*2));    // clear pins f/output
    GPIOA_MODER |= (1 << (7*2));     // set half our connected pins to output
    GPIOA_MODER &= ~(3 << (6*2));
    GPIOA_MODER |= (1 << (6*2));
    GPIOA_MODER &= ~(3 << (11*2));
    GPIOA_MODER |= (1 << (11*2));
    GPIOB_MODER &= ~(3 << (0*2));
    GPIOB_MODER |= (1 << (0*2));

    // (columns)
    GPIOA_MODER &= ~(3 << (5*2));    // clear pins f/input
    GPIOA_MODER &= ~(3 << (4*2));
    GPIOA_MODER &= ~(3 << (1*2));
    GPIOA_MODER &= ~(3 << (12*2));

    // set pull up f/ input pins
    GPIOA_PUPDR &= ~(3 << (5*2));
    GPIOA_PUPDR |= (1 << (5*2));
    GPIOA_PUPDR &= ~(3 << (4*2));
    GPIOA_PUPDR |= (1 << (4*2));
    GPIOA_PUPDR &= ~(3 << (1*2));
    GPIOA_PUPDR |= (1 << (1*2));
    GPIOA_PUPDR &= ~(3 << (12*2));
    GPIOA_PUPDR |= (1 << (12*2));
}

// setup pins
void enableReg() {
	RCC_IOPENR |= (1<<0); // GPIOA
	RCC_IOPENR |= (1<<1); // GPIOB
	RCC_APBENR1 |= (1 << 21); // I2C1
    RCC_APBENR1 |= (1 << 0);  // Enable TIM2 Clock

    // Configure PA9, PA10 for AF6 (I2C)
	GPIOA_MODER &= ~((3<<(9*2)) | (3<<(10*2)));    // reset pins 9/10
	GPIOA_MODER |= ((2<<(9*2)) | (2<<(10*2)));     // set alt function
	GPIOA_OTYPER |= (1<<9) | (1<<10);       // over drain
	GPIOA_AFRH &= ~((1<<4) | (1<<8));   //clear alt function pins
	GPIOA_AFRH |= ((6<<4) | (6<<8));        // set alt function 6
}

// setup for i2c
void enableI2C() {      // setup i2c speed
	I2C1_CR1 = 0x00000000;  // pe disable f/clock setting
	I2C1_TIMINGR = 0x00303D5B; // ~100kHz
	I2C1_CR1 = (1<<0);  //PE endable after freq set
}

/**
 * @brief Display main menu on LCD
 */
void showMenu() {
    lcd_clear();
    lcd_goto(0, 0);
    lcd_writeString("Select Mode:");
    lcd_goto(1, 0);
    lcd_writeString("A=Ohm B=Cap C=Pwr");
}

// setup A to D conveter - int channel: select a specific channel to use
void enableADC(int channel) {
    RCC_APBENR2 |= (1 << 20);    // enable ADC clock
    // ADC volt regulator
    ADC_CHSELR = (1 << channel);  // selected channel f/input channel 0
    ADC_CR |= (1 << 28);    // volt regulator
    wait_ms(1);
    // ADC cal
    ADC_CR |= (1 << 31);    // start cal
    while (ADC_CR & (1<<31));
    // config ADC options as req'd
    ADC_CR |= ADEN;   // enable ADC
    while (!(ADC_ISR & 1)); //wait f/ADRDY
}

// configure system timer for power supply
void enablePWM() {
	TIM2_EGR |= (1<<0);                       // reset timer to 0
    TIM2_PSC = 16 - 1;                        // Prescaler: 16MHz / 16 = 1MHz
    TIM2_ARR = 255;                           // Period for PWM (256 steps)
    TIM2_CCMR1 |= (6 << 12);                  // PWM mode 1 on channel 2
    TIM2_CCER |= (1 << 4);                    // Enable channel 2 output
    TIM2_CR1 |= (1 << 0);                     // Enable counter

    TIM2_CCR2 = 0;
    // pwr supply output PB3
    // clear PB3 and set for AF mode
    GPIOB_MODER &= ~(3 << (3*2));
    GPIOB_MODER |= (2 << (3*2));

    GPIOB_OTYPER &= ~(1 << 3);

    GPIOB_AFRL &= ~(0xF << (3*4));
    GPIOB_AFRL |= (2 << (3*4));
}

//main
int main(void) {
    // Initialize
    enableReg();
    enableI2C();
    enableTIM();
    lcd_init();
    cap_initChargePin();
    enableKeys();

    char key;

    // Main program loop
    while(1) {
        // Display menu and wait for mode selection
        showMenu();
        while((key = keypadRead()) == 0);

        // Mode A: Ohmmeter
        if(key == 'A') {
            // Reconfigure timer for capacitance measurements
        	enableADC(0);                       // configure ADC for channel 0 (PA0)
            TIM2_CR1 = 0;                       // Disable timer
            enableTIM();                        // Reconfigure for microsecond counting

            while(1) {
                runOhm();
                wait_ms(500);
                if(keypadRead() == '#') break;    // Exit on '#'
            }
        }

        // Mode B: Capacitance Meter
        else if(key == 'B') {
            // Reconfigure timer for capacitance measurements
        	enableADC(10);
            TIM2_CR1 = 0;                     // Disable timer
            //wait_ms(50);
            enableTIM();                       // Reconfigure for microsecond counting

            while(1) {
                runCap();
                wait_ms(200);
                if(keypadRead() == '#') break;    // Exit on '#'
            }
        }

        // Mode C: PWM Power Supply
        else if(key == 'C') {                   // exit logic is internal to runPwrSup function
            enablePWM();                       // Reconfigure for timer PWM mode
            runPwrSup();
        }
    }
}
/* =============================================================================
   HARDWARE PIN MAPPING

   GPIO EXTENDER (MCP23017) PIN MAPPING FOR LCD:
   DATA BUS (8-bit):
     D0-D7 = GPA0-GPA7 (MCP23017 Port A)

   CONTROL:
     RS = GPB0
     E  = GPB1
     RW is tied LOW permanently (write-only mode)
     BL = GPB7 (Backlight)

   KEYPAD PIN MAPPING:
   Rows (Output):
     Row 0 = PA6
     Row 1 = PA7
     Row 2 = PA11
     Row 3 = PB0

   Columns (Input with pull-up):
     Col 0 = PA5
     Col 1 = PA4
     Col 2 = PA1
     Col 3 = PA12

   MEASUREMENT INPUTS:
     Ohmmeter ADC Input = PA0 (ADC_IN0)		// N conflict
     Capacitance ADC Input = PB2 (ADC_IN10)

   PWM OUTPUT:
     Power Supply Output = PB3
   I2C PINS:
     SCL = PA9 (AF6)
     SDA = PA10 (AF6)
============================================================================= */
// KNOWN ISSUES:
// program sometimes freezes when switching out of ohmmeter and capacitance meter
// ohmeter and capacitance exit logic is inconsistant
// most likely causes are where the exit logic is, poten. fix may be too robust to complete in time for demo
