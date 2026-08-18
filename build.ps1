$ErrorActionPreference = "Stop"

# 1. PATH temporário
$env:Path = "C:\w64devkit\bin;" + $env:Path

# 2. Limpeza forçada de caches e compilações anteriores
Remove-Item kernel\*.o, *.bin, *.tmp, migOS.img -ErrorAction SilentlyContinue

# 3. Monta os arquivos Assembly
.\nasm.exe -f bin boot\boot.asm -o boot.bin
.\nasm.exe -f win32 kernel\kernel_entry.asm -o kernel\kernel_entry.o
.\nasm.exe -f win32 kernel\isr_asm.asm -o kernel\isr_asm.o

# 4. Compila os módulos C
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe -c kernel\vga.c -o kernel\vga.o
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe -c kernel\pic.c -o kernel\pic.o
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe -c kernel\idt.c -o kernel\idt.o
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe -c kernel\isr.c -o kernel\isr.o
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe -c kernel\keyboard.c -o kernel\keyboard.o
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe -c kernel\shell.c -o kernel\shell.o
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe -c kernel\kernel.c -o kernel\kernel.o

# 5. Linka todos os módulos
ld -m i386pe -T linker.ld kernel\kernel_entry.o kernel\isr_asm.o kernel\vga.o kernel\pic.o kernel\idt.o kernel\isr.o kernel\keyboard.o kernel\shell.o kernel\kernel.o -o kernel.tmp
objcopy -O binary kernel.tmp kernel.bin
Remove-Item kernel.tmp -ErrorAction SilentlyContinue

# 6. Padding do Kernel para 15 setores (7680 bytes)
$targetSize = 15 * 512
$currentSize = (Get-Item kernel.bin).Length
$padBytes = $targetSize - $currentSize

if ($padBytes -gt 0) {
    $kernelData = [IO.File]::ReadAllBytes("$PWD\kernel.bin")
    $paddedData = [byte[]]::new($targetSize)
    [Array]::Copy($kernelData, $paddedData, $kernelData.Length)
    [IO.File]::WriteAllBytes("$PWD\kernel_padded.bin", $paddedData)
} else {
    Copy-Item kernel.bin kernel_padded.bin
}

# 7. Gera a imagem final
Get-Content -Path boot.bin, kernel_padded.bin -Encoding Byte -ReadCount 0 | Set-Content -Path migOS.img -Encoding Byte

# 8. Executa no QEMU
& "C:\Program Files\qemu\qemu-system-x86_64.exe" -drive format=raw,file=migOS.img