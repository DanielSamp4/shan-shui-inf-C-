# Shan Shui Infinito — ESP32-S3

Primeira base do port para a Waveshare ESP32-S3 4.3-inch Display Development
Board, 800×480 RGB565, com módulo N16R8 (16 MB flash / 8 MB PSRAM).

## Estado desta entrega

O projeto não tenta portar Direct3D nem o rasterizador desktop inteiro. Ele
inicializa um painel RGB paralelo, reserva quatro tiles RGB565 em PSRAM e
compõe uma faixa procedural contínua no framebuffer. Isto valida a arquitetura
de memória e o scroll antes de trazer `Planner`, montanhas, hachuras e objetos.

```text
seed / mundo              tiles RGB565 em PSRAM              LCD RGB
procedural      ->   [atrás][visível][visível][futuro]  ->  framebuffer
```

O `main/tile_cache.c` já recicla tiles à frente da câmera. No momento ele usa
uma prévia procedural leve (papel, montanhas de teste e linhas de hachura), e
não a gramática completa de `cpp/src/scenery.cpp`.

## Pré-requisitos

- ESP-IDF 5.3 ou superior;
- target `esp32s3`;
- placa Waveshare compatível com o mapeamento RGB em `main/board_config.h`.

```powershell
cd esp32s3
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

## Atenção à revisão da placa

O mapeamento em `main/board_config.h` foi preparado para a revisão Waveshare
4.3C/N16R8 documentada pela fabricante: RGB565 paralelo, PCLK GPIO7, VSYNC
GPIO3, HSYNC GPIO46 e DE GPIO5. A versão 4.3, 4.3B ou outra revisão deve ser
conferida antes de gravar firmware: não copie timings/pinos entre revisões sem
comparar o esquema oficial.

O backlight dessa revisão é controlado por expansor I²C, não por GPIO direto.
Esta primeira entrega deixa sua ativação como adaptação explícita em
`board_backlight_enable()`. A transmissão RGB e a arquitetura de renderização
podem ser validadas independentemente; para imagem visível no painel, portar o
driver do expansor Waveshare é o próximo passo de bring-up.

## Próximos passos técnicos

1. Validar pinos, timing e backlight na placa física.
2. Portar `prng`, `noise` e `planner` do desktop para C/C++ ESP-IDF.
3. Trocar a prévia em `tile_cache.c` por `PlanItem` reais.
4. Adicionar rasterizador de polígono RGB565/máscara de tinta por tile.
5. Executar planejamento e rasterização em tarefa FreeRTOS separada.
