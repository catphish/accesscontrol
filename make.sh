set -e
rm -f *.o
PATH=/home/charlie/Applications/gcc-arm-none-eabi-9-2019-q4-major/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/home/charlie/.rvm/bin:/home/charlie/.rvm/bin
REPOROOT="/home/charlie/STM32Cube/Repository/STM32Cube_FW_F4_V1.25.0"
CCOPTS="-ggdb3 -Wall -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -I$REPOROOT/Drivers/CMSIS/Device/ST/STM32F4xx/Include -I$REPOROOT/Drivers/CMSIS/Include -DSTM32F4xx -O0 -ffast-math -specs=nano.specs -specs=nosys.specs"

arm-none-eabi-gcc $CCOPTS -c startup_stm32f407xx.s -o startup_stm32f407xx.o

arm-none-eabi-gcc $CCOPTS -c system.c -o system.o
arm-none-eabi-gcc $CCOPTS -c aes.c -o aes.o
arm-none-eabi-gcc $CCOPTS -c sha256.c -o sha256.o
arm-none-eabi-gcc $CCOPTS -c main.c -o main.o
arm-none-eabi-gcc $CCOPTS -c util.c -o util.o
arm-none-eabi-gcc $CCOPTS -c gpio.c -o gpio.o
arm-none-eabi-gcc $CCOPTS -c cloud.c -o cloud.o
arm-none-eabi-gcc $CCOPTS -c door.c -o door.o
arm-none-eabi-gcc $CCOPTS -c rfid.c -o rfid.o
arm-none-eabi-gcc $CCOPTS -c ethernet.c -o ethernet.o

arm-none-eabi-gcc $CCOPTS -T STM32F407VGTx_FLASH.ld -Wl,--gc-sections *.o -o main.elf -lm

arm-none-eabi-objcopy -O binary main.elf main.bin
rm *.o
st-flash write main.bin 0x8000000
