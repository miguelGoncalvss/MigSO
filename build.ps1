<#
.SYNOPSIS
    Script de build, linkagem e inicializacao do migOS.
.DESCRIPTION
    Compila o bootloader (NASM 16-bit), o kernel (x86 32-bit Assembly/C),
    drivers, libc e shell em uma imagem de disco inicializavel migOS.img.
.PARAMETER NoRun
    Se definido, compila e gera a imagem sem iniciar o QEMU.
.PARAMETER Clean
    Se definido, apenas limpa os artefatos de compilacao anteriores.
#>

param(
    [switch]$NoRun,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# ============================================================
# 1. Configuracao do Ambiente & PATH
# ============================================================
$DevKitPath = "C:\w64devkit\bin"
if ((Test-Path $DevKitPath) -and ($env:Path -notlike "*$DevKitPath*")) {
    $env:Path = "$DevKitPath;" + $env:Path
}

# Localiza o montador NASM
$NasmExec = $null
if (Test-Path ".\nasm.exe") {
    $NasmExec = ".\nasm.exe"
} elseif (Get-Command nasm -ErrorAction SilentlyContinue) {
    $NasmExec = "nasm"
} elseif (Test-Path "boot\nasm.exe") {
    $NasmExec = "boot\nasm.exe"
} else {
    Write-Error "[ERRO] Executavel do NASM (nasm.exe) nao encontrado!"
    exit 1
}

# ============================================================
# 2. Diretorios & Limpeza
# ============================================================
$BuildDir = "$PSScriptRoot\build"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   migOS Build System - v0.5            " -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

# Fecha instancias anteriores do QEMU para liberar o arquivo de imagem
try {
    Get-Process -Name "qemu-system-x86_64", "qemu-system-i386" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 150
} catch {}

Write-Host "[1/7] Limpando artefatos anteriores..." -ForegroundColor Yellow
if (Test-Path $BuildDir) {
    Remove-Item -Path "$BuildDir\*" -Recurse -Force -ErrorAction SilentlyContinue
} else {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Limpeza de binarios residuais na raiz
Remove-Item "$PSScriptRoot\*.bin", "$PSScriptRoot\*.o", "$PSScriptRoot\*.tmp", "$PSScriptRoot\migOS.img" -Force -ErrorAction SilentlyContinue

if ($Clean) {
    Write-Host "[OK] Limpeza concluida com sucesso." -ForegroundColor Green
    exit 0
}

# ============================================================
# 3. Montagem dos Arquivos Assembly
# ============================================================
Write-Host "[2/7] Montando Bootloader e Assembly..." -ForegroundColor Yellow

# Bootloader (16-bit MBR)
& $NasmExec -f bin "boot\boot.asm" -o "$BuildDir\boot.bin"
if ($LASTEXITCODE -ne 0) { throw "Falha na montagem de boot.asm" }

# Entry Point do Kernel (32-bit Protected Mode)
& $NasmExec -f win32 "kernel\arch\i386\kernel_entry.asm" -o "$BuildDir\kernel_entry.o"
if ($LASTEXITCODE -ne 0) { throw "Falha na montagem de kernel_entry.asm" }

# ISR Assembly Stubs (Excecoes de CPU)
& $NasmExec -f win32 "kernel\arch\i386\isr_asm.asm" -o "$BuildDir\isr_asm.o"
if ($LASTEXITCODE -ne 0) { throw "Falha na montagem de isr_asm.asm" }

# ============================================================
# 4. Compilacao dos Modulos C
# ============================================================
Write-Host "[3/7] Compilando modulos C do Kernel, Drivers, LibC e Shell..." -ForegroundColor Yellow

$GccFlags = @(
    "-m32",
    "-ffreestanding",
    "-fno-pie",
    "-fno-stack-protector",
    "-fno-asynchronous-unwind-tables",
    "-mno-stack-arg-probe",
    "-mgeneral-regs-only",
    "-mno-sse",
    "-mno-sse2",
    "-mno-mmx",
    "-mno-80387",
    "-Wall",
    "-Wextra",
    "-O2",
    "-I$PSScriptRoot\include",
    "-I$PSScriptRoot\include\kernel",
    "-I$PSScriptRoot\include\arch\i386",
    "-I$PSScriptRoot\include\drivers",
    "-I$PSScriptRoot\include\fs",
    "-I$PSScriptRoot\include\libc",
    "-I$PSScriptRoot\include\shell"
)

$C_Sources = @(
    @{ Src = "kernel\kernel.c";                 Obj = "$BuildDir\kernel.o" },
    @{ Src = "kernel\arch\i386\idt.c";          Obj = "$BuildDir\idt.o" },
    @{ Src = "kernel\arch\i386\isr.c";          Obj = "$BuildDir\isr.o" },
    @{ Src = "kernel\arch\i386\pic.c";          Obj = "$BuildDir\pic.o" },
    @{ Src = "kernel\arch\i386\timer.c";        Obj = "$BuildDir\timer.o" },
    @{ Src = "kernel\memory\pmm.c";             Obj = "$BuildDir\pmm.o" },
    @{ Src = "kernel\memory\kheap.c";           Obj = "$BuildDir\kheap.o" },
    @{ Src = "kernel\fs\migfs.c";               Obj = "$BuildDir\migfs.o" },
    @{ Src = "kernel\drivers\vga\vga.c";        Obj = "$BuildDir\vga.o" },
    @{ Src = "kernel\drivers\keyboard\keyboard.c"; Obj = "$BuildDir\keyboard.o" },
    @{ Src = "libc\string.c";                   Obj = "$BuildDir\string.o" },
    @{ Src = "libc\stdlib.c";                   Obj = "$BuildDir\stdlib.o" },
    @{ Src = "shell\shell.c";                   Obj = "$BuildDir\shell.o" }
)

foreach ($item in $C_Sources) {
    & gcc @GccFlags -c $item.Src -o $item.Obj
    if ($LASTEXITCODE -ne 0) { throw "Erro ao compilar $($item.Src)" }
}

# ============================================================
# 5. Linkagem do Kernel
# ============================================================
Write-Host "[4/7] Linkando modulos do Kernel..." -ForegroundColor Yellow

# ATENCAO: kernel_entry.o DEVE ser o primeiro objeto para posicionar _start em 0x10000
$LinkObjects = @(
    "$BuildDir\kernel_entry.o",
    "$BuildDir\isr_asm.o",
    "$BuildDir\kernel.o",
    "$BuildDir\idt.o",
    "$BuildDir\isr.o",
    "$BuildDir\pic.o",
    "$BuildDir\timer.o",
    "$BuildDir\pmm.o",
    "$BuildDir\kheap.o",
    "$BuildDir\migfs.o",
    "$BuildDir\vga.o",
    "$BuildDir\keyboard.o",
    "$BuildDir\string.o",
    "$BuildDir\stdlib.o",
    "$BuildDir\shell.o"
)

& ld -m i386pe --image-base 0x0 -T "linker.ld" @LinkObjects -o "$BuildDir\kernel.tmp"
if ($LASTEXITCODE -ne 0) { throw "Erro durante o processo de linkagem (ld)" }

& objcopy -O binary "$BuildDir\kernel.tmp" "$BuildDir\kernel.bin"
if ($LASTEXITCODE -ne 0) { throw "Erro ao extrair imagem binaria do Kernel (objcopy)" }

# ============================================================
# 6. Padding do Kernel & Criacao da Imagem Final
# ============================================================
Write-Host "[5/7] Gerando imagem de disco (migOS.img)..." -ForegroundColor Yellow

# O bootloader carrega 64 setores (64 * 512 = 32768 bytes = 32KB)
$SectorSize = 512
$TargetSectors = 64
$TargetKernelSize = $TargetSectors * $SectorSize


$RawKernelData = [System.IO.File]::ReadAllBytes("$BuildDir\kernel.bin")
$CurrentKernelSize = $RawKernelData.Length

Write-Host "      Tamanho do Kernel compilado: $CurrentKernelSize bytes / Limite: $TargetKernelSize bytes" -ForegroundColor Gray

if ($CurrentKernelSize -gt $TargetKernelSize) {
    Write-Error "[ERRO FATAL] O Kernel ($CurrentKernelSize bytes) ultrapassou o limite de $TargetKernelSize bytes ($TargetSectors setores)!"
    exit 1
}

$PaddedKernelData = [byte[]]::new($TargetKernelSize)
[System.Array]::Copy($RawKernelData, $PaddedKernelData, $CurrentKernelSize)
[System.IO.File]::WriteAllBytes("$BuildDir\kernel_padded.bin", $PaddedKernelData)

# Le o bootloader (512 bytes)
$BootData = [System.IO.File]::ReadAllBytes("$BuildDir\boot.bin")

# Monta a imagem final (Bootloader + Kernel Padded)
$FinalImageSize = $BootData.Length + $PaddedKernelData.Length
$FinalImageData = [byte[]]::new($FinalImageSize)
[System.Array]::Copy($BootData, 0, $FinalImageData, 0, $BootData.Length)
[System.Array]::Copy($PaddedKernelData, 0, $FinalImageData, $BootData.Length, $PaddedKernelData.Length)

# Escreve tanto na pasta build quanto na raiz para compatibilidade
[System.IO.File]::WriteAllBytes("$BuildDir\migOS.img", $FinalImageData)
[System.IO.File]::WriteAllBytes("$PSScriptRoot\migOS.img", $FinalImageData)

Write-Host "[6/7] migOS.img gerada com sucesso! ($FinalImageSize bytes)" -ForegroundColor Green

# ============================================================
# 7. Execucao no QEMU
# ============================================================
if ($NoRun) {
    Write-Host "[7/7] Modo -NoRun ativado. Compilacao concluida sem iniciar QEMU." -ForegroundColor Cyan
    exit 0
}

Write-Host "[7/7] Inicializando migOS no QEMU..." -ForegroundColor Green

$QemuCandidates = @(
    "C:\Program Files\qemu\qemu-system-x86_64.exe",
    "C:\Program Files\qemu\qemu-system-i386.exe"
)

$QemuExec = $null
foreach ($q in $QemuCandidates) {
    if (Test-Path $q) {
        $QemuExec = $q
        break
    }
}

if (-not $QemuExec) {
    if (Get-Command "qemu-system-x86_64" -ErrorAction SilentlyContinue) {
        $QemuExec = "qemu-system-x86_64"
    } elseif (Get-Command "qemu-system-i386" -ErrorAction SilentlyContinue) {
        $QemuExec = "qemu-system-i386"
    }
}

if ($QemuExec) {
    & $QemuExec -drive format=raw,file="$PSScriptRoot\migOS.img"
} else {
    Write-Warning "[AVISO] Executavel do QEMU nao encontrado nos caminhos padrao. Imagem 'migOS.img' gerada e pronta para execucao."
}