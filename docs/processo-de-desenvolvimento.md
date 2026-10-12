# Do pergaminho SVG ao renderizador contínuo: diário técnico do projeto

## Contexto

Este repositório é um fork de [LingDong-/shan-shui-inf](https://github.com/LingDong-/shan-shui-inf), projeto que gera paisagens chinesas (*shan shui*) infinitas em SVG. A proposta original — regras geométricas, ruído e aleatoriedade para formar montanhas, árvores, barcos e pavilhões — foi preservada. O trabalho deste fork foi transformar essa base em uma aplicação mais adequada para uma experiência de paisagem contínua e, depois, em uma referência C++ para a próxima etapa embarcada.

O objetivo final é executar uma versão dessa paisagem procedural em uma **ESP32-S3 4.3-inch Display Development Board**: geração contínua, exibição contínua e cenário aparentemente infinito em uma tela pequena. A implementação C++ de desktop encerra a etapa de referência visual e arquitetural; não é o destino de produção do dispositivo.

## Linha do tempo resumida

| Fase | Resultado |
| --- | --- |
| Fork e exploração do original | Compreensão do gerador SVG e da gramática visual de *shan shui*. |
| Aplicação web | A paisagem passou a ser gerada incrementalmente e movida como uma faixa contínua. |
| Port C++ inicial | PRNG, ruído, geometria, planejador de chunks, cenas e verificações determinísticas. |
| Renderização desktop | Pipeline Direct3D 11 com VSync, scroll subpixel e composição multiply fiel. |
| Diagnóstico de desempenho | Logs de stutter localizaram a rasterização de novas peças como gargalo. |
| Pipeline assíncrono | Rasterização saiu da UI; peças chegam à apresentação já prontas para upload GPU. |
| Fidelidade do traço | Contorno de 1 unidade e rasterização de contorno por segmento aproximaram o SVG sem sacrificar throughput. |
| Próxima fase | Analisar o C++ como referência e projetar uma arquitetura compatível com ESP32-S3. |

## Fase web: transformar o SVG em uma aplicação contínua

O `index.html` continua sendo a referência de intenção visual. O elemento central é uma faixa (`STRIP`) que recebe imagens das peças da cena. A câmera não reescreve toda a paisagem: ela move a faixa usando uma transformação CSS fracionária:

```js
strip.style.transform = "translate3d(" + x.toFixed(2) + "px,0,0)";
```

Esse detalhe foi decisivo. `requestAnimationFrame` atualiza a câmera a partir do tempo transcorrido, e o compositor do navegador executa a transformação. Cada peça já foi rasterizada como imagem; durante o movimento, o navegador não precisa redesenhar suas montanhas vetor a vetor.

Outras decisões da aplicação web que passaram a orientar o C++:

- geração em `Worker`, isolando trabalho procedural do ciclo de apresentação;
- fila de peças para rasterização incremental;
- janela/viewport com overflow oculto;
- *look-ahead* para gerar cenário antes de ele entrar na tela;
- remoção de peças muito atrás da câmera;
- `mix-blend-mode: multiply` na faixa, preservando a interação da tinta com o papel.

O último item não é apenas estético. No modo multiply, branco é neutro: um preenchimento branco não vira uma mancha branca, ele preserva o papel que já estava abaixo dele.

## Port para C++: manter a gramática, tornar o resultado verificável

O port em `cpp/src` separou o sistema em responsabilidades explícitas:

- `prng.*` e `noise.*`: aleatoriedade e ruído reproduzíveis a partir de uma seed;
- `stroke.*`, `blob.*` e `scenery.*`: geometria de pinceladas, texturas e objetos;
- `planner.*`: planejamento procedural em chunks de 512 unidades;
- `generator.*`: geração incremental em thread de trabalho e fila de eventos;
- `canvas.*`: rasterizador de referência e operações de composição;
- `raster.*`: conversão de uma `Piece` em imagem pronta para a apresentação;
- `scroll.*`: janela, câmera, streaming e renderizador desktop.

Foram mantidas verificações determinísticas para seed, ruído, traço, montanha, texturas, composição e planejamento. Elas produzem artefatos como `stroke-check.bmp`, `scene-check.bmp`, `chunk-check.bmp` e `scroll-check.bmp`. Esses checks são úteis para desenvolvimento, embora tornem a abertura normal do executável mais demorada; isso é um ponto conhecido para separar no futuro entre modo de verificação e modo de execução.

## Decisões visuais: o que tornou o C++ parecido com o SVG

### Pinceladas como fitas

Tanto o JavaScript quanto o C++ representam boa parte dos traços como uma fita fechada em volta de uma linha central. A largura varia com uma curva senoidal e com ruído. Isso produz pontas afinadas e irregularidade de tinta em vez de linhas matematicamente uniformes.

### O contorno que estava faltando

O HTML desenha cada `stroke()` com preenchimento e `stroke-width: 1` por padrão. No primeiro port, a fita C++ recebia preenchimento, mas quase sempre `outlineWidth = 0`. O resultado era tecnicamente semelhante na geometria, porém visivelmente mais fino: montanhas menos pesadas, hachuras menos densas e menos contraste.

A decisão foi tornar `1.0` o contorno padrão de `addRibbon()`. Isso restabeleceu o peso do traço do SVG, inclusive nas hachuras. No rasterizador de sprites, a largura do contorno também passou a ser escalada junto com o `viewBox`, espelhando o comportamento do SVG rasterizado.

### Anti-aliasing: SVG e Canvas próprio

O navegador rasteriza o SVG com seu mecanismo interno de cobertura de borda. Já o `Canvas` C++ usa supersampling 2×2: cada pixel é avaliado em quatro subamostras; 0, 1, 2, 3 ou 4 subamostras cobertas se transformam em 0%, 25%, 50%, 75% ou 100% de cobertura. É uma aproximação simples e determinística para reduzir serrilhado.

As junções também diferem sutilmente. SVG tende a usar a configuração padrão de junção em ponta (*miter*) quando não instruído de outra forma. O rasterizador C++ mede cobertura ao redor de segmentos; a união desses segmentos tem aparência mais arredondada em vértices. Essa diferença é aceita como aproximação do meio de saída, não como alteração da gramática do desenho.

## Caminhos que não resolveram o problema

### Limitar o loop a 60 Hz não bastava

Uma primeira resposta aos engasgos foi usar passo fixo e `Sleep`/cadência manual. Isso não resolvia a causa: a aplicação ainda reconstruía um frame de 3000×800 na CPU, combinava todos os sprites e chamava GDI a cada iteração. Uma cadência regular não compensa trabalho de dezenas ou centenas de milissegundos dentro de um frame.

### Direct2D com source-over alterou a arte

Outra tentativa carregou cada peça como textura Direct2D e deslocou-a na GPU. O movimento melhorou, mas a composição padrão era `source-over`, não multiply. Áreas brancas, neutras no SVG, passaram a aparecer como branco opaco; uma tentativa de tornar branco transparente criou recortes visualmente errados.

A lição foi importante: transparência e multiply não são intercambiáveis. A cor da tinta precisa ser combinada com a cor do papel que já está atrás dela.

### O outline de 1 px revelou um gargalo algorítmico

Depois de adicionar o contorno correto, a versão inicial de `Canvas::strokePolygon()` ficou lenta. Ela varria a caixa inteira do polígono e, para cada subamostra, calculava a menor distância até **todos** os segmentos. Em hachuras longas, esse custo crescia de forma impraticável; logs de `worker-raster` chegaram a centenas de milissegundos e, em casos extremos, segundos.

O conserto foi rasterizar por segmento. Cada segmento agora examina apenas sua faixa estreita de influência e mantém as quatro subamostras de anti-aliasing. Isso elimina a comparação de cada pixel contra a lista completa de segmentos e torna viável manter o contorno de 1 unidade.

## Renderizador desktop final

O renderizador de referência para Windows usa Direct3D 11 em duas passagens:

1. Cada peça rasterizada é uma textura. Todas as texturas visíveis são desenhadas, em ordem, em um alvo transparente de tinta com composição source-over.
2. Um shader de tela inteira combina o alvo de tinta com o papel:

```text
resultado = papel × (1 − alphaDaTinta + rgbPremultiplicadoDaTinta)
```

Essa é a forma premultiplicada de multiply usada pelo `mix-blend-mode: multiply` da aplicação web. Ela preserva o papel sob tinta branca e devolve o contraste correto de áreas escuras e semitransparentes.

O swap chain apresenta com `Present(1, 0)`, pedindo VSync. A câmera é um `double` atualizado por tempo decorrido e sua translação é enviada à GPU como `float`, permitindo movimento subpixel.

## Stutters: evidência, causa e solução

Os logs de diagnóstico foram adicionados somente acima de um limiar de frame lento. Eles separaram `messages`, extensão do gerador, rasterização na UI, upload GPU, limpeza e apresentação. O resultado foi inequívoco: os frames longos eram quase inteiramente rasterização de peças novas na thread de UI; upload e VSync ficavam tipicamente na ordem de poucos milissegundos.

A solução foi mover `rasterizePiece()` para a thread do `Generator`. Um evento agora transporta a `Piece` lógica e sua `RasterizedPiece` pronta. A UI apenas ordena a peça, cria a textura e a desenha. Isso removeu a etapa CPU cara do caminho crítico da animação.

Também foi aumentado o horizonte de pré-carregamento. O erro anterior era pedir `extend(plan.xmax())`, que geralmente acrescentava somente um chunk de 512 unidades. Agora o gerador recebe o limite completo solicitado pela câmera, com *look-ahead* de 4096 unidades. A intenção é que uma nova montanha esteja pronta muito antes de sua entrada no viewport — sem revelar sua construção.

## Estado atual e encerramento da etapa desktop

A etapa C++ para computador está considerada concluída no nível esperado:

- scroll contínuo com VSync e posição subpixel;
- composição multiply fiel ao modelo web;
- geração e rasterização fora da UI;
- horizonte de conteúdo preparado;
- traço e hachura reforçados pelo contorno equivalente ao SVG;
- rasterização de contorno otimizada;
- diagnósticos disponíveis para investigação pontual.

O C++ deve ser tratado daqui em diante como uma referência estável. Mudanças futuras não devem reabrir a implementação desktop sem uma necessidade explícita de correção.

## Próxima etapa: arquitetura para ESP32-S3

O código desktop não pode ser copiado literalmente para o ESP32-S3. Direct3D 11, swap chain, texturas de desktop e a memória disponível no PC não existem nesse alvo. A etapa seguinte é uma análise arquitetural, com estas perguntas como ponto de partida:

- Qual biblioteca e qual driver comandarão a tela de 4.3 polegadas?
- Há PSRAM disponível e qual orçamento de memória cabe para tiles, sprites e fila de geração?
- A composição multiply será feita por software, por tabela/lookup, ou por uma aproximação visual adequada?
- Qual resolução interna e qual formato de pixel mantêm fluidez no barramento do display?
- Quais elementos devem ser pré-rasterizados, reduzidos, cacheados ou simplificados?
- Como dividir geração, rasterização e envio para a tela entre tarefas do FreeRTOS sem bloquear a atualização visual?
- Como converter o horizonte de chunks em um cache circular de tiles compatível com a memória do dispositivo?

A premissa a preservar é a mesma que guiou o desktop: nunca construir um objeto quando ele já está visível. O dispositivo deverá gerar e preparar um horizonte à frente, descartar conteúdo distante e limitar o trabalho por atualização de tela.

## Material para um futuro post

Uma narrativa possível para um artigo sobre o processo é:

1. O que torna a paisagem SVG original expressiva.
2. Como uma aplicação web contínua separa geração, imagens e transformação de câmera.
3. Por que um port direto para CPU/GDI produz judder.
4. A diferença entre alpha comum e multiply — e por que os brancos denunciaram o erro.
5. Como logs por fase revelaram que o gargalo era rasterização, não GPU.
6. A importância de pré-carregar cenário e de não mostrar a construção do mundo.
7. A otimização de contorno por segmento.
8. O que muda radicalmente ao sair de um desktop com GPU para um ESP32-S3.
