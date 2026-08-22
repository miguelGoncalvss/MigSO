# migOS - Sistema Operacional Educacional Bare-Metal x86 (IA-32)

Sistema operacional monolitico de 32 bits desenvolvido do zero para a arquitetura x86 (IA-32 / Protected Mode), projetado para fins academicos, explorando os fundamentos de software de sistema, drivers de hardware, gerenciamento de memoria fisica e virtual, sistema de arquivos em RAM, interface grafica retro no estilo Mac OS System 7 em alta resolucao 640x480 (32-bit True Color) e execucao de aplicacoes bare-metal.

---

## Indice

- [Visao Geral](#visao-geral)
- [Mapa Mental da Arquitetura](#mapa-mental-da-arquitetura)
- [Principais Funcionalidades](#principais-funcionalidades)
- [Estrutura do Projeto](#estrutura-do-projeto)
- [Detalhamento Arquivo por Arquivo](#detalhamento-arquivo-por-arquivo)
  - [1. Arquivos da Raiz e Automacao](#1-arquivos-da-raiz-e-automacao)
  - [2. Bootloader](#2-bootloader)
  - [3. Arquitetura i386 e Hardware](#3-arquitetura-i386-e-hardware)
  - [4. Nucleo do Kernel](#4-nucleo-do-kernel)
  - [5. Gerenciamento de Memoria](#5-gerenciamento-de-memoria)
  - [6. Drivers de Dispositivos](#6-drivers-de-dispositivos)
  - [7. Sistema de Arquivos (MIGFS / RAMDisk & Disco ATA)](#7-sistema-de-arquivos-migfs--ramdisk--disco-ata)
  - [8. Biblioteca C Minimalista (Freestanding LibC)](#8-biblioteca-c-minimalista-freestanding-libc)
  - [9. Shell Interativo](#9-shell-interativo)
  - [10. Interface Grafica (GUI Mac OS System 7 640x480)](#10-interface-grafica-gui-mac-os-system-7-640x480)
  - [11. Jogos e Aplicacoes](#11-jogos-e-aplicacoes)
  - [12. Editor de Texto Visual (TextEdit & Nano/Edit)](#12-editor-de-texto-visual-textedit--nanoedit)
  - [13. Interpretador e Executor de Scripts .txt](#13-interpretador-e-executor-de-scripts-txt)
  - [14. Emulador de Game Boy & Jogo Pokémon (Peanut-GB Bare-Metal)](#14-emulador-de-game-boy--jogo-pokémon-peanut-gb-bare-metal)
- [Emulador de Game Boy & Jogo Pokémon no migOS](#emulador-de-game-boy--jogo-pokémon-no-migos)
- [Driver de Relógio em Tempo Real (CMOS / RTC) & Timestamps no MIGFS](#driver-de-relógio-em-tempo-real-cmos--rtc--timestamps-no-migfs)
- [Driver de Áudio para PC Speaker (PIT 8254) & Efeitos Sonoros (SFX)](#driver-de-áudio-para-pc-speaker-pit-8254--efeitos-sonoros-sfx)
- [Sistema de Arquivos Hierárquico MIGFS & Persistência em Disco](#sistema-de-arquivos-hierárquico-migfs--persistência-em-disco)
- [Interpretador de Scripts & Calculadora Matemática](#interpretador-de-scripts--calculadora-matemática)
- [Terminal Avançado, Scrollbar & Ergonomia Linux](#terminal-avancado-scrollbar--ergonomia-linux)
- [Ciclo de Inicializacao (Boot Flow)](#ciclo-de-inicializacao-boot-flow)
- [Mapa de Memoria Fisica](#mapa-de-memoria-fisica)
- [Arquitetura do Heap Dinamico (KHeap)](#arquitetura-do-heap-dinamico-kheap)
- [Pipeline Grafico e Double Buffering](#pipeline-grafico-e-double-buffering)
- [Fluxo de Interrupcoes e Drivers de Entrada](#fluxo-de-interrupcoes-e-drivers-de-entrada)
- [Comandos do Terminal](#comandos-do-terminal)
- [Requisitos e Compilacao](#requisitos-e-compilacao)
- [Autor](#autor)

---

## Visao Geral

O migOS e um kernel bare-metal de 32 bits escrito em C (padrao GNU99 freestanding) e Assembly x86 (NASM). O projeto implementa desde o setor de inicializacao MBR de 512 bytes ate um terminal interativo com historico e um ambiente grafico retro inspirado no classico **Mac OS System 7 (1991)** padronizado na resolucao de **640x480 pixels a 32-bit True Color (ARGB/XRGB)**, passando por gerenciamento de memoria fisica baseado em mapa de bits (PMM Frame Bitmap 4KB), alocador dinamico de memoria no Heap (KHeap com coalescencia e divisao de blocos), sistema de arquivos hierárquico com suporte a pastas, carimbos de data/hora reais (timestamps) e persistência real em disco ATA/IDE (MIGFS), controle de interrupcoes por hardware (IDT e PIC 8259A), temporizacao precisa por hardware (PIT 8254), driver de relógio em tempo real da placa-mãe (CMOS / RTC com decodificação BCD), driver completo de áudio para PC Speaker via Canal 2 do PIT e Porta 0x61 com efeitos sonoros clássicos (Mac OS Startup Chime, Sosumi, Clicks, Trash, Success), driver de video Bochs/QEMU BGA com deteccao dinamica de barramento PCI e Linear Framebuffer (LFB), suporte a teclado e mouse PS/2 com rastreamento estável de coordenadas, biblioteca C freestanding, editor de texto visual (TextEdit / Edit), interpretador e executor de scripts `.txt` com calculadora aritmética e **Emulador completo de Nintendo Game Boy (Peanut-GB) executando Pokémon Red / Blue / Yellow** diretamente em Ring 0 bare-metal com salvamento e persistência de save games (`.sav`) em disco.

O kernel nao utiliza qualquer biblioteca padrao do sistema hospedeiro (`-ffreestanding -fno-builtin`), operando diretamente sobre o hardware x86 atraves de instrucoes de maquina e portas de entrada/saida (I/O Ports).

---

## Principais Funcionalidades

- Bootloader MBR proprio de 512 bytes com leitura LBA por pacotes DAP em blocos de 64 setores (INT 0x13 AH=0x42), ativacao da Linha A20 e transicao para Modo Protegido de 32 bits.
- Segmentacao plana (Flat Memory Model) de 4 GB para codigo e dados em Ring 0 via Global Descriptor Table (GDT).
- Tabela de Descritores de Interrupcao (IDT) com 256 vetores e tratadores para as 32 excecoes nativas da CPU x86 (ISRs 0 a 31).
- Kernel Panic com tela de diagnostico, dump completo dos registradores de execucao (EIP, CS, EFLAGS, EAX, EBX, ECX, EDX, ESP, EBP, ESI, EDI), contagem regressiva em tempo real sincronizada via hardware e rotina de reinicializacao automatica (Triple Fault forcado / Fast Reset).
- Remapeamento dos controladores de interrupcao PIC 8259A Master/Slave (IRQs 0-15 redirecionados para vetores 32-47).
- Temporizacao periodica em 100 Hz gerada pelo Programmable Interval Timer (PIT 8254), fornecendo medicoes de tempo de atividade (uptime em segundos e milissegundos) e rotinas de espera (`sleep` e `timer_wait`) sincronizadas com a instrucao `hlt`.
- **Driver de Áudio para PC Speaker & Efeitos Sonoros (SFX)**:
  - Síntese de áudio por onda quadrada utilizando o Canal 2 do PIT 8254 (porta de comando `0x43` com byte `0xB6` e porta de dados `0x42`) operando na frequência base de 1.193.180 Hz.
  - Controle de porta do sistema `0x61` (bits 0 e 1 para habilitação de porta de gating do PIT 2 e acionamento do cone do alto-falante).
  - Tabela completa de frequências de notas musicais (`NOTE_C3` a `NOTE_B6`) e estrutura de notas (`note_t`).
  - Suporte a controle de silenciamento global (*Mute*): `sound_set_mute()` e `sound_is_muted()`.
  - **Conjunto de Efeitos Sonoros Integrados (SFX)**:
    - **Mac OS Classic Startup Chime (`SFX_STARTUP`)**: Acorde arpejado de Fá Maior (F3 175Hz -> C4 262Hz -> F4 349Hz -> A4 440Hz -> C5 523Hz) reproduzido no boot do kernel.
    - **Beep de Alerta da GUI / Sosumi (`SFX_ALERT`)**: A5 880Hz (30ms) -> Pausa (10ms) -> F5 698Hz (60ms) disparado em caixas modais e alertas.
    - **Clique de Interface (`SFX_CLICK`)**: Pulso curto de 2500 Hz para cliques de botões, itens de menu e alternâncias.
    - **Esvaziamento de Lixeira / Ação Destrutiva (`SFX_TRASH`)**: Varredura tonal descendente (800Hz -> 500Hz -> 300Hz -> 150Hz) ao deletar arquivos/pastas.
    - **Confirmação de Sucesso / Save (`SFX_SUCCESS`)**: G5 784Hz (40ms) -> C6 1047Hz (120ms) ao salvar documentos no TextEdit e criar arquivos/pastas no disco ATA.
- **Driver de Relógio em Tempo Real (CMOS / RTC da Placa-Mãe)**:
  - Leitura direta dos registradores da bateria da placa-mãe através das portas de E/S `0x70` (índice) e `0x71` (dados).
  - Sincronização segura aguardando o ciclo UIP (*Update In Progress*) do Registrador de Status A (`0x0A`) para evitar leituras corrompidas na virada de segundos.
  - Decodificação automática de BCD (*Binary-Coded Decimal*) para binário conforme sinalizado pelo Registrador de Status B (`0x0B`), com suporte aos formatos 12h AM/PM e 24h.
  - Suporte ao registrador de século (`0x32`) e algoritmo do calendário Gregoriano com cálculo de anos bissextos.
  - Conversão bidirecional entre tempo estruturado (`rtc_time_t`) e timestamp *Unix Epoch* (segundos desde 1970).
  - **Relógio em Tempo Real na Barra de Menus da GUI**: Exibição contínua do horário real do computador (`HH:MM:SS`) no canto superior direito do desktop System 7, com widget suspenso interativo ao clicar sobre o relógio (exibindo Data Real, Hora Real e Uptime do sistema).
  - **Carimbos de Data/Hora Reais (Timestamps) no MIGFS**: Criação (`created_time`) e modificação (`modified_time`) de arquivos e pastas no disco ATA armazenam timestamps reais, visíveis na coluna de modificação no gerenciador `migOS HD` e no comando `ls` do terminal.
- Gerenciador de Memoria Fisica (PMM) com granularidade de frames de 4 KB gerenciados por mapa de bits (bitmap), protegendo o primeiro 1 MB de memoria fisica e fornecendo alocacao contigua de blocos.
- Alocador dinamico de memoria do Kernel (KHeap / kmalloc, kfree, kcalloc, krealloc) baseado em lista duplamente encadeada com cabecalhos protegidos por assinatura magica (0x1A2B3C4D), politica First-Fit, divisao automatica de blocos (splitting), fusao de blocos livres adjacentes (coalescing) e autoexpansao sob demanda via PMM.
- **Sistema de Arquivos Hierárquico e Persistente (MIGFS)**:
  - Suporte completo a diretórios e subdiretórios hierárquicos (`cd`, `pwd`, `mkdir`, `rmdir`, `mv`, `cp`, `touch`).
  - Gravação e leitura persistente de setores LBA no Disco Rígido ATA/IDE primário (Superbloco `0x4D494746` no LBA 1025, Tabela de Descritores de 64 bytes e Blocos de Dados).
  - Sincronização sob demanda com disco rígido (`sync`) e carregamento automático durante o boot preservando os carimbos de data/hora.
- Driver Grafico Bochs/QEMU BGA (Bochs Graphics Adapter) com leitura dinamica do barramento PCI para identificacao do endereco base do Linear Framebuffer (BAR0) e configuracao de modos de video de alta resolucao em 32-bit True Color.
- **Emulador de Game Boy (Peanut-GB) e Jogo Pokémon Bare-Metal**:
  - Emulador completo de Game Boy (DMG) portado para Ring 0 bare-metal sem sistema operacional hospedeiro.
  - Execução fluida de **Pokémon Red**, Pokémon Blue, Pokémon Yellow e outras ROMs comerciais de Game Boy a 60 FPS com upscaling limpo centralizado no display 640x480.
  - Suporte a controladores de memória de cartucho (MBC1, MBC3, MBC5 com Real-Time Clock RTC).
  - **Preservação e persistência automática de Saves (.sav)**: O progresso do jogador em Pokémon é salvo e mantido no disco ATA, garantindo que o jogo possa ser continuado mesmo após reiniciar o sistema.
  - Lançamento direto via terminal (`pokemon`, `gameboy <rom.gb>`, `gb <rom>`) ou via GUI Desktop através de ícone clássico ou duplo-clique em qualquer arquivo `.gb`.
- **Editor de Texto Visual Integrado (TextEdit & Nano/Edit)**:
  - No Terminal CLI: Comandos `edit <arquivo>` ou `nano <arquivo>` com interface de edição em tela cheia, navegação por cursor, atalhos de gravação (`Ctrl+S`) e saída (`Ctrl+Q`).
  - Na GUI Mac OS System 7: Aplicativo gráfico `TextEdit.app` com barra de ferramentas interativa (`Novo`, `Abrir`, `Salvar`, `Executar`, `Limpar`), indicador de alterações não salvas (`*`), cursor de texto e buffer de edição dinâmico.
- **Interpretador e Executor de Scripts (.txt)**:
  - Execução de scripts em lote via CLI (`run <script.txt>`, `exec <script.txt>`) ou diretamente pelo TextEdit.
  - Suporte a variáveis de ambiente (`set VAR=VAL`), expansão de variáveis (`$VAR`), controle de pausas (`sleep`, `pause`), impressão de mensagens (`echo`), alteração de cores (`color`) e comandos nativos do sistema.
  - **Calculadora Aritmética Embutida (`calc` / `eval`)**: Motor de análise sintática descendente recursiva (*Recursive Descent Parser*) com suporte a operadores (`+`, `-`, `*`, `/`), parênteses aninhados e precedência matemática correta.
- **Terminal CLI Avançado com Scrollbar & Ergonomia Linux (640x480)**:
  - 80 colunas por 30 linhas de texto com fonte bitmap 8x16 de alta nitidez e quebra inteligente na coluna 78.
  - **Buffer Unificado Contínuo (500 Linhas)**: Arquitetura idêntica a terminais modernos (PowerShell / Linux / Windows Terminal), mantendo histórico de comandos e saídas em ordem cronológica contínua.
  - **Barra Lateral de Rolagem (Scrollbar Visual na Coluna 79)**: Botões de navegação superior (`▲`) e inferior (`▼`), trilha contínua e bloco deslizante (*Thumb `█`*) dimensionado e posicionado proporcionalmente ao histórico da sessão.
  - **Navegação de Rolagem Completa**: Suporte nativo a Roda do Mouse (*Scroll Wheel*), cliques com mouse na barra lateral, e teclas `Page Up`, `Page Down`, `Shift + ↑` / `Shift + ↓` (1 linha suave) e `Shift + Page Up` / `Shift + Page Down` (topo / base).
  - **Indicador de Rolagem no Topo (*Overlay Badge*)**: Exibição elegante de `[^ +N lin]` informando a distância de rolagem e retorno automático ao prompt ao vivo ao digitar.
  - **Edição de Linha Interativa no Shell**: Navegação de cursor (`←`, `→`, `Home`, `End`), inserção de caracteres no meio da linha, deleção à frente (`Delete`) e atrás (`Backspace`).
  - **Atalhos POSIX / Linux CLI**: `Ctrl+C` (cancelar linha), `Ctrl+L` (limpar tela mantendo o prompt), `Ctrl+U` (apagar até o início), `Ctrl+K` (apagar até o fim), `Ctrl+W` (apagar palavra anterior), `Ctrl+A` (início da linha), `Ctrl+E` (fim da linha).
  - **Auto-Completar Inteligente (`Tab`)**: Completa comandos internos, executáveis, arquivos e pastas do diretório atual com detecção contextual.
  - **Histórico Estendido de Comandos**: Armazena até 32 comandos na sessão, comando `history` para listagem numerada e reexecução rápida por índice (`!n`) ou prefixo (`!str`).
  - **Comandos e Utilitários Padrão Linux / POSIX**: `echo`, `free` (diagnóstico de memória RAM), `df` (estatísticas do disco rígido ATA e MIGFS), `date` (data/hora e uptime), `help [comando]` contextual e aliases clássicos (`cls`, `dir`, `md`, `rd`, `del`, `type`, `unlink`).
  - Cursor visual animado e espelhamento serial instantâneo via porta COM1 (`0x3F8`).
  - Decodificador e transliterador de caracteres acentuados UTF-8 para a tabela IBM CP437.
  - Prompt inteligente colorido com diretório de trabalho corrente (`migOS:/docs>`).
- Interface Grafica Retro Mac OS System 7 Classic (640x480 @ 32-bit True Color):
  - Fundo pontilhado dithered 50% Gray Stipple.
  - Barra de menus superior fixa com logo do migOS, menus suspensos interativos ("migOS", "File", "Edit", "View", "Special", "Help") e relogio de uptime em tempo real.
  - Janelas amplas com estetica Platinum, barras de titulo com pinstripes horizontais, botao quadrado de fechar (Go-Away Box), sombra projetada e arraste suave com o mouse.
  - Janela de Perfil do Sistema ("System Profile" 460x340 px) com estatisticas detalhadas de hardware e memoria.
  - Janela do Gerenciador de Arquivos ("migOS HD" 460x330 px) com suporte a navegação por pastas, barra de ferramentas (`< Voltar`, `+ Pasta`, `+ Arq`, `Mover`, `Copiar`, `Excluir`), modais com caixas de texto e duplo-clique para abrir pastas e arquivos.
  - Janela do Editor de Texto ("TextEdit") para leitura e criação de arquivos de texto.
  - Icones interativos no Desktop ("migOS HD", "Pokemon", "Snake", "Terminal", "Trash") com suporte a selecao e duplo-clique.
  - Sprite classico do cursor do mouse System 7 renderizado suavemente no backbuffer antes da transferencia grafica.
- Driver de Teclado PS/2 com decodificacao de scancodes Set 1, teclas estendidas (0xE0), buffer de edicao no terminal, historico de comandos e fila assincrona circular de eventos (press/release) para aplicacoes graficas e jogos.
- Driver de Mouse PS/2 com decodificacao padronizada de 3 bytes, extensão de sinal aritmética, prevenção de conflito com o buffer de teclado, delimitacao de bordas de tela personalizavel (0..639 e 0..479) e suporte completo a arrasto e duplo clique na GUI.
- Biblioteca C Freestanding com implementacao completa de operacoes com memoria (`memset`, `memcpy`, `memmove`, `memcmp`), strings (`strcmp`, `strncmp`, `strcasecmp`, `strcpy`, `strncpy`, `strcat`, `strlen`, `strchr`, `strstr`, `strdup`, `strupr`), formatacao de texto (`printf`, `sprintf`, `snprintf`, `vsnprintf`, `sscanf`), utilitarios (`atoi`, `atol`, `strtol`, `itoa`, `qsort`, `rand`, `srand`) e rotinas aritmeticas para inteiros de 64 bits.
- Jogo da Cobrinha (Snake Game) integrado nativamente com arena ampliada (60x24), controle de velocidade progressiva por pontuacao, frutas bonus temporizadas, persistencia de recorde e tela de Game Over.

---

## Estrutura do Projeto

```
MigelSO/
│
├── .gitignore                       # Configuracao de exclusao de artefatos do controle de versao
├── README.md                        # Documentacao tecnica completa do projeto
├── build.ps1                        # Script de automacao de compilacao, linkagem, imagem e emulacao
├── linker.ld                        # Script do GNU Linker para posicionamento do Kernel em 0x10000
├── nasm.exe                         # Montador NASM portatil para ambiente Windows
├── PokemonRed.gb                    # ROM comercial de Pokémon Red Version embutida no disco ATA
│
├── boot/                            # Codigo do setor de inicializacao MBR
│   └── boot.asm                     # Bootloader em Assembly x86 16-bit Real Mode (512 bytes)
│
├── include/                         # Arquivos de cabecalho e interfaces publicas (.h)
│   ├── arch/                        # Cabecalhos de arquitetura do processador
│   │   └── i386/
│   │       ├── idt.h                # Estruturas da Interrupt Descriptor Table e ponteiro IDTR
│   │       ├── io.h                 # Primitivas inline de portas de I/O (inb, outb, inw, outw, inl, outl, io_wait)
│   │       ├── isr.h                # Estrutura de salvamento de contexto registers_t e ISRs
│   │       ├── pic.h                # Constantes e funcoes do controlador PIC 8259A
│   │       ├── reboot.h             # Rotina de reinicializacao por hardware e Triple Fault
│   │       └── timer.h              # Constantes e funcoes do temporizador PIT 8254
│   │
│   ├── drivers/                     # Cabecalhos dos drivers de dispositivos
│   │   ├── ata.h                    # Interface do driver de disco rígido ATA / IDE PIO
│   │   ├── bga.h                    # Interface do adaptador grafico Bochs/QEMU BGA (640x480 32-bit)
│   │   ├── keyboard.h               # Interface do driver de teclado PS/2
│   │   ├── mouse.h                  # Interface do driver de mouse PS/2
│   │   ├── rtc.h                    # Interface do driver de relogio em tempo real (CMOS / RTC)
│   │   ├── sound.h                  # Interface do driver de audio PC Speaker e efeitos sonoros (SFX)
│   │   ├── vga.h                    # Interface do terminal de texto em alta resolucao (80x30 / 640x480)
│   │   └── vga_mode13.h             # Interface legada de registradores VGA
│   │
│   ├── editor/                      # Cabecalho do editor de texto CLI/GUI
│   │   └── editor.h                 # Interface publica do editor de texto em tela cheia
│   │
│   ├── emulator/                    # Cabecalhos do emulador de Game Boy
│   │   ├── gameboy.h                # Interface de execucao, mapeamento e save game do Game Boy
│   │   └── peanut_gb.h              # Core do emulador Peanut-GB (CPU LR35902, PPU, MBC1/3/5)
│   │
│   ├── fs/                          # Cabecalho do sistema de arquivos
│   │   └── migfs.h                  # Estruturas de inode, diretorios e persistencia ATA
│   │
│   ├── games/                       # Cabecalho de jogos
│   │   └── snake.h                  # Interface do jogo da cobrinha (Snake Game)
│   │
│   ├── gui/                         # Cabecalhos da interface grafica
│   │   ├── font8x8.h                # Tabela de fonte bitmap 8x8 monocromatica de 128 caracteres ASCII
│   │   └── gui.h                    # Estruturas de janelas e primitivas de renderizacao da GUI 640x480
│   │
│   ├── interpreter/                 # Cabecalho do interpretador de scripts
│   │   └── txt_interp.h             # Motor de execucao de scripts .txt e calculadora com AST
│   │
│   ├── kernel/                      # Cabecalhos do nucleo do sistema operacional
│   │   ├── kheap.h                  # Interface do alocador dinamico de memoria do Kernel
│   │   └── pmm.h                    # Interface do gerenciador de memoria fisica por frames
│   │
│   ├── libc/                        # Cabecalhos da biblioteca C freestanding
│   │   ├── ctype.h                  # Funcoes de classificacao e conversao de caracteres
│   │   ├── stdarg.h                 # Macros de suporte a funcoes variadicas (va_list, va_start, va_end)
│   │   ├── stddef.h                 # Tipos fundamentais (size_t, ptrdiff_t, NULL)
│   │   ├── stdint.h                 # Tipos inteiros de largura exata (uint8_t, int32_t, uint32_t, uint64_t)
│   │   ├── stdio.h                  # Formatacao e manipulacao de texto em streams
│   │   ├── stdlib.h                 # Alocacao dinamica, conversao de strings e utilitarios
│   │   ├── string.h                 # Operacoes com blocos de memoria e strings C
│   │   └── time.h                   # Tipos de tempo, struct tm e prototipos time_t
│   │
│   └── shell/                       # Cabecalho do interpretador de comandos
│       └── shell.h                  # Interface de execucao do shell e buffer de comandos
│
├── kernel/                          # Codigo-fonte C do Kernel e drivers (.c)
│   ├── kernel.c                     # Ponto de entrada C do Kernel (kmain) e rotina de Kernel Panic
│   │
│   ├── arch/                        # Implementacao especifica da arquitetura i386
│   │   └── i386/
│   │       ├── idt.c                # Configuracao dos 256 portoes da IDT
│   │       ├── isr.c                # Tratamento de excecoes da CPU (0-31) e depuracao
│   │       ├── isr_asm.asm          # Stubs Assembly de baixo nivel para salvamento de contexto
│   │       ├── kernel_entry.asm     # Ponto de entrada de baixo nivel de 32 bits (_start em 0x10000)
│   │       ├── pic.c                # Remapeamento e controle das portas de I/O do PIC 8259A
│   │       ├── reboot.c             # Sequencia de reinicializacao rapida via porta 0x64 e Triple Fault
│   │       └── timer.c              # Tratador de IRQ0, contagem de ticks do PIT e temporizacao
│   │
│   ├── drivers/                     # Implementacao dos drivers de hardware
│   │   ├── ata/
│   │   │   └── ata.c                # Leitura e escrita de setores LBA em disco ATA PIO
│   │   ├── bga/
│   │   │   └── bga.c                # Driver Bochs/QEMU BGA, escaneamento PCI e controle LFB 640x480
│   │   ├── disk/                    # Drivers de armazenamento
│   │   ├── keyboard/
│   │   │   └── keyboard.c           # Tratador de IRQ1, tabela de scancodes e fila de eventos
│   │   ├── mouse/
│   │   │   └── mouse.c              # Tratador de IRQ12, decodificacao de 3 bytes e tracking de coordenadas
│   │   ├── rtc/
│   │   │   └── rtc.c                # Driver CMOS/RTC com BCD, UIP, 24h e conversao Unix Epoch
│   │   ├── sound/
│   │   │   └── sound.c              # Driver de audio PC Speaker (PIT Canal 2 / Porta 0x61) e SFX
│   │   └── vga/
│   │       ├── vga.c                # Renderizador de terminal em 640x480 com historico retroativo
│   │       └── vga_mode13.c         # Manipulador direto de registradores VGA
│   │
│   ├── editor/                      # Editor de texto visual
│   │   └── editor.c                 # Interface interativa de edicao em tela cheia no terminal
│   │
│   ├── emulator/                    # Emulador de Game Boy
│   │   └── gameboy.c                # Loop de execucao, renderizacao BGA, controles e saves .sav
│   │
│   ├── fs/                          # Implementacao do sistema de arquivos
│   │   └── migfs.c                  # Gerenciamento de pastas, arquivos e persistencia em disco ATA
│   │
│   ├── games/                       # Codigo-fonte dos jogos integrados
│   │   └── snake.c                  # Logica, colisao, pontuacao e renderizacao do Snake Game
│   │
│   ├── gui/                         # Codigo-fonte do subsistema grafico Mac OS System 7
│   │   └── gui.c                    # Gerenciador de janelas, pastas, TextEdit, Pokémon e duplo-clique
│   │
│   ├── interpreter/                 # Interpretador de scripts e calculadora
│   │   └── txt_interp.c             # Executor de scripts .txt e avaliador de expressoes com parser
│   │
│   └── memory/                      # Gerenciamento de memoria do Kernel
│       ├── kheap.c                  # Alocador dinamico de heap com coalescencia e divisao de blocos
│       └── pmm.c                    # Gerenciador de paginas fisicas (PMM) com bitmap de 4 KB
│
├── libc/                            # Implementacao da biblioteca C minimalista (.c)
│   ├── stdio.c                      # Implementacao de printf, sprintf, snprintf, vsnprintf e sscanf
│   ├── stdlib.c                     # Implementacao de itoa, atoi, atol, strtol, qsort, rand, srand
│   ├── string.c                     # Implementacao de memcpy, memset, memcmp, strcpy, strcmp, etc.
│   └── time.c                       # Implementacao de time, localtime, gmtime, mktime, asctime, ctime
│
└── shell/                           # Interpretador de comandos
    └── shell.c                      # Processamento de comandos hierarquicos, lancadores e utilitarios
```

---

## Detalhamento Arquivo por Arquivo

### 1. Arquivos da Raiz e Automacao

#### `build.ps1`
Script mestre de automacao em PowerShell responsavel por todo o pipeline de construcao do sistema:
1. Limpeza de artefatos de compilacoes anteriores na pasta `build/`.
2. Montagem dos modulos em Assembly (`boot/boot.asm`, `kernel/arch/i386/kernel_entry.asm`, `kernel/arch/i386/isr_asm.asm`) utilizando o `nasm.exe`.
3. Compilacao de todos os modulos C do Kernel, Drivers, LibC, GUI, Jogos e Shell com GCC i686/x86 (`-m32 -ffreestanding -fno-builtin -nostdlib -O2 -Wall -Wextra`).
4. Linkagem dos objetos atraves do GNU Linker (`ld`) utilizando o script de linkagem `linker.ld`, gerando a imagem intermediaria `build/kernel.tmp` e extraindo o binario bruto com `objcopy`.
5. Preenchimento de alinhamento (padding) do Kernel para exatamente 524.288 bytes (1024 setores).
6. Construcao da imagem de disco unificada `migOS.img` combinando o setor de boot MBR (512 bytes) e o binario do Kernel alinhado.
7. Execucao automatica da imagem no emulador QEMU (`qemu-system-x86_64` ou `qemu-system-i386`) com 64 MB de memoria RAM e placa de video padrao (`-vga std`).

#### `linker.ld`
Script de linkagem que define a organizacao espacial do Kernel na memoria fisica:
- Define o ponto de entrada simbolico na funcao `_start`.
- Posiciona a secao de codigo executavel (`.text`) no endereco fisico `0x10000` (64 KB), garantindo que o modulo `kernel_entry.o` seja o primeiro objeto a ser executado logo apos o salto do bootloader.
- Alinha as secoes subsequentes (`.rodata`, `.data` e `.bss`) em fronteiras de 4 KB (0x1000), reservando espaco para variaveis globais e declarando os simbolos de inicio e fim de cada secao (`_text_start`, `_text_end`, `_data_start`, `_data_end`, `_bss_start`, `_bss_end` e `_kernel_end`).

#### `nasm.exe`
Binario executavel portatil do Netwide Assembler (NASM) de 32/64 bits para ambiente Windows, utilizado pelo script de automacao para montagem dos arquivos `.asm` sem necessidade de instalacao global de ferramentas externas.

#### `.gitignore`
Arquivo de configuracao do Git responsavel por ignorar arquivos binarios gerados na esteira de compilacao (`*.o`, `*.bin`, `*.tmp`, `*.img`) e pastas temporarias (`build/`), mantendo o repositorio limpo.

---

### 2. Bootloader

#### `boot/boot.asm`
Codigo de inicializacao gravado no primeiro setor do disco (Setor 0 / MBR, 512 bytes finalizados com a assinatura magica `0xAA55`). Executado pelo processador em Modo Real de 16 bits no endereco `0x7C00`:
- Configura os registradores de segmento (`CS=0`, `DS=0`, `ES=0`, `SS=0`) e inicializa a pilha do bootloader em `0x7C00`.
- Salva o numero da unidade de disco de boot fornecida pela BIOS no registrador `DL`.
- Exibe mensagens de progresso na tela via interrupcao de video da BIOS (`INT 0x10, AH=0x0E`).
- Carrega 1024 setores consecutivos do Kernel (512 KB) a partir do setor 1 do disco para a memoria fisica no endereco `0x10000` (64 KB). A leitura utiliza o protocolo LBA estendido da BIOS (`INT 0x13, AH=0x42`) com uma estrutura DAP (Disk Address Packet) iterando em blocos seguros de 64 setores.
- Habilita a Linha de Endereco A20 atraves do metodo Fast A20 Gate (porta `0x92`), garantindo acesso irrestrito aos 4 GB de memoria fisica sem envelopamento no primeiro megabyte.
- Carrega uma Global Descriptor Table (GDT) temporaria com 3 descritores planos (Null Descriptor, Code Segment 0x08 de base 0x0 limite 4GB e Data Segment 0x10 de base 0x0 limite 4GB).
- Desativa interrupcoes mascaraveis (`cli`), ativa o bit 0 (Protected Mode Enable - PE) do registrador de controle `CR0` e realiza um salto longo (`jmp 0x08:init_pm`) para descarregar o pipeline da CPU e ingressar definitivamente no Modo Protegido de 32 bits.
- No bloco de 32 bits, recarrega os registradores de segmento de dados (`DS`, `ES`, `FS`, `GS`, `SS`) com o seletor `0x10`, reposiciona a pilha em `0x90000` e salta para o endereco base do Kernel em `0x10000`.

---

### 3. Arquitetura i386 e Hardware

#### `include/arch/i386/io.h`
Cabecalho contendo primitivas de manipulacao direta de portas de entrada/saida (I/O Ports) x86 implementadas como funcoes `static inline` em assembly embutido GNU C:
- `outb(port, val)`: Escreve 1 byte de dados em uma porta de I/O especificada.
- `inb(port)`: Le 1 byte de dados a partir de uma porta de I/O especificada.
- `outw(port, val)`: Escreve uma palavra de 16 bits (word) em uma porta de I/O.
- `inw(port)`: Le uma palavra de 16 bits (word) a partir de uma porta de I/O.
- `outl(port, val)`: Escreve uma palavra dupla de 32 bits (dword) em uma porta de I/O.
- `inl(port)`: Le uma palavra dupla de 32 bits (dword) a partir de uma porta de I/O.
- `io_wait()`: Executa uma escrita nula na porta `0x80` para introduzir um pequeno atraso de sincronizacao de hardware.

#### `include/arch/i386/idt.h` e `kernel/arch/i386/idt.c`
Definicao e implementacao da Tabela de Descritores de Interrupcao (Interrupt Descriptor Table - IDT):
- Estrutura `idt_entry_t` empacotada (`__attribute__((packed))`) representando um descritor de portao de interrupcao de 64 bits (offset baixo, seletor de segmento, zero reservado, flags de tipo/DPL/presente e offset alto).
- Estrutura `idt_ptr_t` contendo o limite de tamanho da tabela e o ponteiro base de 32 bits para a instrucao `lidt`.
- A funcao `idt_set_gate(num, base, sel, flags)` configura um vetor especifico.
- A funcao `idt_init()` inicializa os 256 vetores da IDT com zeros, vincula as 32 ISRs de excecoes da CPU, vincula as 16 IRQs dos controladores PIC e carrega a tabela no processador atraves da chamada de baixo nivel `idt_flush`.

#### `include/arch/i386/isr.h` e `kernel/arch/i386/isr.c`
Mecanismo de captura e tratamento das 32 excecoes nativas da arquitetura x86:
- Estrutura `registers_t` contendo o estado integral da CPU no momento do disparo da interrupcao (registradores de segmento `DS`, registradores de proposito geral `EDI`, `ESI`, `EBP`, `ESP`, `EBX`, `EDX`, `ECX`, `EAX`, numero da interrupcao `int_no`, codigo de erro `err_code` e contexto salvo pelo hardware `EIP`, `CS`, `EFLAGS`, `useresp`, `SS`).
- A funcao `isr_handler(registers_t* regs)` identifica a excecao recebida e, caso seja uma falha grave (como Divisao por Zero, Falha de Protecao Geral ou Page Fault), dispara o `kernel_panic` com o relatorio de registradores na tela.

#### `kernel/arch/i386/isr_asm.asm`
Rotinas em Assembly NASM de baixo nivel para entrada e saida de todas as interrupcoes:
- Cria os stubs individuais `isr0` a `isr31` (empilhando codigo de erro dummy para as excecoes que nao o fornecem nativamente).
- Cria os stubs individuais `irq0` a `irq15` (mapeados nos vetores 32 a 47).
- Encaminha o fluxo para `isr_common_stub` e `irq_common_stub`, que salvam todos os registradores na pilha (`pusha`), ajustam os seletores de segmento para o espaco de dados do kernel (`0x10`), passam o ponteiro da pilha como parametro para o tratador em C (`isr_handler` ou `irq_handler`) e, ao retornar, restauram o contexto com `popa` e executam a instrucao de retorno de interrupcao `iret`.

#### `kernel/arch/i386/kernel_entry.asm`
Ponto de entrada de baixo nivel de 32 bits do Kernel:
- Posicionado explicitamente no inicio da secao `.text` pelo linker script.
- Define o simbolo global `_start`.
- Configura o registrador de pilha `ESP` no endereco seguro `0x90000`.
- Zera a secao `.bss` na memoria RAM iterando de `_bss_start` a `_bss_end`.
- Invoca a funcao de inicializacao C `kmain`.
- Caso `kmain` retorne, desativa interrupcoes (`cli`) e trava o processador em laco de parada infinita (`hlt`).

#### `include/arch/i386/pic.h` e `kernel/arch/i386/pic.c`
Driver de controle e remapeamento dos controladores de interrupcao programaveis (PIC 8259A Master e Slave):
- Define as portas de comando e dados do PIC Master (`0x20` e `0x21`) e PIC Slave (`0xA0` e `0xA1`).
- A funcao `pic_remap(offset1, offset2)` envia a sequencia de palavras de controle de inicializacao (ICW1 a ICW4) para redirecionar as linhas de interrupcao por hardware IRQ 0..7 para os vetores da IDT 32..39 e IRQ 8..15 para os vetores 40..47, evitando conflitos com as 32 excecoes nativas da CPU.
- Funcoes `pic_send_eoi(irq)` para sinalizacao de fim de interrupcao (End of Interrupt) e `pic_set_mask` / `pic_clear_mask` para mascara de linhas individuais.

#### `include/arch/i386/timer.h` e `kernel/arch/i386/timer.c`
Driver do temporizador de intervalo programavel (PIT 8254) e servicos de tempo:
- Porta de canal de dados 0 (`0x40`) e porta de controle de comando (`0x43`).
- A funcao `timer_init(freq)` configura o canal 0 do PIT no Modo 3 (Square Wave Generator) calculando o divisor com base na frequencia base de oscilacao de 1.193.180 Hz (configurado padrao em 100 Hz = 10 ms por tick).
- A funcao `timer_handler` atende ao IRQ 0, incrementando a contagem global de ticks e calculando o tempo de atividade (`uptime`) em segundos e milissegundos.
- Funcoes de temporizacao de alta precisao `timer_get_ticks()`, `get_uptime()`, `timer_get_uptime_ms()`, `timer_wait(ticks)` e `sleep(ms)` utilizando a instrucao `hlt` para economia de consumo de ciclos da CPU durante as esperas.

#### `include/arch/i386/reboot.h` e `kernel/arch/i386/reboot.c`
Rotina de reinicializacao fisica do sistema:
- A funcao `reboot_system()` tenta inicialmente enviar o comando de pulso de reinicializacao na linha de reset da placa-mae atraves do controlador de teclado PS/2 (porta `0x64`, comando `0xFE`).
- Caso o hardware nao responda imediatamente, carrega um ponteiro de IDT com limite nulo (`lidt [0]`) e dispara uma interrupcao por software (`int 3`), gerando um Triple Fault intencional que forca a CPU x86 a reiniciar instantaneamente o computador.

---

### 4. Nucleo do Kernel

#### `kernel/kernel.c`
Arquivo central de inicializacao e tratamento de panico do Kernel:
- A funcao `kmain()` executa a sequencia ordenada de subida dos subsistemas do sistema operacional:
  1. Inicializa o terminal de video em alta resolucao 640x480 (`vga_init`).
  2. Inicializa e carrega a tabela IDT de 256 vetores (`idt_init`).
  3. Remapeia o controlador de interrupcoes PIC 8259A (`pic_init`).
  4. Configura o temporizador PIT 8254 em 100 Hz (`timer_init`).
  5. Habilita as interrupcoes da CPU com a instrucao `sti`.
  6. Inicializa o gerenciador de paginas de memoria fisica (`pmm_init`).
  7. Inicializa o alocador dinamico de memoria Heap com 8 MB de capacidade inicial (`kheap_init`).
  8. Inicializa o sistema de arquivos em RAMDisk MIGFS com arquivos pre-embutidos (`migfs_init`).
  9. Inicializa o driver de teclado PS/2 (`keyboard_init`).
  10. Inicializa o driver de mouse PS/2 com suporte a IntelliMouse (`mouse_init`).
  11. Exibe a tela de boas-vindas do sistema e transfere o controle de execucao para o interpretador de comandos (`shell_run`).
- A funcao `kernel_panic(message, regs)` implementa a rotina de encerramento de seguranca por falha critica:
  - Desativa interrupcoes (`cli`).
  - Limpa a tela com fundo azul e renderiza o relatorio de diagnostico tecnico contendo a causa da falha, instrucao de falha (`EIP`), seletor de codigo (`CS`), flags da CPU (`EFLAGS`) e o dump completo dos registradores `EAX`, `EBX`, `ECX`, `EDX`, `ESI`, `EDI`, `EBP` e `ESP`.
  - Executa uma contagem regressiva visual de 10 segundos sincronizada por polling de hardware e executa a reinicializacao automatica da maquina.

---

### 5. Gerenciamento de Memoria

#### `include/kernel/pmm.h` e `kernel/memory/pmm.c`
Gerenciador de Memoria Fisica (Physical Memory Manager - PMM):
- Baseado em granularidade de frames de pagina de 4 KB (4.096 bytes).
- Gerencia ate 64 MB de memoria fisica atraves de um mapa de bits compacto (bitmap) onde cada bit representa o estado de ocupacao (0 = livre, 1 = ocupado) de um frame de 4 KB.
- A funcao `pmm_init(total_memory)` zera o bitmap, protege todo o primeiro 1 MB de memoria (reservado para BIOS, IVT, BDA, memoria de video VGA e memoria convencional) e marca os frames ocupados pelo binario do Kernel.
- `pmm_alloc_frame()`: Varre o bitmap em busca do primeiro bit livre, marca-o como ocupado e retorna o endereco fisico do frame.
- `pmm_free_frame(address)`: Converte o endereco fisico no indice do frame e limpa o bit correspondente no bitmap.
- `pmm_alloc_contiguous_frames(count)`: Localiza e aloca um bloco sequencial de frames contiguos na memoria fisica.
- Metricas de inspecao: `pmm_get_free_frames()`, `pmm_get_used_frames()` e `pmm_get_total_memory()`.

#### `include/kernel/kheap.h` e `kernel/memory/kheap.c`
Alocador dinamico de memoria do Kernel (Kernel Heap Manager):
- Gerencia uma regiao continua de memoria dinamica a partir de `0x200000` (2 MB) com capacidade expansivel sob demanda solicitando frames ao PMM.
- Implementado sobre uma lista duplamente encadeada de blocos de memoria (`kheap_block_t`).
- Cada bloco contem um cabecalho com assinatura magica de integridade (`KHEAP_MAGIC = 0x1A2B3C4D`), flag de ocupacao (`is_free`), tamanho util de dados (`size`) e ponteiros para os blocos anterior (`prev`) e proximo (`next`).
- Funcoes publicas:
  - `kmalloc(size)`: Aloca um bloco de memoria utilizando a estrategia First-Fit com alinhamento de 4 bytes e divisao automatica do bloco remanescente (splitting).
  - `kfree(ptr)`: Valida a assinatura magica do cabecalho, marca o bloco como livre e executa a fusao imediata (coalescing) com blocos livres vizinhos para eliminar a fragmentacao externa.
  - `kcalloc(num, size)`: Aloca memoria e preenche todos os bytes com zero.
  - `krealloc(ptr, size)`: Realoca um bloco de memoria existente preservando os dados anteriores.
- Servicos de telemetria: `kheap_get_used_bytes()`, `kheap_get_free_bytes()` e `kheap_print_stats()`.

---

### 6. Drivers de Dispositivos

#### `include/drivers/bga.h` e `kernel/drivers/bga/bga.c`
Driver do adaptador grafico Bochs/QEMU BGA (Bochs Graphics Adapter):
- Controla a placa de video atraves das portas de E/S `0x01CE` (indice) e `0x01CF` (dados).
- Implementa rotina de escaneamento de dispositivos no barramento PCI para localizar controladores de display (Classe 0x03) ou adaptadores Bochs/QEMU (Vendor ID `0x1234`), lendo o registrador BAR0 para obter dinamicamente o endereco fisico do Linear Framebuffer (LFB).
- A funcao `bga_init()` ativa a aceleracao grafica em resolucao de 640x480 pixels a 32-bit True Color (ARGB/XRGB).
- `bga_putpixel(x, y, color)`: Grava pixels diretamente no LFB.
- `bga_clear(color)`: Limpa a tela inteira em 32 bits.
- `bga_blit(buffer)`: Transfere um backbuffer completo de 1.228.800 bytes (640x480x4) diretamente para a memoria de video de hardware em alta velocidade.

#### `include/drivers/vga.h` e `kernel/drivers/vga/vga.c`
Driver de terminal de texto de alta resolucao em 640x480:
- **Buffer Unificado Contínuo (500 Linhas)**: Implementa matriz unificada `term_buffer[500][80]` operando com janela deslizante (*Viewport* de 30 linhas), garantindo rolagem 100% contínua e sem corte de texto, idêntica ao Windows Terminal, PowerShell e Linux.
- **Barra Lateral de Rolagem (Scrollbar na Coluna 79)**: Renderiza botão superior `▲` (char 30), botão inferior `▼` (char 31), trilha contínua vertical e bloco deslizante (*Thumb `█`* em ciano, char 127) dimensionado e posicionado em tempo real proporcionalmente à extensão do histórico.
- **Indicador de Rolagem no Topo (*Overlay Badge*)**: Quando o usuário visualiza linhas anteriores, um badge `[^ +N lin]` é exibido no canto superior direito indicando a distância exata em relação ao prompt.
- **Quebra Inteligente na Coluna 78**: O texto e o cursor quebram automaticamente antes da coluna 79, mantendo a barra de rolagem sempre visível e limpa.
- **Retorno Automático ao Vivo**: Se o usuário estiver lendo o histórico e começar a digitar, o terminal volta instantaneamente para a linha de comando atual.
- Suporte a 16 cores clássicas do padrão VGA/ANSI mapeadas em 32-bit True Color.
- Cursor visual animado e espelhamento simultâneo de todos os caracteres para a porta serial COM1 (`0x3F8`).
- Decodificador UTF-8 para exibição de caracteres acentuados da língua portuguesa (á, é, í, ó, ú, ç, ã, õ, etc.).
- Funções `vga_scroll_history_up(lines)`, `vga_scroll_history_down(lines)`, `vga_scroll_history_reset()` e `vga_get_scroll_offset()`.
- A função `vga_set_cell(x, y, c, fg, bg)` permite aos jogos e utilitários manipularem células de texto individuais com atualização visual imediata.

#### `include/drivers/vga_mode13.h` e `kernel/drivers/vga/vga_mode13.c`
Driver de manipulacao direta de registradores de hardware da controladora VGA padrao (MISC, Sequencer, CRTC, Graphics Controller e Attribute Controller) e operacoes de paleta DAC de 256 cores.

#### `include/drivers/ata.h` e `kernel/drivers/disk/ata.c`
Driver de disco rígido ATA / IDE em Modo PIO (Programmed Input/Output):
- Opera sobre o canal primario do controlador ATA (portas `0x1F0` a `0x1F7` e porta de controle `0x3F6`).
- Funcoes `ata_read_sectors(lba, count, buffer)` e `ata_write_sectors(lba, count, buffer)` para leitura e gravacao de setores de 512 bytes via enderecamento LBA de 28 bits, aguardando o estado de prontidao da controladora atraves de polling no registrador de Status (`0x1F7`).

#### `include/drivers/keyboard.h` e `kernel/drivers/keyboard/keyboard.c`
Driver do teclado padrao PS/2 (porta de dados `0x60` e porta de status `0x64`):
- Atendido na rotina de interrupcao IRQ 1 (vetor 33).
- Decodifica scancodes do Set 1, tratando teclas de modificacao (Shift, Ctrl, Alt, Caps Lock) e teclas estendidas (`0xE0`).
- **Edição Interativa de Linha**: Navegação por cursor (`←` e `→`), início (`Home`), fim (`End`), inserção no meio do texto, deleção à frente (`Delete`) e atrás (`Backspace`).
- **Atalhos POSIX / Linux CLI com `Ctrl`**:
  - `Ctrl+C`: Cancela a linha atual e imprime `^C`, restaurando o prompt limpo.
  - `Ctrl+L`: Limpa a tela do terminal e redessina o prompt com o texto atual.
  - `Ctrl+U`: Apaga todo o texto do início da linha até o cursor.
  - `Ctrl+K`: Apaga todo o texto do cursor até o final da linha.
  - `Ctrl+W`: Apaga a palavra anterior ao cursor.
  - `Ctrl+A`: Move o cursor instantaneamente para o início da linha.
  - `Ctrl+E`: Move o cursor instantaneamente para o fim da linha.
- **Navegação de Rolagem por Teclado**:
  - `Page Up` / `Page Down`: Rola 10 linhas para cima / para baixo.
  - `Shift + ↑` / `Shift + ↓`: Rola suavemente 1 linha para cima / para baixo.
  - `Shift + Page Up` / `Ctrl + Home`: Salta direto para o topo do histórico.
  - `Shift + Page Down` / `Ctrl + End`: Salta direto para a base (prompt ao vivo).
- **Auto-Completar Inteligente (`Tab`)**: Dispara a função `shell_autocomplete()` para autocompletar comandos e caminhos.
- Mantém fila circular assíncrona de eventos (`keyboard_get_doom_key`) para uso em jogos (Game Boy, Snake, etc.).

#### `include/drivers/mouse.h` e `kernel/drivers/mouse/mouse.c`
Driver do mouse PS/2 com suporte ao protocolo IntelliMouse:
- Atendido na rotina de interrupcao IRQ 12 (vetor 44) atraves do controlador auxiliar PS/2.
- Inicializa e ativa a extensao IntelliMouse enviando a sequencia magica de amostragem (200, 100, 80) para habilitar o pacote de 4 bytes com suporte à roda de rolagem vertical (*Scroll Wheel*).
- **Integração com a Barra de Rolagem do Terminal**:
  - Rolar a rodinha do mouse para **cima**: Sobe a visualização do histórico (`vga_scroll_history_up(3)`).
  - Rolar a rodinha do mouse para **baixo**: Desce a visualização até o prompt (`vga_scroll_history_down(3)`).
  - Clicar na seta superior (`▲`) ou inferior (`▼`) da barra lateral com o botão esquerdo: Executa rolagem interativa.
- Decodifica movimento relativo nos eixos X e Y, estado dos botoes esquerdo, direito e central, e pulsos da roda de rolagem.
- Implementa delimitacao configuravel de bordas de tela (`mouse_set_bounds`) para integracao nativa tanto na resolucao de 640x480 quanto no terminal.

#### `include/drivers/rtc.h` e `kernel/drivers/rtc/rtc.c`
Driver do Relógio em Tempo Real (CMOS / Real-Time Clock) da placa-mãe:
- Opera diretamente com a controladora CMOS através da porta de comando/endereço `0x70` e porta de dados `0x71`.
- **Sincronização UIP (*Update In Progress*)**: Realiza espera ativa e verificação de integridade no bit 7 do *Status Register A* (`0x0A`), executando dupla leitura confirmatória para evitar anomalias durante a transição de segundos do hardware.
- **Decodificação BCD & Modos de Horário**: Inspeciona os bits de configuração do *Status Register B* (`0x0B`) para identificar automaticamente se os registradores de tempo estão em formato BCD (*Binary-Coded Decimal*) ou binário puro, convertendo valores BCD para binário e ajustando formatos 12h AM/PM para 24h.
- **Suporte ao Século & Calendário Gregoriano**: Leitura do registrador de século `0x32` (quando disponível) e algoritmo completo para conversão bidirecional entre a data/hora do RTC (`rtc_time_t`) e o timestamp *Unix Epoch* (segundos decorridos desde 01/01/1970 00:00:00 UTC), com suporte exato a anos bissextos.
- **Formatação de Strings & Cache Otimizado**: Funções `rtc_format_datetime`, `rtc_format_date`, `rtc_format_time_full`, `rtc_format_time`, `rtc_format_epoch` e `rtc_format_epoch_short`, além de leitura rápida em cache (`rtc_get_time` e `rtc_get_unix_timestamp`) para atualização fluida a 60 FPS na interface gráfica sem gargalo no barramento de I/O.

#### `include/drivers/sound.h` e `kernel/drivers/sound/sound.c`
Driver de áudio para PC Speaker e gerador de efeitos sonoros (SFX):
- **Canal 2 do PIT 8254**: Configura o gerador de onda quadrada através da porta de comando `0x43` com o byte de controle `0xB6` e programa o divisor de 16 bits na porta de dados `0x42` baseado no oscilador de 1.193.180 Hz ($divisor = 1193180 / freq\_hz$).
- **Porta de Controle B (`0x61`)**: Habilita e desabilita os bits 0 (gate do PIT 2) e 1 (dados do alto-falante) para iniciar e cessar a oscilação acústica.
- **Gerenciamento de Mudo**: Flag global e funções `sound_set_mute()` e `sound_is_muted()` para silenciar o sistema a qualquer momento.
- **Tabela de SFX Integrados**: Execução sequencial de notas (`note_t`) com frequências em Hz e durações em ms:
  - `SFX_STARTUP`: Mac OS Classic Startup Chime (Acorde arpejado F3 -> C4 -> F4 -> A4 -> C5).
  - `SFX_ALERT`: Beep de Alerta da GUI / Sosumi (A5 880Hz -> Pausa -> F5 698Hz).
  - `SFX_CLICK`: Clique de interface (pulso ultra-rápido de 2500 Hz).
  - `SFX_TRASH`: Esvaziamento de Lixeira / Ação destrutiva (varredura descendente 800Hz -> 150Hz).
  - `SFX_SUCCESS`: Confirmação de Sucesso / Gravação no Disco ATA (G5 784Hz -> C6 1047Hz).
- **Primitivas de Áudio**: `sound_init()`, `sound_tone(freq)`, `sound_stop()`, `sound_beep(freq, dur)`, `sound_play_sfx(sfx)` e `sound_play_melody(notes, count)`.

---

### 7. Sistema de Arquivos (MIGFS / RAMDisk & Disco ATA)

#### `include/fs/migfs.h` e `kernel/fs/migfs.c`
Sistema de arquivos hierarquico, persistente e com carimbos de data/hora do migOS:
- **Estrutura de Descritores com Timestamps**: Gerencia uma tabela de descritores de arquivos e pastas (`migfs_file_t` e `migfs_disk_entry_t` de exatos 64 bytes). Cada entrada contem nome/caminho (32 bytes), tamanho em bytes (`size`), flags de permissao e tipo (`MIGFS_FILE_READONLY`, `MIGFS_FILE_DIRECTORY`, `MIGFS_FILE_INUSE`), setor inicial (`start_sector`), contagem de setores (`sector_count`), e carimbos de data/hora reais: `created_time` (Unix timestamp de criação) e `modified_time` (Unix timestamp da última alteração).
- **Suporte Hierarquico a Diretorios**:
  - Funcoes de navegacao e manipulacao de pastas: `migfs_mkdir`, `migfs_rmdir`, `migfs_move`, `migfs_copy`, `migfs_is_dir`, `migfs_get_dir_items`, `migfs_path_combine` e resolucao inteligente de caminhos relativos e absolutos (`..`, `/`).
  - Criacao automatica de pastas padrao do sistema (`/docs`, `/scripts`) e arquivos de demonstracao durante a montagem inicial.
- **Carimbos de Tempo em Tempo Real**:
  - Toda operação de escrita (`migfs_create`, `migfs_add_buffer`, `migfs_write`, `migfs_append`, `migfs_mkdir`, `migfs_move`, `migfs_copy`) e o utilitário `migfs_touch` obtêm o horário real da bateria da placa-mãe via `rtc_get_unix_timestamp()`.
- **Persistencia Real em Disco Rigido ATA/IDE**:
  - Implementa layout de disco seguro a partir do setor LBA 1025 (posicionado logo apos o Kernel de 512 KB).
  - Superbloco gravado no LBA 1025 com assinatura magica `0x4D494746` (`MIGF`), contagem de arquivos e setores alocados.
  - Gravacao de tabela de descritores de 64 bytes (preservando timestamps) e blocos de dados contiguos de 512 bytes diretamente no disco ATA em modo PIO.
  - A funcao `migfs_sync_to_disk()` executa a sincronizacao sob demanda de todas as alteracoes feitas na sessao para a midia de disco, enquanto `migfs_load_from_disk()` restaura o estado completo e metadados no boot.

---

### 8. Biblioteca C Minimalista (Freestanding LibC)

#### `include/libc/stdint.h` e `include/libc/stddef.h`
Definicao padrao de tipos inteiros exatos (`uint8_t`, `int8_t`, `uint16_t`, `int16_t`, `uint32_t`, `int32_t`, `uint64_t`, `int64_t`), ponteiros (`uintptr_t`, `intptr_t`), tamanhos (`size_t`, `ptrdiff_t`) e o ponteiro nulo `NULL`.

#### `include/libc/stdarg.h`
Implementacao portatil das macros de tratamento de argumentos variaveis (`va_list`, `va_start`, `va_arg`, `va_copy`, `va_end`) utilizando as funcoes intrinsecas do compilador GCC (`__builtin_va_*`).

#### `include/libc/string.h` e `libc/string.c`
Implementacao completa de operacoes com memoria e strings:
- Operacoes com blocos de memoria: `memset`, `memcpy`, `memmove`, `memcmp`, `memchr`.
- Manipulacao de cadeias de caracteres: `strlen`, `strcpy`, `strncpy`, `strcat`, `strncat`, `strcmp`, `strncmp`, `strcasecmp`, `strncasecmp`, `strchr`, `strrchr`, `strstr`, `strdup`, `strupr`, `strlwr`.

#### `include/libc/stdlib.h` e `libc/stdlib.c`
Funcoes utilitarias de conversao numerica, ordenacao e geracao de numeros pseudoaleatorios:
- Conversoes de texto para numero: `atoi`, `atol`, `strtol`.
- Conversao de numero para texto: `itoa` (com suporte a qualquer base numerica de 2 a 36).
- Algoritmo de ordenacao Quicksort generico `qsort`.
- Gerador de numeros pseudoaleatorios congruencial linear `rand` e `srand`.

#### `include/libc/stdio.h` e `libc/stdio.c`
Motor de formatacao de texto completo:
- Implementacao de `printf`, `sprintf`, `snprintf` e `vsnprintf` com suporte a especificadores de formato `%d`, `%i`, `%u`, `%x`, `%X`, `%p`, `%s`, `%c` e `%%`, incluindo especificadores de largura de campo, preenchimento com zeros (`%02x`, `%08x`), alinhamento a esquerda (`%-10s`) e especificadores de precisao.
- Implementacao de `sscanf` para analise sintatica de strings com especificadores `%d`, `%s` e `%c`.

#### `include/libc/time.h` e `libc/time.c`
Implementação da interface padrão C para manipulação e formatação de tempo:
- Definição de `time_t` e estrutura `struct tm` padrão (`tm_sec`, `tm_min`, `tm_hour`, `tm_mday`, `tm_mon`, `tm_year`, `tm_wday`, `tm_yday`, `tm_isdst`).
- Funções `time()`, `localtime()`, `gmtime()`, `mktime()`, `asctime()` e `ctime()` integradas ao driver CMOS/RTC para compatibilidade com aplicações C padrão.

---

### 9. Shell Interativo

#### `include/shell/shell.h` e `shell/shell.c`
Interpretador de comandos avancado com prompt hierarquico (`migOS:/docs>`):
- Opera de forma desacoplada da rotina de interrupcao do teclado, processando comandos assíncronos e atualizando o terminal de 640x480.
- **Auto-Completar Inteligente (`Tab`)**:
  - Pressionar `Tab` completa automaticamente o nome de comandos internos, scripts, executáveis e arquivos/pastas locais. Se houver múltiplos matches, lista as opções de forma limpa.
- **Histórico Estendido e Reexecução Rápida**:
  - Buffer de histórico com capacidade para 32 comandos na memória.
  - Navegação fluida com setas `↑` e `↓`.
  - Comando `history`: Lista todos os comandos executados com numeração ordinal.
  - Execução por índice `!n`: Executa o comando de número `n` do histórico (ex: `!3`).
  - Execução por prefixo `!str`: Executa o comando mais recente iniciado por `str` (ex: `!pok` executa `pokemon`).
- **Conjunto de Comandos e Ergonomia Linux/POSIX**:
  - **Navegacao e Diretorios**: `cd <dir>` (troca diretorio), `pwd` (exibe diretorio atual), `mkdir <pasta>` (cria pasta), `rmdir <pasta>` (remove pasta vazia), `mv <orig> <dest>` (move/renomeia), `cp <orig> <dest>` (copia arquivo ou pasta).
  - **Gerenciamento de Arquivos**: `ls [dir]` (listagem hierarquica com `<DIR>`, `<FILE>`, tamanho, carimbo de modificação e atributos), `ls -a` (visao flat global com timestamps), `cat <arquivo>` (exibe texto), `touch <arquivo>` (cria arquivo ou atualiza timestamp real), `write <arq> <txt>` (escreve texto), `rm <arquivo>` (remove arquivo), `sync` (sincroniza RAMDisk com disco ATA).
  - **Utilitários Padrão Linux / POSIX**:
    - `echo <texto>`: Imprime texto no terminal com suporte a quebra de linha.
    - `free`: Exibe relatório de memória RAM física (PMM) e dinâmica (KHeap) com total, uso e livre formatados em KB/MB.
    - `df`: Exibe o espaço total, utilizado e livre na partição persistente MIGFS / Disco ATA em KB.
    - `date` / `time`: Exibe a data/hora real decodificada do hardware CMOS RTC (`DD/MM/YYYY HH:MM:SS`), tempo de atividade (*uptime*) e timestamp Unix atual.
    - `rtc` / `clock` / `cmos`: Painel de diagnóstico do hardware CMOS/RTC com portas I/O (`0x70`/`0x71`), decodificação BCD, modo 24h, data/hora e status de energia da bateria da placa-mãe.
    - `help [comando]`: Exibe a lista completa de comandos organizados por categoria ou a ajuda detalhada do comando solicitado.
  - **Aliases de Compatibilidade (DOS / Windows / Linux)**:
    - `cls` -> `clear`
    - `dir` -> `ls`
    - `md` -> `mkdir`
    - `rd` -> `rmdir`
    - `del` / `unlink` -> `rm`
    - `type` -> `cat`
    - `vi` / `vim` -> `edit`
    - `time` -> `date`
    - `clock` / `cmos` -> `rtc`
  - **Edicao e Scripts**: `edit <arquivo>` ou `nano <arquivo>` (abre editor visual no terminal), `run <script.txt>` ou `exec <script.txt>` (executa script em lote), `calc <expressao>` (avalia expressao aritmetica).
  - **Emulacao e Jogos**: `pokemon` (inicia Pokémon Red), `gameboy <rom.gb>` ou `gb <rom>` (emulador de Game Boy), `gbinfo <rom>` (exibe cabecalho do cartucho), `snake` (Jogo da Cobrinha), `matrix` (animacao visual).
  - **Diagnostico do Sistema**: `rtc` (diagnostico CMOS RTC), `meminfo` (estatisticas PMM e KHeap), `memtest` (teste de estresse do Heap), `uptime` (tempo de execucao e ticks PIT), `version` (versao do kernel), `about` (resumo arquitetural), `panic` (teste de Kernel Panic), `reboot` (reinicializacao).

---

### 10. Interface Grafica (GUI Mac OS System 7 640x480)

#### `include/gui/font8x8.h`
Tabela de fonte bitmap monocromatica 8x8 contendo o desenho dos 128 caracteres da tabela ASCII padrao com clamping de seguranca `0..127`.

#### `include/gui/gui.h` e `kernel/gui/gui.c`
Subsistema de interface grafica retro inspirada no classico **Apple Macintosh System 7 (1991)** rodando em **640x480 pixels a 32-bit True Color**:
- Aloca um backbuffer de 1.228.800 bytes (640x480x4) via `kmalloc` para desenho suave sem cintilacao de tela (double buffering).
- Renderiza o padrao de fundo classico 50% Gray Stipple dithered em cinza e branco.
- **Barra de Menus Superior Fixa com Relógio Real**: Altura de 20 pixels contendo o logotipo do migOS, menus suspensos interativos ("migOS", "File", "Edit", "View", "Special", "Help") com sombra projetada e **Relógio Real CMOS RTC** no canto superior direito (`HH:MM:SS`). Clicar sobre a área do relógio abre um widget suspenso exibindo a Data Real (`DD/MM/AAAA`), Hora Real e Tempo de Atividade (*Uptime*).
- **Gerenciador de Arquivos Visual ("migOS HD" 460x330 px)**:
  - Navegacao completa por subdiretorios com exibicao do caminho atual no titulo da janela.
  - Barra de ferramentas interativa com botoes `< Voltar`, `+ Pasta`, `+ Arq`, `Mover`, `Copiar` e `Excluir`.
  - Coluna de **Data/Hora de Modificação**: Exibe o carimbo real (`DD/MM HH:MM`) de cada arquivo e diretório gerado a partir do hardware RTC.
  - Caixas de dialogo modais com caixas de texto interativas para entrada de nomes de arquivos e pastas.
  - **Suporte a Duplo Clique**: Clique duplo em pastas navega para o subdiretorio (ou sobe com `..`), clique duplo em `.txt` abre no editor TextEdit e clique duplo em `.gb` inicia o emulador de Game Boy.
- **Janela de Perfil do Sistema ("System Profile" 460x340 px)**: Exibe a data/hora real lida do hardware CMOS RTC e o tempo de atividade decorrido do sistema.
- **Editor de Texto Grafico ("TextEdit")**: Janela com barra de ferramentas (`Novo`, `Abrir`, `Salvar`, `Executar`, `Limpar`), campo de nome do arquivo, indicador de alteracoes pendentes (`*`), cursor de texto e atalhos.
- **Icones Interativos do Desktop**: "migOS HD", "Pokemon", "Snake.app", "Terminal.app" e "Trash" com suporte a selecao e duplo-clique.
- **Botão Fechar Fiel ao Mac OS System 7 (Go-Away Box Animado)**: Caixa quadrada de 13x13 px com bisel duplo na barra de título. Ao clicar e segurar o mouse sobre o botão, exibe a animação clássica de quadrado rebaixado escuro (*depressed inset box*). Se o usuário arrastar o cursor para fora enquanto segura o clique, o botão volta ao estado normal; ao soltar o mouse dentro do botão, a janela é fechada imediatamente com confirmação visual.
- Cursor do mouse classico System 7 renderizado diretamente sobre o backbuffer com rastreamento confiavel via interrupcao IRQ12.

---

### 11. Jogos e Aplicacoes

#### `include/games/snake.h` e `kernel/games/snake.c`
Jogo da Cobrinha (Snake Game) integrado nativamente ao sistema operacional:
- Renderizacao em arena ampliada de 60 colunas por 24 linhas no terminal de alta resolucao.
- Controle da cobrinha por teclado utilizando as teclas `WASD` ou setas direcionais.
- Aceleracao progressiva da velocidade do jogo conforme o jogador acumula pontos.
- Frutas normais (vermelhas, 10 pontos) e frutas bonus temporizadas (douradas, 50 pontos).
- Registro de recorde de pontuacao (High Score) e tela de Game Over com retorno limpo ao shell.

---

### 12. Editor de Texto Visual (TextEdit & Nano/Edit)

#### `include/editor/editor.h` e `kernel/editor/editor.c`
Editor de texto interativo em tela cheia para o terminal de comandos e integrado a GUI:
- **Modo Terminal (`edit <arquivo>` / `nano <arquivo>`)**:
  - Interface visual com cabecalho informativo, area de edicao com numeracao de linhas e barra de atalhos inferior.
  - Navegacao completa por setas direcionais, edicao com insercao, quebra de linha (`Enter`) e remocao (`Backspace`).
  - Atalhos rapidos: `Ctrl+S` para salvar no disco/MIGFS e `Ctrl+Q` para sair com aviso de alteracoes nao salvas.
- **Modo Grafico (`TextEdit.app` na GUI)**:
  - Edicao em janela flutuante com suporte a mouse, selecao de arquivos e botao de execucao direta de scripts.

---

### 13. Interpretador e Executor de Scripts .txt

#### `include/interpreter/txt_interp.h` e `kernel/interpreter/txt_interp.c`
Motor de interpretacao e execucao em lote de scripts de texto:
- Executa arquivos de script `.txt` linha por linha sem necessidade de compilacao externa.
- **Comandos Nativos do Interpretador**:
  - `echo <mensagem>`: Imprime mensagem no terminal ou no log da GUI.
  - `set VAR=VALOR`: Define variavel de ambiente local.
  - Expansao de variaveis: Substitui `$VAR` pelo seu valor em tempo de execucao.
  - `calc <expressao>` / `eval <expressao>`: Avalia expressoes matematicas com o parser aritmetico.
  - `sleep <ms>`: Introduz pausas temporizadas em milissegundos.
  - `pause`: Aguarda pressionamento de tecla do usuario.
  - `color <fg> [bg]`: Altera as cores do terminal durante a execucao.
  - Execucao de qualquer comando interno do shell (`ls`, `touch`, `mkdir`, `cat`, `matrix`, etc.).
- **Calculadora Aritmetica Embutida com Parser Descendente Recursivo**:
  - Gramatica formal implementada em C com operadores de adicao (`+`), subtracao (`-`), multiplicacao (`*`), divisao (`/`) e parenteses aninhados (`()`) respeitando precedencia de operacoes.

---

### 14. Emulador de Game Boy & Jogo Pokémon (Peanut-GB Bare-Metal)

#### `include/emulator/peanut_gb.h`, `include/emulator/gameboy.h` e `kernel/emulator/gameboy.c`
Emulador completo de Nintendo Game Boy (DMG) portado para Ring 0 bare-metal sobre o hardware x86:
- **Core Peanut-GB Integrado**:
  - Emulacao precisa da CPU LR35902 (derivada do Z80), temporizacao de ciclos de clock, registradores e interrupcoes de V-Blank, LCD STAT, Timer, Serial e Joypad.
  - Unidade de Processamento Grafico (PPU/LCD): Emulacao das 144 linhas de varredura a 160x144 pixels, com renderizacao de background tiles, window overlay e sprites de 8x8 / 8x16 com paletas clássicas monocromaticas mapeadas em 32-bit True Color.
  - Emulacao de Memory Bank Controllers (MBC1, MBC3 com suporte a RTC e MBC5), permitindo a execucao de ROMs comerciais de ate varios megabytes.
- **Execucao Fluida de Pokémon Red Version**:
  - A ROM comercial `PokemonRed.gb` (1.048.576 bytes / 1 MB) e embutida no disco ATA e carregada diretamente para o espaco de memoria do kernel.
  - Renderizacao com taxa de quadros estavel a 60 FPS com upscaling 2x centralizado na tela de 640x480 pixels com moldura de cartucho retro.
- **Persistencia e Preservacao de Save Games (.sav)**:
  - A memoria SRAM do cartucho (Save Game) e automaticamente monitorada e gravada no disco rigido ATA (`PokemonRed.sav`), preservando o progresso da jornada do jogador entre reinicializacoes e recompilacoes.
- **Mapeamento de Controles do Game Boy**:
  - **D-Pad (Cima, Baixo, Esquerda, Direita)**: Teclas `W`, `S`, `A`, `D` ou Setas Direcionais.
  - **Botao A**: Teclas `J` ou `Z`.
  - **Botao B**: Teclas `K` ou `X`.
  - **START**: Tecla `Enter`.
  - **SELECT**: Tecla `Espaço` (Spacebar).
  - **Sair do Emulador**: Tecla `ESC` (retorna instantaneamente ao Shell ou Desktop).

---

## Emulador de Game Boy & Jogo Pokémon no migOS

O migOS incorpora um emulador completo de **Nintendo Game Boy (DMG)** baseado no core **Peanut-GB** portado diretamente para execucao em **Ring 0 Bare-Metal**, sem intermediarios, sem sistema hospedeiro e sem bibliotecas externas:

```mermaid
flowchart TD
    subgraph Emulador Game Boy Bare-Metal [kernel/emulator/gameboy.c]
        A[ROM PokemonRed.gb no Disco ATA / RAM] --> B[Peanut-GB Core]
        B --> C1[CPU LR35902 / Z80 Interpreter]
        B --> C2[Memory Bank Controller MBC1 / MBC3 / MBC5]
        B --> C3[PPU / LCD Scanline Renderer 160x144]
        B --> C4[SRAM Save Game Handler]
        
        C3 --> D[Framebuffer 160x144 2-bit DMG Palettes]
        D --> E[Scaler 2x & Moldura Centralizada no BGA 640x480 LFB]
        
        C4 --> F[Gravacao Automatica no Disco ATA LBA 1025+]
        F --> G[Arquivo PokemonRed.sav Persistente]
    end
    
    H[Teclado PS/2 IRQ1: WASD, J/Z, K/X, Enter, Space] --> C1
```

### Controles do Game Boy:

| Botao Original Game Boy | Tecla no migOS (Principal) | Tecla Alternativa | Funcao no Jogo |
| :--- | :--- | :--- | :--- |
| **D-Pad Up (Cima)** | `W` | `Seta para Cima` | Move o personagem para cima |
| **D-Pad Down (Baixo)** | `S` | `Seta para Baixo` | Move o personagem para baixo |
| **D-Pad Left (Esquerda)**| `A` | `Seta para a Esquerda` | Move o personagem para a esquerda |
| **D-Pad Right (Direita)**| `D` | `Seta para a Direita` | Move o personagem para a direita |
| **Botao A** | `J` | `Z` | Confirmar / Interagir / Conversar |
| **Botao B** | `K` | `X` | Cancelar / Correr / Voltar menu |
| **START** | `Enter` | `Enter` | Abre o menu principal / Pausa |
| **SELECT** | `Espaço` | `Espaço` | Seleciona item cadastrado / Reordena golpes |
| **Sair / Retornar** | `ESC` | `ESC` | Salva SRAM e retorna ao Terminal/Desktop |

### Persistencia de Saves de Pokémon:
Quando o jogador salva a partida dentro de Pokémon (pelo menu *SAVE* do jogo), os dados da memória SRAM de 32 KB do cartucho são imediatamente sincronizados e persistidos no disco ATA (`PokemonRed.sav`). Ao reiniciar a máquina virtual ou recompilar o migOS, o jogo continua exatamente de onde você parou (*CONTINUE*).

---

## Driver de Relógio em Tempo Real (CMOS / RTC) & Timestamps no MIGFS

O migOS integra um driver completo de **Relógio em Tempo Real (CMOS / RTC)** operando diretamente sobre o circuito integrado de temporização e bateria da placa-mãe (chip Motorola MC146818 ou compatível). Essa arquitetura substitui medições relativas de uptime por carimbos de tempo reais de alta precisão em todo o sistema operacional:

```mermaid
flowchart TD
    subgraph Hardware da Placa-Mãe [CMOS RTC - Bateria CR2032]
        P0["Porta 0x70: Registrador de Endereço / Índice"]
        P1["Porta 0x71: Registrador de Dados"]
        Regs["Registradores 0x00-0x09: Segundos, Minutos, Horas, Dia, Mês, Ano"]
        StatA["Reg 0x0A (Status A): Bit 7 UIP (Update In Progress)"]
        StatB["Reg 0x0B (Status B): Bit 2 (BCD/Binário), Bit 1 (24h/12h)"]
        Cent["Reg 0x32: Século (20xx)"]
    end

    subgraph Driver CMOS RTC [kernel/drivers/rtc/rtc.c]
        UIP["Sincronização UIP: Aguarda término do ciclo de atualização"]
        BCD["Decodificação BCD: ((v >> 4) * 10) + (v & 0x0F)"]
        Conv["Ajuste de Formato: 12h AM/PM para 24h & Século 2000+"]
        Epoch["Conversor Gregoriano: rtc_time_t <-> Timestamp UNIX Epoch"]
        Cache["Cache Rápido em RAM: Atualização sem bloqueio de I/O a 60 FPS"]
    end

    subgraph Subsistemas Integrados no migOS
        GUI["GUI Desktop System 7: Relógio HH:MM:SS & Widget Suspenso"]
        FS["MIGFS: Metadados created_time e modified_time no Disco ATA"]
        CLI["Shell Terminal: date, time, rtc, clock e ls com Timestamps"]
        LibC["LibC Freestanding: time(), localtime(), gmtime(), ctime()"]
    end

    P0 --> UIP
    P1 --> UIP
    StatA --> UIP
    UIP --> BCD
    StatB --> BCD
    Regs --> BCD
    Cent --> Conv
    BCD --> Conv
    Conv --> Epoch
    Epoch --> Cache
    Cache --> GUI
    Cache --> FS
    Cache --> CLI
    Cache --> LibC
```

### 1. Comunicação com o Hardware CMOS e Decodificação BCD
- **Portas de E/S**: A CPU acessa a memória CMOS enviando o índice do registrador para a porta `0x70` e lendo/escrevendo o byte correspondente na porta `0x71`.
- **Prevenção de Condições de Corrida (*Race Conditions*)**: O driver monitora o bit 7 do *Status Register A* (`RTC_STATUS_A_UIP`). Quando o chip está atualizando os registradores (frequência de 1 Hz), o driver aguarda o ciclo terminar e realiza uma **dupla leitura confirmatória**, garantindo que nenhum segundo seja lido no meio da transição.
- **Decodificação BCD Dinâmica**: Em placas-mãe PC/AT padrão e emuladores (QEMU/Bochs/VirtualBox), os números são comumente gravados em formato *Binary-Coded Decimal* (por exemplo, `0x26` representando o número 26). O driver inspeciona o bit 2 do *Status Register B* (`0x04` = Binário Puro, `0x00` = BCD) e converte automaticamente os valores utilizando a fórmula:
  $$\text{Valor} = \left(\frac{\text{BCD}}{16} \times 10\right) + (\text{BCD} \pmod{16})$$
- **Formato de 24 Horas & Século**: Converte horas em modo 12h (com flag de PM no bit 7) para o formato militar de 24 horas e calcula o século atual através do registrador `0x32` ou inferência da virada do milênio.

### 2. Timestamps Reais no Sistema de Arquivos (MIGFS)
- Os descritores de disco de 64 bytes (`migfs_disk_entry_t`) e descritores em memória (`migfs_file_t`) armazenam dois carimbos de tempo em formato **Unix Epoch Timestamp** (segundos desde 1970):
  - `created_time`: Data e hora exata da criação do arquivo ou pasta.
  - `modified_time`: Data e hora da última operação de escrita (`write`), concatenação (`append`), cópia (`cp`), movimentação (`mv`) ou toque (`touch`).
- **Visualização em Colunas**: O comando `ls` e a listagem da janela `migOS HD` exibem o carimbo formatado no padrão `DD/MM HH:MM` para rápida inspeção visual.

### 3. Relógio e Widget Interativo na GUI Mac OS System 7
- **Barra Superior**: O canto superior direito exibe o relógio digital (`HH:MM:SS`) sincronizado continuamente com o RTC da placa-mãe.
- **Menu Dropdown / Widget do Relógio**: Ao clicar sobre o relógio na barra superior, um menu suspenso elegante exibe:
  - **Data Real**: `DD/MM/AAAA`
  - **Hora Real**: `HH:MM:SS`
  - **Tempo Ativo (*Uptime*)**: `X min Y s (Z ticks PIT @ 100Hz)`

---

## Driver de Áudio para PC Speaker (PIT 8254) & Efeitos Sonoros (SFX)

O migOS implementa um driver completo de síntese de áudio bare-metal através do **Canal 2 do PIT 8254** e da **Porta de Controle B (`0x61`)**, gerando ondas quadradas harmônicas diretamente no transdutor piezoelétrico ou cone do PC Speaker da placa-mãe sem necessidade de drivers de terceiros:

```mermaid
flowchart TD
    subgraph Hardware de Temporização e Som [x86 Architecture]
        PIT_CMD["Porta 0x43: Byte de Comando 0xB6<br/>(Canal 2, LSB/MSB, Modo 3 Square Wave)"]
        PIT_DAT["Porta 0x42: Divisor de 16 bits<br/>(1193180 / freq_hz)"]
        PORT_61["Porta 0x61: System Control Port B<br/>Bit 0 (PIT2 Gate) | Bit 1 (Speaker Data)"]
        SPEAKER["Cone do Alto-Falante / PC Speaker"]
    end

    subgraph Driver de Áudio migOS [kernel/drivers/sound/sound.c]
        TONE["sound_tone(freq): Calcula divisor e ativa Porta 0x61"]
        STOP["sound_stop(): Limpa bits 0 e 1 da Porta 0x61"]
        BEEP["sound_beep(freq, ms): Tom bloqueante com sleep(ms)"]
        SFX["sound_play_sfx(tipo): Tabela de melodias predefinidas"]
        MUTE["sound_set_mute(1/0): Flag global de silenciamento"]
    end

    subgraph Efeitos Sonoros Integrados [SFX]
        S1["SFX_STARTUP: Mac OS Classic Startup Chime (Fá Maior)"]
        S2["SFX_ALERT: Beep de Alerta da GUI / Sosumi"]
        S3["SFX_CLICK: Clique de Interface de Usuário (2500 Hz)"]
        S4["SFX_TRASH: Esvaziamento de Lixeira / Ação Destrutiva"]
        S5["SFX_SUCCESS: Confirmação de Sucesso / Salvar no Disco"]
    end

    TONE --> PIT_CMD
    TONE --> PIT_DAT
    TONE --> PORT_61
    STOP --> PORT_61
    PORT_61 --> SPEAKER

    SFX --> S1
    SFX --> S2
    SFX --> S3
    SFX --> S4
    SFX --> S5
```

### 1. Mecanismo de Funcionamento e Cálculo de Frequência
- **Geração de Onda Quadrada**: O Canal 2 do PIT opera como divisor de frequência do clock base de **1.193.180 Hz**.
- **Fórmula de Frequência**:
  $$\text{Divisor} = \frac{1.193.180}{\text{Frequência (Hz)}}$$
- **Controle dos Bits da Porta 0x61**:
  - **Ligar o Som**: `outb(0x61, inb(0x61) | 0x03)` (habilita o sinal de clock do PIT2 e conecta a saída ao speaker).
  - **Silenciar o Som**: `outb(0x61, inb(0x61) & ~0x03)` (desconecta a saída e desativa a oscilação).

### 2. Tabela de Efeitos Sonoros do migOS (SFX):

| Efeito Sonoro | Identificador | Sequência de Notas / Frequências | Aplicação no Sistema |
| :--- | :--- | :--- | :--- |
| **Startup Chime** | `SFX_STARTUP` | F3 (175Hz, 25ms) $\rightarrow$ C4 (262Hz, 25ms) $\rightarrow$ F4 (349Hz, 30ms) $\rightarrow$ A4 (440Hz, 40ms) $\rightarrow$ C5 (523Hz, 220ms) | Inicialização do kernel / Boot |
| **Alerta Sosumi** | `SFX_ALERT` | A5 (880Hz, 30ms) $\rightarrow$ Pausa (10ms) $\rightarrow$ F5 (698Hz, 60ms) | Diálogos modais e avisos na GUI |
| **Clique de Interface** | `SFX_CLICK` | Pulso ultra-curto de 2500 Hz (2ms) | Cliques de botões, abas e menus |
| **Trash / Exclusão** | `SFX_TRASH` | Varredura descendente: 800Hz (15ms) $\rightarrow$ 500Hz (15ms) $\rightarrow$ 300Hz (25ms) $\rightarrow$ 150Hz (40ms) | Exclusão de arquivos e diretórios |
| **Sucesso / Salvar** | `SFX_SUCCESS` | G5 (784Hz, 40ms) $\rightarrow$ C6 (1047Hz, 120ms) | Gravação de arquivos e criação de pastas |

---

## Sistema de Arquivos Hierárquico MIGFS & Persistência em Disco

O **MIGFS** gerencia a hierarquia de pastas e arquivos em memória RAM com sincronização bidirecional no **Disco Rígido ATA/IDE**:

```mermaid
flowchart LR
    subgraph Layout do Disco ATA Primario [16 MB / 32.768 Setores]
        MBR[LBA 0: MBR Bootloader 512B]
        Kernel[LBA 1 - 1024: Binario migOS Kernel 512KB]
        Super[LBA 1025: Superbloco MIGFS Magic 0x4D494746]
        Descs[LBA 1026 - 1033: Tabela de 64 Descritores de Arquivos/Pastas]
        Data[LBA 1034+: Blocos de Dados Contiguos de Arquivos e ROMs]
    end

    MBR --> Kernel
    Kernel --> Super
    Super --> Descs
    Descs --> Data
```

### Operacoes e Suporte a Pastas:
- **CLI**: Comandos `mkdir <pasta>`, `rmdir <pasta>`, `cd <caminho>`, `pwd`, `mv <orig> <dest>`, `cp <orig> <dest>` e `ls` hierárquico com visualização de pastas (`<DIR>`) e arquivos (`<FILE>`).
- **GUI Desktop**: Janela `migOS HD` com barra de ferramentas, criação de pastas e arquivos via caixas de diálogo modais, e **duplo clique** para navegação profunda e subida com `..`.
- **Persistencia**: O comando `sync` (ou o botão Salvar do editor) grava instantaneamente todo o sistema de arquivos para os setores ATA correspondentes.

---

## Interpretador de Scripts & Calculadora Matemática

O migOS inclui um interpretador em tempo de execução para scripts de lote `.txt` e avaliação de expressões aritméticas:

```mermaid
flowchart TD
    subgraph Interpretador e Calculadora [kernel/interpreter/txt_interp.c]
        Input["Expressao: (10 + 5) * 2 - 8 / 4"] --> Lexer[Analise Lexica / Tokens]
        Lexer --> Parser[Parser Descendente Recursivo]
        
        Parser --> E[Expressao: + / -]
        E --> T[Termo: * / /]
        T --> F[Fator: Numeros / Variaveis $VAR / Parenteses]
        
        F --> Result["Resultado: 28"]
    end
```

### Recursos de Scripting:
- Variáveis locais com `set VAR=VAL` e interpolação `$VAR`.
- Comandos de controle `echo`, `sleep <ms>`, `pause`, `color <cor>` e execução em lote.
- Avaliação matemática instantânea no shell via `calc <expressao>` ou `eval <expressao>`.

---

## Terminal Avançado, Scrollbar & Ergonomia Linux

O migOS implementa uma arquitetura de terminal profissional inspirada no **Windows Terminal, PowerShell e terminais Linux modernos**, combinando um **Buffer Unificado Contínuo de 500 linhas** com uma barra lateral de rolagem (*Scrollbar*) dinâmica renderizada em tempo real:

```mermaid
flowchart LR
    subgraph Buffer Unificado de 500 Linhas [term_buffer: 500 x 80 Cells]
        direction TB
        B0["Linha 0: Topo do Histórico (Saídas antigas)"]
        B1["... Linhas de Comandos, help, meminfo, scripts ..."]
        B2["Linha N-30: Início da Viewport Visível"]
        B3["Linhas N-29 a N: Tela Visível Atual (30 Linhas)"]
        B4["Linha N: Prompt Ativo migOS:/docs> _"]
    end

    subgraph Viewport da Tela 640x480 [80x30 Caracteres]
        V1["Colunas 0 a 78: Texto, Cores, Acentos e Cursor"]
        V2["Coluna 79: Barra Lateral de Rolagem (Scrollbar)"]
    end

    B3 -->|Desliza Suavemente (term_view_row)| V1
```

### 1. Barra Lateral de Rolagem (*Scrollbar Visual*)
- **Localização**: Coluna 79 (borda direita da tela).
- **Componentes**:
  - `▲` (Seta Superior): Indica o topo do histórico.
  - `▼` (Seta Inferior): Indica a base da tela ao vivo.
  - **Trilha Vertical (`|`)**: Canal contínuo de rolagem.
  - **Bloco Deslizante (*Thumb `█`*)**: Bloco sólido em ciano dimensionado e posicionado proporcionalmente ao volume de texto gerado na sessão.
- **Indicador de Rolagem no Topo (*Overlay Badge*)**: Exibe `[^ +N lin]` no canto superior direito quando o usuário está navegando no histórico. Ao digitar qualquer tecla, o terminal retorna automaticamente para a linha de comando ativa.

### 2. Controles de Navegação e Rolagem:

| Meio de Entrada | Ação / Atalho | Comportamento no Terminal |
| :--- | :--- | :--- |
| **Mouse** | `Roda para Cima (Scroll Wheel Up)` | Sobe a tela suavemente no histórico de saída |
| **Mouse** | `Roda para Baixo (Scroll Wheel Down)` | Desce a tela em direção ao prompt ao vivo |
| **Mouse** | `Clique na Seta Superior (▲)` | Rola 2 linhas para cima |
| **Mouse** | `Clique na Seta Inferior (▼)` | Rola 2 linhas para baixo |
| **Teclado** | `Page Up` | Rola 10 linhas para cima |
| **Teclado** | `Page Down` | Rola 10 linhas para baixo |
| **Teclado** | `Shift + Seta para Cima (↑)` | Rola suavemente 1 linha para cima |
| **Teclado** | `Shift + Seta para Baixo (↓)` | Rola suavemente 1 linha para baixo |
| **Teclado** | `Shift + Page Up` / `Ctrl + Home` | Salta imediatamente para o topo absoluto do histórico |
| **Teclado** | `Shift + Page Down` / `Ctrl + End` | Retorna imediatamente para o prompt ao vivo |

### 3. Atalhos de Edição de Linha (Padrão POSIX / Bash / Zsh):

| Atalho | Função | Descrição |
| :--- | :--- | :--- |
| `←` / `→` | Mover Cursor | Move o cursor livremente para a esquerda ou direita na linha |
| `Home` / `Ctrl+A` | Início da Linha | Posiciona o cursor no primeiro caractere do comando |
| `End` / `Ctrl+E` | Fim da Linha | Posiciona o cursor após o último caractere digitado |
| `Delete` | Deletar Caractere | Apaga o caractere sob o cursor |
| `Backspace` | Apagar Anterior | Apaga o caractere à esquerda do cursor e desloca o texto |
| `Tab` | Auto-Completar | Completa comandos, scripts, arquivos e pastas automaticamente |
| `Ctrl + C` | Cancelar Linha | Aborta a linha digitada, imprime `^C` e gera um novo prompt |
| `Ctrl + L` | Limpar Tela | Limpa o terminal e redessina o prompt preservando o comando atual |
| `Ctrl + U` | Cortar para o Início | Apaga todo o conteúdo desde o cursor até o início da linha |
| `Ctrl + K` | Cortar para o Fim | Apaga todo o conteúdo desde o cursor até o final da linha |
| `Ctrl + W` | Apagar Palavra | Apaga a palavra anterior ao cursor |
| `↑` / `↓` | Histórico | Navega entre os comandos anteriores executados na sessão |

### 4. Histórico de Comandos e Reexecução Rápida:
- O shell mantém um buffer com os últimos 32 comandos executados.
- `history`: Lista todos os comandos numerados.
- `!n`: Executa o comando de número `n` (ex: `!2` reexecuta o comando 2).
- `!prefixo`: Executa o comando mais recente iniciado com aquele prefixo (ex: `!pok` executa `pokemon`).

---

## Ciclo de Inicializacao (Boot Flow)

```mermaid
flowchart TD
    A[Power On / BIOS Real Mode 16-bit] --> B[POST & Leitura do Setor 0 em 0x7C00]
    B --> C[boot.asm: Inicializa Segmentos e Pilha]
    C --> D[Carrega 1024 Setores do Kernel via LBA INT 13h em 0x10000]
    D --> E[Habilita Linha A20 via Porta 0x92 Fast Gate]
    E --> F[Carrega GDT Plana de 4GB para Ring 0]
    F --> G[Ativa bit PE em CR0 e Salto Longo para Modo Protegido 32-bit]
    G --> H[kernel_entry.asm: Configura Pilha em 0x1FFFF0 e Zera BSS]
    H --> I[kmain em kernel.c: Inicializacao Ordenada do Kernel]
    
    subgraph Inicializacao do Kernel [kmain]
        I --> J1[vga_init: BGA 640x480 32-bit e Terminal 80x30]
        J1 --> J2[idt_init: IDT 256 Vetores e LIDT]
        J2 --> J3[pic_init: Remapeamento PIC 8259A IRQs 32-47]
        J3 --> J4[timer_init: PIT 8254 100Hz e STI]
        J4 --> J5[pmm_init: Bitmap de Memoria Fisica 64MB]
        J5 --> J6[kheap_init: Heap Dinamico 8MB em 0x200000]
        J6 --> J7[migfs_init: Montagem e Carga do Disco ATA MIGFS]
        J7 --> J8[keyboard_init: Driver de Teclado PS/2]
        J8 --> J9[mouse_init: Driver PS/2 3-Bytes com IRQ 12]
        J9 --> J10[shell_run: Shell Interativo ou Desktop GUI]
    end
```

---

## Mapa de Memoria Fisica

```mermaid
classDiagram
    class EspacoDeMemoria {
        0x00000000 - 0x000003FF : IVT (Interrupt Vector Table BIOS)
        0x00000400 - 0x000004FF : BDA (BIOS Data Area)
        0x00007C00 - 0x00007DFF : MBR Bootloader (512 Bytes)
        0x00010000 - 0x0008FFFF : Kernel migOS (.text, .rodata, .data, .bss)
        0x00090000 - 0x0009FFFF : Pilha do Kernel (Stack Pointer ESP)
        0x000A0000 - 0x000BFFFF : Video Legado VGA (Memoria 0xA0000 / 0xB8000)
        0x00100000 - 0x00101FFF : PMM Frame Bitmap (Gerencia 64MB em 4KB)
        0x00200000 - 0x009FFFFF : Kernel Dynamic Heap (KHeap 8MB)
        0x00A00000 - 0x03FFFFFF : Memoria RAM Livre para Alocacoes (64MB)
        0xFD000000 - 0xFD12C000 : PCI BAR0 Linear Framebuffer (LFB BGA 640x480x4)
    }
```

```
+-------------------------------------------------------------------------+
|                  ORGANIZACAO DA MEMORIA FISICA (4 GB)                   |
+---------------------------+ 0x00000000                                  |
| Vetores de Interrupcao    | (IVT Modo Real da BIOS)                     |
+---------------------------+ 0x00000400                                  |
| Area de Dados da BIOS     | (BDA)                                       |
+---------------------------+ 0x00007C00                                  |
| Codigo do Bootloader MBR  | (512 bytes finalizados com 0xAA55)          |
+---------------------------+ 0x00007E00                                  |
| Memoria Livre Convencional|                                             |
+---------------------------+ 0x00010000 (64 KB)                          |
| Codigo do Kernel migOS    | (.text, .rodata, .data, .bss)               |
+---------------------------+ 0x00090000                                  |
| Pilha do Kernel (Stack)   | (Cresce para baixo em direcao a 0x10000)    |
+---------------------------+ 0x000A0000                                  |
| Memoria de Video VGA      | (Buffer Grafico Legado / Fontes Plane 2)    |
+---------------------------+ 0x000B8000                                  |
| Memoria de Texto VGA      | (Modo Texto Legado 80x25)                   |
+---------------------------+ 0x00100000 (1 MB)                           |
| Bitmap de Frames do PMM   | (Gerencia 16.384 frames de 4 KB de RAM)     |
+---------------------------+ 0x00200000 (2 MB)                           |
| Kernel Dynamic Heap       | (Alocador Dinamico KHeap de 8 MB)           |
+---------------------------+ 0x00A00000 (10 MB)                          |
| RAM Livre para Alocacoes  | (Frames livres gerenciados pelo PMM ate 64M)|
+---------------------------+ 0x04000000 (64 MB)                          |
| ...                       |                                             |
+---------------------------+ 0xFD000000 (PCI BAR0)                       |
| Linear Framebuffer (LFB)  | (Memoria de video BGA 640x480 a 32-bit True)|
+-------------------------------------------------------------------------+
```

---

## Arquitetura do Heap Dinamico (KHeap)

O alocador de memoria dinamica do Kernel opera sobre uma lista duplamente encadeada de blocos com cabecalhos protegidos:

```mermaid
flowchart LR
    subgraph Bloco 1 [Bloco Alocado]
        H1["Cabecalho kheap_block_t<br/>Magic: 0x1A2B3C4D<br/>is_free: 0<br/>size: 1024 bytes"]
        D1["Dados Uteis (Payload)"]
    end
    
    subgraph Bloco 2 [Bloco Livre]
        H2["Cabecalho kheap_block_t<br/>Magic: 0x1A2B3C4D<br/>is_free: 1<br/>size: 4096 bytes"]
        D2["Espaco Disponivel"]
    end
    
    subgraph Bloco 3 [Bloco Alocado]
        H3["Cabecalho kheap_block_t<br/>Magic: 0x1A2B3C4D<br/>is_free: 0<br/>size: 512 bytes"]
        D3["Dados Uteis (Payload)"]
    end

    H1 -->|next| H2
    H2 -->|prev| H1
    H2 -->|next| H3
    H3 -->|prev| H2
```

### Mecanismos de Otimizacao do Heap:
1. **First-Fit Allocation**: Localiza o primeiro bloco livre com tamanho suficiente.
2. **Block Splitting**: Se o bloco livre for maior que o tamanho requisitado mais o cabecalho, divide-o automaticamente, alocando a parte requisitada e mantendo a sobra na lista como um novo bloco livre.
3. **Immediate Coalescing**: Ao executar `kfree()`, se o bloco anterior ou o proximo bloco tambem estiverem livres, funde-os instantaneamente em um unico bloco continuo, eliminando a fragmentacao externa.

---

## Pipeline Grafico e Double Buffering

Para garantir atualizacoes visuais instantaneas sem cintilacao de tela (flicker-free), a interface grafica e o terminal utilizam renderizacao em memoria RAM antes do envio ao hardware:

```mermaid
flowchart TD
    subgraph Camadas de Renderizacao da GUI
        L1[Fundo 50% Gray Stipple Dither] --> L2[Camada de Janelas: Pinstripes, Bordas e Textos]
        L2 --> L3[Icones do Desktop: migOS HD, Pokemon, Snake, Terminal, Trash]
        L3 --> L4[Barra de Menus Fixa e Menus Suspensos]
        L4 --> L5[Sprite do Cursor do Mouse 12x18]
    end

    L5 --> B[Backbuffer em Memoria RAM<br/>640 x 480 x 4 Bytes = 1.228.800 Bytes]
    B -->|bga_blit via memcpy| C[Linear Framebuffer LFB de Hardware<br/>PCI BAR0 0xFD000000]
    C --> D[Display de Video 640x480 32-bit True Color 60 FPS]
```

---

## Fluxo de Interrupcoes e Drivers de Entrada

```mermaid
sequenceDiagram
    autonumber
    actor Hardware as Teclado / Mouse / Timer
    participant PIC as Controlador PIC 8259A
    participant CPU as Processador x86
    participant IDT as Tabela IDT (256 Vetores)
    participant Stub as isr_asm.asm (pusha / iret)
    participant Driver as Tratador C (Driver)
    participant App as Shell / GUI / Jogo

    Hardware->>PIC: Sinal de Linha IRQ (0, 1 ou 12)
    PIC->>CPU: Linha INTR (Vetor Remapeado 32, 33 ou 44)
    CPU->>IDT: Consulta Portao de Interrupcao
    IDT->>Stub: Salto para irq0 / irq1 / irq12
    Stub->>Stub: pusha (Salva registradores na Pilha)
    Stub->>Driver: Invoca tratador C correspondente
    Driver->>PIC: Envia comando EOI (End of Interrupt)
    Driver->>App: Atualiza Buffer / Fila de Eventos / Coordenadas
    Stub->>Stub: popa (Restaura registradores)
    Stub->>CPU: Instrucao iret (Retorna execucao)
```

---

## Comandos do Terminal

| Comando | Sintaxe | Descricao |
| :--- | :--- | :--- |
| `help` | `help [cmd]` | Exibe a lista de comandos categorizada ou ajuda detalhada de um comando |
| `clear` / `cls` | `clear` | Limpa a tela do terminal e reposiciona a janela de visualizacao no topo |
| `history` | `history` | Lista todos os comandos executados na sessao com numeracao ordinal |
| `!n` / `!str` | `!n` ou `!str` | Reexecuta o comando de numero `n` ou o mais recente iniciado por `str` |
| `pwd` | `pwd` | Imprime o caminho do diretorio de trabalho corrente |
| `cd` | `cd [pasta]` | Troca de diretorio (`cd docs`, `cd ..`, `cd /`) |
| `mkdir` / `md` | `mkdir <pasta>` | Cria um novo subdiretorio no sistema de arquivos |
| `rmdir` / `rd` | `rmdir <pasta>` | Remove um diretorio vazio |
| `mv` | `mv <orig> <dest>` | Move ou renomeia um arquivo ou pasta |
| `cp` | `cp <orig> <dest>` | Copia um arquivo ou pasta para um novo destino |
| `ls` / `dir` | `ls [dir] [-a]` | Lista arquivos e subdiretórios com indicacao de `<DIR>`, `<FILE>`, tamanho, carimbo de modificacao e atributos |
| `cat` / `type` | `cat <arquivo>` | Exibe o conteudo de texto do arquivo especificado |
| `touch` | `touch <arquivo>` | Cria um novo arquivo vazio ou atualiza o carimbo de data/hora de modificacao |
| `write` | `write <arq> <txt>` | Escreve ou concatena uma cadeia de texto dentro de um arquivo |
| `rm` / `del` / `unlink` | `rm <arquivo>` | Exclui um arquivo gravavel do sistema de arquivos |
| `echo` | `echo <texto>` | Imprime uma mensagem de texto no terminal |
| `free` | `free` | Exibe o diagnostico e ocupacao de memoria RAM fisica (PMM) e dinamica (KHeap) |
| `df` | `df` | Exibe o espaco total, alocado e livre no disco rigido ATA / MIGFS |
| `date` / `time` | `date` | Exibe a data e hora reais do CMOS RTC, tempo decorrido de atividade (uptime) e Unix epoch |
| `rtc` / `clock` / `cmos` | `rtc` | Diagnostico tecnico do controlador CMOS/RTC, decodificacao BCD e status da bateria |
| `beep` | `beep [freq] [dur]` | Emite um tom sonoro de frequencia (Hz) e duracao (ms) no PC Speaker (padrao: 440Hz 100ms) |
| `sfx` | `sfx <nome>` | Toca efeito sonoro do sistema (`startup`, `alert`, `click`, `trash`, `success`) |
| `mute` / `sound` | `mute [on/off]` | Silencia ou reativa a emissao sonora do PC Speaker |
| `edit` / `nano` / `vi` / `vim` | `edit <arquivo>` | Abre o Editor de Texto Visual em tela cheia com atalhos `Ctrl+S` e `Ctrl+Q` |
| `run` / `exec` | `run <script.txt>` | Executa um script em lote `.txt` com variaveis e comandos |
| `calc` / `eval` | `calc <expressao>` | Avalia uma expressao matematica com parenteses e precedencia de operadores |
| `pokemon` | `pokemon` | Inicia imediatamente o jogo **Pokémon Red Version** no emulador Game Boy |
| `gameboy` / `gb` | `gameboy <rom.gb>` | Executa uma ROM de Nintendo Game Boy no emulador Peanut-GB |
| `gbinfo` | `gbinfo <rom.gb>` | Exibe o cabecalho tecnico e metadados de um cartucho Game Boy |
| `sync` | `sync` | Sincroniza e grava todas as alteracoes do sistema de arquivos no disco rigido ATA |
| `meminfo` | `meminfo` | Exibe o diagnostico detalhado de memoria fisica (PMM) e memoria heap (KHeap) |
| `memtest` | `memtest` | Executa bateria de testes de alocacao dinamica (`kmalloc` e `kfree`) |
| `uptime` | `uptime` | Exibe o tempo de atividade do sistema em segundos e ticks do PIT |
| `matrix` | `matrix` | Inicia o efeito visual da chuva de codigos verdes do filme Matrix |
| `snake` | `snake` | Executa o Jogo da Cobrinha (Snake Game) integrado |
| `gui` / `desktop`| `gui` | Inicia a Interface Grafica Mac OS System 7 Classic em 640x480 |
| `version` | `version` | Exibe a versao atual e arquitetura do kernel |
| `about` | `about` | Informacoes sobre os objetivos academicos do projeto migOS |
| `panic` | `panic` | Dispara intencionalmente um Kernel Panic de teste (Divisao por Zero / INT 0) |
| `reboot` | `reboot` | Reinicia a maquina virtual atraves do controlador PS/2 ou Triple Fault |

---

## Requisitos e Compilacao

### Ferramentas Necessarias
- **Windows PowerShell 5.1+**
- **GCC (GNU Compiler Collection)** com suporte a geracao de codigo i686/x86 de 32 bits (disponivel via MinGW-w64, MSYS2 ou GCC nativo).
- **GNU Binutils** (`ld` e `objcopy`).
- **NASM (Netwide Assembler)** (incluso no projeto como `nasm.exe`).
- **QEMU Emulator** (`qemu-system-x86_64` ou `qemu-system-i386`).

### Como Compilar e Executar

1. Abra o terminal do PowerShell na pasta raiz do projeto.
2. Execute o script de automacao para compilar e iniciar o migOS no QEMU:
   ```powershell
   .\build.ps1
   ```
3. Para compilar sem iniciar o emulador:
   ```powershell
   .\build.ps1 -NoRun
   ```

> [!TIP]
> **Áudio no QEMU**: O script `build.ps1` já inclui automaticamente os parâmetros `-audiodev dsound,id=snd0 -machine pcspk-audiodev=snd0` para conectar o PC Speaker virtual do migOS ao driver DirectSound do Windows. Caso execute o QEMU manualmente, lembre-se de passar esses parâmetros para habilitar o som.

---

## Autor

Desenvolvido por **Miguel Pereira** como projeto pratico da disciplina de **Sistemas Operacionais (2026)**, explorando os conceitos fundamentais de construcao de sistemas operacionais monoliticos, programacao bare-metal em C e Assembly x86, gerenciamento de memoria, drivers de dispositivos e interfaces graficas.
