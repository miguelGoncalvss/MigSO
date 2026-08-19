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
    "-std=gnu99",
    "-ffreestanding",
    "-fno-pie",
    "-fno-stack-protector",
    "-fno-asynchronous-unwind-tables",
    "-mno-stack-arg-probe",
    "-m80387",
    "-mfpmath=387",
    "-mno-sse",
    "-mno-sse2",
    "-O2",
    "-Wall",
    "-Wno-unused-parameter",
    "-Wno-unused-variable",
    "-Wno-unused-but-set-variable",
    "-Wno-dangling-pointer",
    "-Wno-missing-field-initializers",
    "-Wno-sign-compare",
    "-Wno-unused-function",
    "-Wno-implicit-function-declaration",
    "-Wno-int-conversion",
    "-DCMAP256",
    "-DDOOMGENERIC_RESX=320",
    "-DDOOMGENERIC_RESY=200",
    "-DNORMALUNIX",
    "-I$PSScriptRoot\include",
    "-I$PSScriptRoot\include\kernel",
    "-I$PSScriptRoot\include\arch\i386",
    "-I$PSScriptRoot\include\drivers",
    "-I$PSScriptRoot\include\fs",
    "-I$PSScriptRoot\include\libc",
    "-I$PSScriptRoot\include\shell",
    "-I$PSScriptRoot\include\gui",
    "-I$PSScriptRoot\include\games",
    "-I$PSScriptRoot\include\editor",
    "-I$PSScriptRoot\include\interpreter",
    "-I$PSScriptRoot\include\emulator"
)

$Core_Sources = @(
    @{ Src = "kernel\kernel.c";                 Obj = "$BuildDir\kernel.o" },
    @{ Src = "kernel\arch\i386\idt.c";          Obj = "$BuildDir\idt.o" },
    @{ Src = "kernel\arch\i386\isr.c";          Obj = "$BuildDir\isr.o" },
    @{ Src = "kernel\arch\i386\pic.c";          Obj = "$BuildDir\pic.o" },
    @{ Src = "kernel\arch\i386\timer.c";        Obj = "$BuildDir\timer.o" },
    @{ Src = "kernel\memory\pmm.c";             Obj = "$BuildDir\pmm.o" },
    @{ Src = "kernel\memory\kheap.c";           Obj = "$BuildDir\kheap.o" },
    @{ Src = "kernel\fs\migfs.c";               Obj = "$BuildDir\migfs.o" },
    @{ Src = "kernel\editor\editor.c";          Obj = "$BuildDir\editor.o" },
    @{ Src = "kernel\interpreter\txt_interp.c"; Obj = "$BuildDir\txt_interp.o" },
    @{ Src = "kernel\emulator\gameboy.c";       Obj = "$BuildDir\gameboy.o" },
    @{ Src = "kernel\drivers\bga\bga.c";        Obj = "$BuildDir\bga.o" },
    @{ Src = "kernel\drivers\vga\vga.c";        Obj = "$BuildDir\vga.o" },
    @{ Src = "kernel\drivers\vga\vga_mode13.c"; Obj = "$BuildDir\vga_mode13.o" },
    @{ Src = "kernel\drivers\disk\ata.c";       Obj = "$BuildDir\ata.o" },
    @{ Src = "kernel\drivers\keyboard\keyboard.c"; Obj = "$BuildDir\keyboard.o" },
    @{ Src = "kernel\drivers\mouse\mouse.c";       Obj = "$BuildDir\mouse.o" },
    @{ Src = "kernel\gui\gui.c";                   Obj = "$BuildDir\gui.o" },
    @{ Src = "kernel\games\snake.c";               Obj = "$BuildDir\snake.o" },
    @{ Src = "libc\string.c";                   Obj = "$BuildDir\string.o" },
    @{ Src = "libc\stdlib.c";                   Obj = "$BuildDir\stdlib.o" },
    @{ Src = "libc\stdio.c";                    Obj = "$BuildDir\stdio.o" },
    @{ Src = "shell\shell.c";                   Obj = "$BuildDir\shell.o" }
)

foreach ($item in $Core_Sources) {
    & gcc @GccFlags -c $item.Src -o $item.Obj
    if ($LASTEXITCODE -ne 0) { throw "Erro ao compilar $($item.Src)" }
}

# ============================================================
# 5. Linkagem do Kernel
# ============================================================
Write-Host "[4/7] Linkando modulos do Kernel e GUI..." -ForegroundColor Yellow

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
    "$BuildDir\editor.o",
    "$BuildDir\txt_interp.o",
    "$BuildDir\gameboy.o",
    "$BuildDir\bga.o",
    "$BuildDir\vga.o",
    "$BuildDir\vga_mode13.o",
    "$BuildDir\ata.o",
    "$BuildDir\keyboard.o",
    "$BuildDir\mouse.o",
    "$BuildDir\gui.o",
    "$BuildDir\snake.o",
    "$BuildDir\string.o",
    "$BuildDir\stdlib.o",
    "$BuildDir\stdio.o",
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

# O bootloader suporta carregar ate 1024 setores (1024 * 512 = 524288 bytes = 512KB)
$SectorSize = 512
$TargetSectors = 1024
$TargetKernelSize = $TargetSectors * $SectorSize
$DiskTotalSectors = 32768 # 16 MB
$DiskTotalSize = $DiskTotalSectors * $SectorSize

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

# Cria a imagem de disco final de 16MB (32768 setores)
$FinalImageData = [byte[]]::new($DiskTotalSize)
[System.Array]::Copy($BootData, 0, $FinalImageData, 0, $BootData.Length)
$KernelOffset = $BootData.Length
[System.Array]::Copy($PaddedKernelData, 0, $FinalImageData, $KernelOffset, $PaddedKernelData.Length)

# ============================================================
# Formatacao e Injecao de Arquivos no MIGFS (LBA 1025+)
# ============================================================
$MIGFS_SUPER_LBA = 1025
$MIGFS_FILE_TABLE_LBA = 1026
$MIGFS_DATA_START_LBA = 1035
$MIGFS_MAGIC = 0x4D494746

$FilesToPack = @()

$DefaultReadme = @"
====================================================
           BEM-VINDO AO migOS (v0.5)               
====================================================

migOS eh um Sistema Operacional x86 (IA-32) desenvolvido
do zero por Miguel para a disciplina de Sistemas Operacionais.

Principais componentes ativos:
 [OK] Bootloader MBR (16-bit) -> Protected Mode (32-bit)
 [OK] GDT (4GB Flat) e IDT (256 Vetores de Interrupcao)
 [OK] PIC 8259A & PIT 8254 Timer (100 Hz)
 [OK] Driver de Teclado PS/2 (Shift, Caps, Setas, Modificadores)
 [OK] Driver de Video VGA 80x25 / BGA 640x480 True Color
 [OK] PMM (Physical Memory Manager - Frames 4KB Bitmap)
 [OK] KHeap (Alocador de Heap kmalloc / kfree)
 [OK] MIGFS Persistente com Gravacao em Disco ATA/IDE
 [OK] Editor de Texto Visual (CLI: 'edit'/'nano' & GUI: 'TextEdit')
 [OK] Interpretador e Executor de Scripts .txt ('run'/'exec')
 [OK] Emulador de Game Boy Nativo Bare-Metal ('pokemon'/'gameboy')

Comandos disponiveis no shell:
 - pokemon / gameboy    : Inicia o emulador com Pokemon Red
 - ls                   : Lista os arquivos do disco
 - cat <arquivo>        : Exibe o conteudo de um arquivo
 - edit / nano <arq>    : Abre o Editor de Texto no Terminal
 - run / exec <arq.txt> : Executa script ou interpretador .txt
 - calc <expressao>     : Avalia expressoes matematicas
 - gui / desktop        : Inicia a Interface Grafica System 7
"@

$DefaultDemo = @"
# Script Demonstrativo do migOS
echo ====================================
echo   Iniciando Script de Teste migOS   
echo ====================================
set A=50
set B=25
calc `$A + `$B * 2
echo Calculo realizado com sucesso!
ls
"@

$DefaultCalc = @"
# Script de Calculo
set RAIO=10
set PI=3
calc `$PI * `$RAIO * `$RAIO
echo Area calculada!
"@

$FilesToPack += @{ Name = "readme.txt"; Bytes = [System.Text.Encoding]::ASCII.GetBytes($DefaultReadme); Flags = 1 }
$FilesToPack += @{ Name = "demo.txt";   Bytes = [System.Text.Encoding]::ASCII.GetBytes($DefaultDemo);   Flags = 0 }
$FilesToPack += @{ Name = "calc.txt";   Bytes = [System.Text.Encoding]::ASCII.GetBytes($DefaultCalc);   Flags = 0 }

# Se existir PokemonRed.gb na raiz, inclui no disco persistente!
$RomPath = "$PSScriptRoot\PokemonRed.gb"
if (-not (Test-Path $RomPath)) {
    $RomPath = "$PSScriptRoot\pokemon.gb"
}

if (Test-Path $RomPath) {
    $RomBytes = [System.IO.File]::ReadAllBytes($RomPath)
    Write-Host "      [Game Boy] Incluindo ROM '$([System.IO.Path]::GetFileName($RomPath))' ($($RomBytes.Length) bytes) no disco ATA..." -ForegroundColor Magenta
    $FilesToPack += @{ Name = "pokemon.gb";    Bytes = $RomBytes; Flags = 0 }
    $FilesToPack += @{ Name = "PokemonRed.gb"; Bytes = $RomBytes; Flags = 0 }
}

# Se ja existia um save pokemon.sav no migOS.img anterior, preserva-o!
$PrevImgPath = "$PSScriptRoot\migOS.img"
if ((Test-Path $PrevImgPath) -and (-not $Clean)) {
    try {
        $PrevData = [System.IO.File]::ReadAllBytes($PrevImgPath)
        if ($PrevData.Length -ge ($MIGFS_SUPER_LBA * 512 + 4)) {
            $PrevMagic = [System.BitConverter]::ToUInt32($PrevData, $MIGFS_SUPER_LBA * 512)
            if ($PrevMagic -eq $MIGFS_MAGIC) {
                $TableOffset = $MIGFS_FILE_TABLE_LBA * 512
                for ($ei = 0; $ei -lt 64; $ei++) {
                    $EntryOff = $TableOffset + ($ei * 64)
                    $InUse = [System.BitConverter]::ToUInt32($PrevData, $EntryOff + 48)
                    if ($InUse -eq 1) {
                        $NameBytes = [byte[]]::new(32)
                        [System.Array]::Copy($PrevData, $EntryOff, $NameBytes, 0, 32)
                        $NullPos = [System.Array]::IndexOf($NameBytes, [byte]0)
                        if ($NullPos -lt 0) { $NullPos = 32 }
                        $FName = [System.Text.Encoding]::ASCII.GetString($NameBytes, 0, $NullPos).Trim()
                        $FSize = [System.BitConverter]::ToUInt32($PrevData, $EntryOff + 32)
                        $FStartSec = [System.BitConverter]::ToUInt32($PrevData, $EntryOff + 40)
                        
                        if ($FName.EndsWith(".sav") -and $FSize -gt 0 -and $FStartSec -gt 0) {
                            $SavDataOffset = $FStartSec * 512
                            if ($SavDataOffset + $FSize -le $PrevData.Length) {
                                $SavBytes = [byte[]]::new($FSize)
                                [System.Array]::Copy($PrevData, $SavDataOffset, $SavBytes, 0, $FSize)
                                Write-Host "      [Save Game] Preservando save game persistente '$FName' ($FSize bytes)!" -ForegroundColor Green
                                $FilesToPack = $FilesToPack | Where-Object { $_.Name -ne $FName }
                                $FilesToPack += @{ Name = $FName; Bytes = $SavBytes; Flags = 0 }
                            }
                        }
                    }
                }
            }
        }
    } catch {}
}

# Grava os arquivos no MIGFS dentro do $FinalImageData
$CurLba = $MIGFS_DATA_START_LBA
$TableBytes = [byte[]]::new(8 * 512)
$PackedCount = 0

foreach ($f in $FilesToPack) {
    if ($PackedCount -ge 64) { break }
    
    $fName = $f.Name
    $fBytes = $f.Bytes
    $fSize = $fBytes.Length
    $fFlags = $f.Flags
    $fSectors = [Math]::Max(1, [int][Math]::Ceiling($fSize / 512.0))
    
    $EntryOff = $PackedCount * 64
    $NameEnc = [System.Text.Encoding]::ASCII.GetBytes($fName)
    $CopyNameLen = [Math]::Min(31, $NameEnc.Length)
    [System.Array]::Copy($NameEnc, 0, $TableBytes, $EntryOff, $CopyNameLen)
    $TableBytes[$EntryOff + $CopyNameLen] = 0
    
    [System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$fSize), 0, $TableBytes, $EntryOff + 32, 4)
    [System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$fFlags), 0, $TableBytes, $EntryOff + 36, 4)
    [System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$CurLba), 0, $TableBytes, $EntryOff + 40, 4)
    [System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$fSectors), 0, $TableBytes, $EntryOff + 44, 4)
    [System.Array]::Copy([System.BitConverter]::GetBytes([uint32]1), 0, $TableBytes, $EntryOff + 48, 4)
    
    $DataOffset = $CurLba * 512
    if ($DataOffset + $fSize -le $DiskTotalSize) {
        [System.Array]::Copy($fBytes, 0, $FinalImageData, $DataOffset, $fSize)
    }
    
    $CurLba += $fSectors
    $PackedCount++
}

# Grava Tabela de Arquivos no $FinalImageData
$TableDataOffset = $MIGFS_FILE_TABLE_LBA * 512
[System.Array]::Copy($TableBytes, 0, $FinalImageData, $TableDataOffset, $TableBytes.Length)

# Grava Superbloco no $FinalImageData
$SuperBlockBytes = [byte[]]::new(512)
[System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$MIGFS_MAGIC), 0, $SuperBlockBytes, 0, 4)
[System.Array]::Copy([System.BitConverter]::GetBytes([uint32]1), 0, $SuperBlockBytes, 4, 4)
[System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$PackedCount), 0, $SuperBlockBytes, 8, 4)
[System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$CurLba), 0, $SuperBlockBytes, 12, 4)
[System.Array]::Copy([System.BitConverter]::GetBytes([uint32]32000), 0, $SuperBlockBytes, 16, 4)
$LabelBytes = [System.Text.Encoding]::ASCII.GetBytes("migOS_PERSISTENT_HD")
[System.Array]::Copy($LabelBytes, 0, $SuperBlockBytes, 24, [Math]::Min(31, $LabelBytes.Length))

$SuperDataOffset = $MIGFS_SUPER_LBA * 512
[System.Array]::Copy($SuperBlockBytes, 0, $FinalImageData, $SuperDataOffset, $SuperBlockBytes.Length)

# Escreve tanto na pasta build quanto na raiz para compatibilidade
[System.IO.File]::WriteAllBytes("$BuildDir\migOS.img", $FinalImageData)
[System.IO.File]::WriteAllBytes("$PSScriptRoot\migOS.img", $FinalImageData)

Write-Host "[6/7] migOS.img gerada com sucesso! ($DiskTotalSize bytes / $DiskTotalSectors setores)" -ForegroundColor Green

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
    & $QemuExec -m 64M -vga std -drive format=raw,file="$PSScriptRoot\migOS.img" -rtc base=localtime
} else {
    Write-Warning "[AVISO] Executavel do QEMU nao encontrado nos caminhos padrao. Imagem 'migOS.img' gerada e pronta para execucao."
}