CC            = x86_64-w64-mingw32-gcc
AS            = x86_64-w64-mingw32-as


EFI_INC       = /usr/include/efi
EFI_LIB       = /usr/lib

CFLAGS        = -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I$(EFI_INC)/protocol \
                -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
                -Wall -DEFI_FUNCTION_WRAPPER


LDFLAGS       = -nostdlib -Wl,--subsystem,10 -e efi_main


LIBS          = $(EFI_LIB)/libefi.a $(EFI_LIB)/libgnuefi.a

all: main.efi

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

main.efi: main.o
	$(CC) $(LDFLAGS) -o main.efi main.o $(LIBS)

clean:
	rm -f *.o *.efi
