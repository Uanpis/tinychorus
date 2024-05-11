CC		=	avr-gcc
OBJCOPY	=	avr-objcopy
CFLAGS	=	-funsigned-char \
			-funsigned-bitfields \
			-ffunction-sections \
			-fdata-sections \
			-fpack-struct \
			-fshort-enums \
			-g2 \
			-std=gnu99 \
			-O1 \
			-Wall \
			-mmcu=attiny404 \

PORT	=	/dev/ttyUSB0

main.out: main.c
	${CC} ${CFLAGS} -o main.out main.c

upload: main.out
	avrdude -p t404 -c serialupdi -b 225000 -P ${PORT} -U flash:w:main.out:e

clean:
	@rm -f *.out
	
