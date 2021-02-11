/*
Ver. 1.01 - 2020-05-06

Changelog:
1.01 - 2020-05-06 - Recofigured pins in setup(). Attempt to fix a bug for Simone (buttons stop working when wheel is rotated quickly, T300RS base)
1.00 - 2016-02-26 - Initial, for Arduino IDE 1.6.3

This sketch emulates Thrustmaster T300 PlayStation stock wheel
allowing to connect arduino to T300 wheelbase (may work for TXRW as well)
and emulate button presses - up to 15 buttons can be connected to arduino (because stock PS wheel has 15 buttons)

Thrustmaster Wheelbase cable pinout (PS/2 connector, also known as mini-Din 6pin female):
1 - Green    - not used
2 - Blue     - GND (ground)
3 - White    - MISO (master in, slave out - to read the data from the wheel)
4 - Orange   - SS (slave select, or PL - parallel load, set it to 0 when you want to read the data)
5 - Red      - CLK (clock impulses)
6 - Black    - +VCC (+3.3 volts! if you have Arduino 5V version - use external +3.3V power or use the wheelbase power!!!)

Arduino UNO pins -> T300 wheelbase cable pins

Arduino GND                 -> T300 Blue wire (2)
Arduino pin 12              -> T300 White wire (3) (data from wheel to base)
Arduino pin 10 + pin 2 (SS) -> T300 Orange wire pin (4) (yes, on Arduino it's wired to two pins. 10 - SS, 2 - INT0)
Arduino pin 13 (SCK)        -> T300 Red wire (5)
Arduino +5V                 -> T300 Black wire (6) (it gives 3.3V, but must be connected into +5V socket on arduino uno side)

Button mappings (we send 8 bytes to wheelbase, only 3 first bytes matter, only 15 bits are used for buttons)

Byte 1
  1 - not used
  1 - n/u
  0 - n/u
  1 - n/u
  0 - n/u
  0 - n/u
  0 - n/u
  1 – n/u
  
Byte 2
  1 – n/u
  1 – x (cross, button 6)                 ->   pin 7  (portD bit7)
  1 – o (circle, button 5)                ->   pin 6  (portD bit6)
  1 – [] (square, button 4)               ->   pin 9  (portB bit1)
  1 – options (button 8)                  ->   pin 0  (portD bit0) - if you don't use serial debug (UART RX)
  1 – R1 (Gear Up, button 2)              ->   pin 8  (portB bit0)
  1 – Triangle (button 3)                 ->   pin 11 (portB bit3)
  1 – R2 (button 9)                       ->   pin 4  (portD bit4)

Byte 3
  1 – Share (button 7)                    ->   pin 1  (portD bit1) - if you don't use serial debug (UART TX)
  1 – PS Button (Menu, button 13)         ->   pin A5 (portC bit5)
  1 – D-Pad Down (button 11)              ->   pin A3 (portC bit3)
  1 – L1 (Gear Down, button 1)            ->   pin 3  (portD bit3)
  1 – D-Pad Right                         ->   pin A2 (portC bit2)
  1 - D-Pad Left                          ->   pin A1 (portC bit1)
  1 - D-Pad Up                            ->   pin A0 (portC bit0)
  1 - L2 (button 10)                      ->   pin A4 (portC bit4)
  
Ah, each push button has 2 pins - you connect one to Arduino, another to the Ground :)
Pressed button gives you "0" (grounded signal), released = "1" (pulled-up to +VCC)

Feel free to modify and use for your needs.
This sketch and the documentation above provided "AS IS" under the BSD New license.
http://opensource.org/licenses/BSD-3-Clause

(c) Taras Ivaniukovich (blog@rr-m.org) May 2020
https://rr-m.org/blog/

Credits:
Deren 20 for reading his T300 PS wheel and testing.
Bryan Y for testing with T500RS base
Simone S for testing and bugfix
*/

byte wheelState [8]; // local push-buttons state saved here
volatile byte pos;

void setup (void) {
  DDRB  &= B11110000; // digital pins 8,9,11 used as inputs. 10 - SS also input
  PORTB |= B00001011; // 8,9,11 - pull-up to +VCC via internal 100k resistors.

  DDRC  &= B11000000; // pins 14-19 (A0 - A5) also used as digital inputs
  PORTC |= B00111111; // pulled-up to +VCC via internal 100k resistors

  DDRD  &= B00100000; // digital pins 0,1,3,4,6,7 are inputs for buttons. 2 - also input - for external interrupt 0
  PORTD |= B11011011; // 0,1,3,4,6,7 pulled-up to +VCC via internal 100k resistors
  
  wheelState[0] = B11010001; // T300 PS wheel first data byte
  wheelState[1] = B11111111; // second data byte - buttons
  wheelState[2] = B11111111; // third data byte - buttons
  wheelState[3] = B11111111; // this and below - not used, but wheelbase reads all 8 bytes...
  wheelState[4] = B11111111;
  wheelState[5] = B11111111;
  wheelState[6] = B11111111;
  wheelState[7] = B11111111;
  
  //Serial.begin(9600);    // Arduino debug console - occupies pins RX (0) and TX (1) on Uno
  pinMode(MISO, OUTPUT); // arduino is a slave device
  SPCR |= _BV(SPE);      // Enables the SPI when 1
  SPCR |= _BV(SPIE);     // Enables the SPI interrupt when 1
  
  // interrupt for SS rising edge. Arduino Uno Pin10 must be connected to Pin2!!!
  attachInterrupt (0, ss_rising, RISING);
  
}

// Interrupt0 (external, pin 2) - prepare to start the transfer
void ss_rising () {
  SPDR = wheelState[0]; // load first byte into SPI data register
  pos = 1;
}

// SPI interrupt routine
ISR (SPI_STC_vect) {
  SPDR = wheelState[pos++]; // load the next byte to SPI output register and return.
}

void loop() {
  // scan the button presses and save that to wheelState array. Data transfer to wheelbase is interrupt-driven above.
  //wheelState[0] - always constant for T300 PS wheel
  wheelState[1] = ((PIND & B11000000) >> 1) | ((PINB & B00000010) << 3) | ((PIND & B00000001) << 3) | ((PINB & B00000001) << 2) | ((PINB & B00001000) >> 2) | ((PIND & B00010000) >> 4) | B10000000;
  wheelState[2] = ((PIND & B00000010) << 6) | ((PINC & B00100000) << 1) | ((PINC & B00001000) << 2) | ((PIND & B00001000) << 1) | ((PINC & B00000111) << 1) | ((PINC & B00010000) >> 4);
}
