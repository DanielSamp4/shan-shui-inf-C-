# {Shan, Shui}* — fork para paisagem procedural embarcada

Este projeto é um fork de [LingDong-/shan-shui-inf](https://github.com/LingDong-/shan-shui-inf): uma paisagem chinesa procedural, inspirada em pergaminhos *shan shui*, formada por ruído, geometria e desenho vetorial.

O objetivo deste fork é levar essa experiência para uma **ESP32-S3 4.3-inch Display Development Board**. O dispositivo deverá gerar e exibir uma paisagem contínua e aparentemente infinita, sem revelar a construção dos objetos durante o deslocamento.

## Caderno interativo

O primeiro capítulo do blog está em [blog/](blog/README.md). Ele usa Canvas
para explicar, com controles de semente e camadas, como uma montanha procedural
é composta a partir de uma receita determinística.

## Estado do projeto

A etapa desktop em C++ está concluída como referência de implementação e fidelidade visual. Ela inclui geração procedural incremental, pré-carregamento de horizonte, rasterização fora da thread de apresentação, scroll subpixel com VSync e composição de tinta em modo multiply.

A próxima etapa é analisar essa referência e arquitetar a versão para o sistema embarcado. A intenção não é continuar alterando o código C++ de desktop, salvo correções explicitamente necessárias.

A implementação inicial dessa nova fase está em [esp32s3/](esp32s3/README.md): um projeto ESP-IDF para a Waveshare 4.3-inch 800×480 que estabelece framebuffer RGB565 em PSRAM e cache circular de tiles antes do port da gramática completa de montanhas.

## Milestones

- [x] Fork do projeto original e entendimento da gramática visual SVG.
- [x] Evolução de `index.html` para uso como aplicação contínua e incremental.
- [x] Port da geração procedural para C++ com checks determinísticos.
- [x] Renderizador desktop com Direct3D 11, VSync e composição multiply.
- [x] Eliminação de stutters por rasterização na UI e pré-carregamento de cenário.
- [x] Aproximação do peso de traço e hachura do SVG.
- [ ] Análise de memória, tela, biblioteca gráfica e tarefas no ESP32-S3.
- [ ] Arquitetura de tiles/cache circular e geração assíncrona no dispositivo.
- [ ] Port e otimização para a placa embarcada.

## Documentação do processo

Leia [o diário técnico do desenvolvimento](docs/processo-de-desenvolvimento.md) para a cronologia, decisões de arquitetura, tentativas descartadas, diagnóstico de desempenho e plano para a fase ESP32-S3.

## Projeto original

Generate your own on https://lingdong-.github.io/shan-shui-inf/ (or [Alternative link](https://shan-shui-inf.glitch.me)).

Some examples:
![Screenshot1](/screenshots/screen001.jpg?raw=true "")
![Screenshot2](/screenshots/screen002.jpg?raw=true "")

{Shan, Shui}\* is inspired by [traditional Chinese landscape scrolls](https://en.wikipedia.org/wiki/Shan_shui) (such as [this](https://en.wikipedia.org/wiki/Dwelling_in_the_Fuchun_Mountains) and [this](https://en.wikipedia.org/wiki/Wang_Ximeng)) and uses noises and mathematical functions to model the mountains and trees from scratch. It is written entirely in javascript and outputs Scalable Vector Graphics (SVG) format.
