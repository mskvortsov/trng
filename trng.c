#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <string.h>

static const int ROW_DELAY = 500;
static const int SHOW_CYCLES = 1000;
static const int NEXT_ALLOW_CYCLE = 100;

static uint8_t random_block[8] = { 0 };
static volatile uint8_t random_block_ready = 0;

static uint8_t stir(uint8_t r, uint8_t sample) {
  return ((r << 1) | (r >> 7)) ^ sample;
}

ISR(WDT_OVERFLOW_vect) {
  static uint8_t bits = 0;
  static uint8_t bits_ready = 0;
  uint8_t sample = TCNT1L;

  bits = stir(bits, sample);
  bits_ready += 1;

  if ((bits_ready & 7) == 0) {
    random_block[(bits_ready >> 3) - 1] = bits;
    if (bits_ready == 64) {
      bits_ready = 0;
      random_block_ready = 1;
      wdt_disable();
    }
  }
}

static void reset_row() {
  PORTB &= ~_BV(PB0);
  PORTB &= ~_BV(PB5);
  PORTD &= ~_BV(PD6);
  PORTB &= ~_BV(PB3);
  PORTD &= ~_BV(PD0);
  PORTD &= ~_BV(PD5);
  PORTD &= ~_BV(PD1);
  PORTD &= ~_BV(PD3);
}

static void reset_col() {
  PORTB |= _BV(PB4);
  PORTA |= _BV(PA1);
  PORTA |= _BV(PA0);
  PORTB |= _BV(PB1);
  PORTD |= _BV(PD4);
  PORTB |= _BV(PB2);
  PORTB |= _BV(PB6);
  PORTB |= _BV(PB7);
}

static void set_row(uint8_t r) {
  switch (r) {
    case 0: PORTB |= _BV(PB0); break;
    case 1: PORTB |= _BV(PB5); break;
    case 2: PORTD |= _BV(PD6); break;
    case 3: PORTB |= _BV(PB3); break;
    case 4: PORTD |= _BV(PD0); break;
    case 5: PORTD |= _BV(PD5); break;
    case 6: PORTD |= _BV(PD1); break;
    case 7: PORTD |= _BV(PD3); break;
  }
}

static void set_col(uint8_t c) {
  switch (c) {
    case 0: PORTB &= ~_BV(PB4); break;
    case 1: PORTA &= ~_BV(PA1); break;
    case 2: PORTA &= ~_BV(PA0); break;
    case 3: PORTB &= ~_BV(PB1); break;
    case 4: PORTD &= ~_BV(PD4); break;
    case 5: PORTB &= ~_BV(PB2); break;
    case 6: PORTB &= ~_BV(PB6); break;
    case 7: PORTB &= ~_BV(PB7); break;
  }
}

static void set_cols(uint8_t cols) {
  for (uint8_t i = 0; i < 8; ++i) {
    if (cols & 1) {
      set_col(i);
    }
    cols >>= 1;
  }
}

static void show_random_block() {
  // show the rows one by one
  for (uint8_t r = 0; r < 8; ++r) {
    set_row(r);
    set_cols(random_block[r]);
    _delay_us(ROW_DELAY);
    reset_row();
    reset_col();
  }
}

static volatile uint8_t int0_fired = 0;
ISR(INT0_vect) {
  // disable INT0 interrupt
  GIMSK &= ~_BV(INT0);
  int0_fired = 1;
}

static void wdt_enable_16ms_interrupt() {
  // enable WDT interrupt
  // set the shortest possible timeout of 16ms
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  WDTCSR = 1 << WDIE;
}

static void collect_entropy() {
  // start collecting entropy
  wdt_enable_16ms_interrupt();

  // wait for a new random block and blink
  uint8_t n = 20;
  while (random_block_ready == 0) {
    set_row(7);
    set_col(0);
    _delay_ms(20);
    reset_row();
    reset_col();
    for (int i = 0; i < n; ++i) {
      _delay_ms(20);
    }
    if (n > 1) {
      n >>= 1;
    }
  }
  random_block_ready = 0;
}

static void setup() {
  // disable analog comparator
  ACSR |= 1 << ACD;
  // disable AIN0 and AIN1 digital input
  DIDR = (1 << AIN1D) | (1 << AIN0D);

  DDRA |= _BV(PA0) | _BV(PA1); // PA2 is RESET
  DDRB |= 0xff;
  DDRD |= 0x7b; // PD2 (INT0) is input, PD7 is unavailable
  PORTD |= _BV(PD2); // pull up the INT0 pin

  reset_row();
  reset_col();

  // enable Timer1 w/o prescaler
  TCCR1B = 1 << CS10;

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  sei();
}

int main() {
  setup();

  while (1) {
    // show the random block
    for (int counter = 0; counter < SHOW_CYCLES; ++counter) {
      show_random_block();
      // enable INT0 interrupt after a while
      if (counter == NEXT_ALLOW_CYCLE) {
        int0_fired = 0;
        GIMSK |= _BV(INT0);
      }
      if (int0_fired) {
        break;
      }
    }

    if (int0_fired) {
      int0_fired = 0;
      collect_entropy();
      continue; // show new block again
    }

    // enable INT0 interrupt
    GIMSK |= _BV(INT0);
    // enter power-down mode
    sleep_mode();

    // wake-up occurs here
    int0_fired = 0;
  }
}
