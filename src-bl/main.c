#include <efi.h>

static EFI_GUID LoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID SimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID DevicePathProtocolGuid = { 0x09576e91, 0x6d3f, 0x11d2, { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };
static inline UINT64 rdtsc(void) {
    UINT32 low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d" (high));
    return ((UINT64)high << 32) | low;
}

void get_cpu_brand_string(CHAR16 *out_buf) {
    UINT32 registers[12];
    char *char_ptr = (char *)registers;


    for (UINT32 i = 0; i < 3; i++) {
        UINT32 op = 0x80000002 + i;
        __asm__ volatile (
            "cpuid"
            : "=a"(registers[i*4]), "=b"(registers[i*4+1]), "=c"(registers[i*4+2]), "=d"(registers[i*4+3])
            : "a"(op)
        );
    }

    for (int i = 0; i < 48 && char_ptr[i] != '\0'; i++) {
        out_buf[i] = (CHAR16)char_ptr[i];
        out_buf[i+1] = L'\0';
    }
}

void print_timestamp(EFI_SYSTEM_TABLE *SystemTable) {
    UINT64 cycles = rdtsc();
    
    UINT64 total_micros = cycles / 2500; 
    UINT64 seconds = total_micros / 1000000;
    UINT64 micros = total_micros % 1000000;

    CHAR16 buf[32];
    
    buf[0] = L'[';
    
    
    UINT64 temp_secs = seconds;
    for (int i = 4; i >= 1; i--) {
        if (temp_secs > 0 || i == 4) {
            buf[i] = L'0' + (temp_secs % 10);
            temp_secs /= 10;
        } else {
            buf[i] = L' ';
        }
    }
    
    buf[5] = L'.';
    
    for (int i = 11; i >= 6; i--) {
        buf[i] = L'0' + (micros % 10);
        micros /= 10;
    }
    buf[12] = L']';
    buf[13] = L' ';
    buf[14] = L'\0';

    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, buf);
}

void print_hex(EFI_SYSTEM_TABLE *SystemTable, UINT64 val) {
    CHAR16 buf[19];
    buf[0] = L'0';
    buf[1] = L'x';
    for (int i = 17; i >= 2; i--) {
        UINT8 nibble = val & 0xF;
        if (nibble < 10)
            buf[i] = L'0' + nibble;
        else
            buf[i] = L'A' + (nibble - 10);
        val >>= 4;
    }
    buf[18] = L'\0';
    SystemTable->ConOut->OutputString(SystemTable->ConOut, buf);
}

void print_str(EFI_SYSTEM_TABLE *SystemTable, CHAR16 *Str) {
    SystemTable->ConOut->OutputString(SystemTable->ConOut, Str);
}

void wait_for_key(EFI_SYSTEM_TABLE *SystemTable) {
    UINTN Index;
    EFI_INPUT_KEY Key;
    SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, &Index);
    SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key);
}

void draw_progress(EFI_SYSTEM_TABLE *SystemTable, UINTN current, UINTN total) {

    UINTN percentage = (current * 100) / total;

    UINTN progress_bars = percentage / 5; 

    CHAR16 bar[32];
    bar[0] = L'\r';
    bar[1] = L'[';

    int i = 2;
    for (; i < 2 + progress_bars; i++) {
        bar[i] = L'#';
    }
    for (; i < 22; i++) {
        bar[i] = L'-';
    }
    bar[22] = L']';
    bar[23] = L' ';
    

    bar[24] = L'0' + (percentage / 100);
    bar[25] = L'0' + ((percentage % 100) / 10);
    bar[26] = L'0' + (percentage % 10);
    bar[27] = L'%';
    bar[28] = L'\0';

    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x0A);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, bar);
}

void print_dec(EFI_SYSTEM_TABLE *SystemTable, UINT64 val) {
    CHAR16 buf[21];
    int i = 19;
    buf[20] = L'\0';
    if (val == 0) {
        buf[--i] = L'0';
    } else {
        while (val > 0) {
            buf[--i] = L'0' + (val % 10);
            val /= 10;
        }
    }
    SystemTable->ConOut->OutputString(SystemTable->ConOut, &buf[i]);
}

int w_strcmp(CHAR16 *s1, CHAR16 *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *s1 - *s2;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) { 
    CHAR16 *KernelArgs = L"root=/dev/nvme0n1p2 rw rootflags=subvol=@ initrd=\\initramfs.img console=tty0";
    EFI_LOADED_IMAGE_PROTOCOL *KernelLoadedImage;
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Filesystem;
    EFI_FILE_PROTOCOL *RootDirectory = NULL;
    EFI_HANDLE KernelImageHandle = NULL;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;
    CHAR16 cpu_name[49];

    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x02);

    print_str(SystemTable, L" _              _   _              _         \r\n");
    print_str(SystemTable, L" | |__  ___  ___| |_| |___  __ _ __| |___ _ _ \r\n");
    print_str(SystemTable, L" | '_ \\/ _ \\/ _ \\  _| / _ \\/ _` / _` / -_) '_|\r\n");
    print_str(SystemTable, L" |_.__/\\___/\\___/\\__|_\\___/\\__,_\\__,_\\___|_|  \r\n");
    
    print_timestamp(SystemTable);
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x0B);
    print_str(SystemTable, L"[ DIAG  ] ");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);
    print_str(SystemTable, L"CPU: ");
    get_cpu_brand_string(cpu_name);
    print_str(SystemTable, cpu_name);
    print_str(SystemTable, L"\r\n");

    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);

    print_timestamp(SystemTable);
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x0B);
    print_str(SystemTable, L"[ DIAG  ] ");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);
    print_str(SystemTable, L"Firmware Vendor: ");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x0E);
    print_str(SystemTable, SystemTable->FirmwareVendor);
    print_str(SystemTable, L"\r\n");
    print_timestamp(SystemTable);
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x0B);
    print_str(SystemTable, L"[ DIAG  ] ");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);
    print_str(SystemTable, L"Firmware Revision: v");
    print_dec(SystemTable, SystemTable->FirmwareRevision >> 16);
    print_str(SystemTable, L".");
    print_dec(SystemTable, SystemTable->FirmwareRevision & 0xFFFF);
    print_str(SystemTable, L" | UEFI Spec: ");
    print_dec(SystemTable, SystemTable->Hdr.Revision >> 16);
    print_str(SystemTable, L".");
    print_dec(SystemTable, (SystemTable->Hdr.Revision & 0xFFFF) / 10);
    print_str(SystemTable, L"\r\n");

    print_timestamp(SystemTable);
    print_str(SystemTable, L"Loading ");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x03);
    print_str(SystemTable, L"Arch Linux");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);
    print_str(SystemTable, L"...\r\n");

    print_timestamp(SystemTable);
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x05);
    print_str(SystemTable, L"[ INFO  ] ");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);
    print_str(SystemTable, L"DEBUG: ImageHandle=");
    print_hex(SystemTable, (UINT64)ImageHandle);
    print_str(SystemTable, L" | SystemTable=");
    print_hex(SystemTable, (UINT64)SystemTable);
    print_str(SystemTable, L"\r\n");
    Status = SystemTable->BootServices->OpenProtocol(
        ImageHandle, &LoadedImageProtocolGuid, (void**)&LoadedImage, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    if(EFI_ERROR(Status)) return Status;

    print_timestamp(SystemTable);
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x05);
    print_str(SystemTable, L"[ INFO  ] ");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);
    print_str(SystemTable, L"Loader ImageBase: ");
    print_hex(SystemTable, (UINT64)LoadedImage->ImageBase);
    print_str(SystemTable, L" | Size: ");
    print_dec(SystemTable, LoadedImage->ImageSize / 1024);
    print_str(SystemTable, L" KB\r\n");

    Status = SystemTable->BootServices->OpenProtocol(
        LoadedImage->DeviceHandle, &SimpleFileSystemProtocolGuid, (void**)&Filesystem, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    if(EFI_ERROR(Status)) return Status;

    Status = Filesystem->OpenVolume(Filesystem, &RootDirectory);
    if(EFI_ERROR(Status)) return Status;

    Status = SystemTable->BootServices->OpenProtocol(
        LoadedImage->DeviceHandle, &DevicePathProtocolGuid, (void**)&DevicePath, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    if(EFI_ERROR(Status)) {
        RootDirectory->Close(RootDirectory);
        return Status;
    }

    EFI_FILE_PROTOCOL *KernelFile = NULL;
    Status = RootDirectory->Open(RootDirectory, &KernelFile, L"\\vmlinuz", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        print_str(SystemTable, L"dayn?: Cannot open \\vmlinuz\r\n");
        wait_for_key(SystemTable);
        return Status;
    }

    EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
    UINTN InfoSize = 0;
    UINTN RealFileSize = 0;

    Status = KernelFile->GetInfo(KernelFile, &FileInfoGuid, &InfoSize, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        EFI_FILE_INFO *FileInfo = NULL;
        SystemTable->BootServices->AllocatePool(EfiLoaderData, InfoSize, (void**)&FileInfo);
        Status = KernelFile->GetInfo(KernelFile, &FileInfoGuid, &InfoSize, FileInfo);
        if (!EFI_ERROR(Status)) {

            RealFileSize = FileInfo->FileSize;
        }
        SystemTable->BootServices->FreePool(FileInfo);
    }

    if (RealFileSize == 0) {
        KernelFile->Close(KernelFile);
        RootDirectory->Close(RootDirectory);
        return EFI_LOAD_ERROR;
    }

 
    UINTN PagesCount = (RealFileSize + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS KernelBufferPages = 0;
    
    Status = SystemTable->BootServices->AllocatePages(
        AllocateAnyPages,
        EfiLoaderCode,
        PagesCount,
        &KernelBufferPages
    );
    
    if(EFI_ERROR(Status)) {
        KernelFile->Close(KernelFile);
        RootDirectory->Close(RootDirectory);
        return Status;
    }
    
    print_timestamp(SystemTable);
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x05);
    print_str(SystemTable, L"[ INFO  ] ");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);
    print_str(SystemTable, L"Reserved RAM: ");
    print_dec(SystemTable, PagesCount);
    print_str(SystemTable, L" pages for kernel staging at ");
    print_hex(SystemTable, KernelBufferPages);
    print_str(SystemTable, L"\r\n");

    void *KernelBuffer = (void*)KernelBufferPages;
    UINTN TotalRead = 0;
    void *CurrentBufferPtr = KernelBuffer;

    while (TotalRead < RealFileSize) {
        UINTN ChunkSize = RealFileSize - TotalRead;

        if (ChunkSize > 65536) ChunkSize = 65536; 

        Status = KernelFile->Read(KernelFile, &ChunkSize, CurrentBufferPtr);
        if (EFI_ERROR(Status) || ChunkSize == 0) {
            break;
        }
        TotalRead += ChunkSize;
        CurrentBufferPtr = (void*)((UINTN)CurrentBufferPtr + ChunkSize);


        draw_progress(SystemTable, TotalRead, RealFileSize);
    }
    print_str(SystemTable, L"\r\n");

    KernelFile->Close(KernelFile);
    RootDirectory->Close(RootDirectory);

    if (TotalRead != RealFileSize) {
        SystemTable->BootServices->FreePages(KernelBufferPages, PagesCount);
        print_str(SystemTable, L"Error: Failed to read full file!\r\n");
        wait_for_key(SystemTable);
        return EFI_LOAD_ERROR;
    }
    print_timestamp(SystemTable);
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x02);
    print_str(SystemTable, L"[  OK  ] ");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);
    print_str(SystemTable, L"vmlinuz loaded into page memory. Initializing Image...\r\n");

    Status = SystemTable->BootServices->LoadImage(
        FALSE,
        ImageHandle,
        DevicePath,
        KernelBuffer,
        RealFileSize, 
        &KernelImageHandle
    );

    SystemTable->BootServices->FreePages(KernelBufferPages, PagesCount);

    if(EFI_ERROR(Status)) {
        print_str(SystemTable, L"Error: LoadImage from memory failed!\r\n");
        wait_for_key(SystemTable);
        return Status;
    }

    print_timestamp(SystemTable);
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x02);
    print_str(SystemTable, L"[  OK  ] ");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);
    print_str(SystemTable, L"DEBUG: Kernel Image initialized. KernelHandle=");
    print_hex(SystemTable, (UINT64)KernelImageHandle);
    print_str(SystemTable, L"\r\n");

    Status = SystemTable->BootServices->OpenProtocol(
        KernelImageHandle, &LoadedImageProtocolGuid, (void**)&KernelLoadedImage, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );

    if(!EFI_ERROR(Status)) {
        KernelLoadedImage->LoadOptions = (void*)KernelArgs;
        UINTN len = 0; while(KernelArgs[len]) len++;
        KernelLoadedImage->LoadOptionsSize = (len + 1) * sizeof(CHAR16);
        print_timestamp(SystemTable);
        SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x02);
        print_str(SystemTable, L"[  OK  ] ");
        SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x07);
        print_str(SystemTable, L"DEBUG: Kernel LoadOptions injected successfully.\r\n");
    }
    
    print_timestamp(SystemTable);

    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x0E);
    print_str(SystemTable, L"[ LAUNCH ] ");
    SystemTable->ConOut->SetAttribute(SystemTable->ConOut, 0x0F);
    print_str(SystemTable, L"Jumping into the kernel memory addr...\r\n");

    Status = SystemTable->BootServices->StartImage(KernelImageHandle, NULL, NULL);

    print_str(SystemTable, L"Error: StartImage failed!\r\n");
    wait_for_key(SystemTable);
    return Status;
}
