all: trng.hex

trng.elf: trng.c
	avr-gcc -Os -S -mmcu=attiny2313 $<
	avr-gcc -Os -mmcu=attiny2313 $< -o $@

trng.hex: trng.elf
	avr-size --format=avr --mcu=attiny2313 $<
	avr-objcopy -j .text -j .data -O ihex $< $@

# https://github.com/amitesh-singh/FASTUSBasp
flash: trng.hex
	avrdude -c usbasp-clone -p attiny2313 -B 375kHz -U flash:w:$<

clean:
	rm -f trng.elf trng.hex
