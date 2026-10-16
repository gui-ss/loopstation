# Etapa 6H.2 — qualidade e custo do INPUT FX ROBOT

Data: 16/10/2026. Escopo: apenas ROBOT, sua configuração de compilação e testes. AudioSystem, LoopTrack, roteamento, interface e outros efeitos não foram modificados.

## Referências e diagnóstico

Consultados o Parameter Guide oficial local, p. 36 (inclusive a ilustração), as seções INPUT FX/FX por pista e MK2_BEHAVIOR_REFERENCE.md. Permanecem NOTE C–B, FORMANT -50–+50 e MODE 1/2. O manual distingue algoritmo anterior/novo, mas não publica os algoritmos. As duas implementações continuam aproximações, não equivalência sonora BOSS. C3–B3 e deslocamento de envelope de ±0,75 oitava são mapeamentos locais anteriores, não especificações oficiais.

Foi revisado todo RobotEffect.cpp/.h e o caminho em AudioSystem: entrada → slots INPUT FX → monitor único/captura por pista → RAM; reprodução → TRACK FX → soma. Não foi encontrado caminho duplicado de monitoramento. FX OFF por pista conserva captura seca; o loop gravado com efeito não é processado novamente pelo ROBOT.

Problemas encontrados:

1. **Prazo do áudio excedido em Debug:** a interpolação espectral, pesos e limites de até 64 harmônicos eram recalculados por amostra, com trabalho adicional por canal. Notas graves ativavam mais harmônicos. A medição do DSP antigo isolado chegou a consumir 2,076 vezes o tempo do sinal. Isso é suficiente para inviabilizar entrega contínua; os testes antigos de valores finitos e pico limitado não detectavam atrasos. Não houve coleta de xruns do dispositivo físico do usuário: essa é uma causa reproduzida de sobrecarga e uma explicação provável do chiado ouvido, não uma medição daquele evento.
2. **Ondulação do envelope:** o detector retificado de ataque 3 ms/release 18 ms modulava a portadora com a periodicidade da voz grave. Em vogal sintética a 80 Hz, a energia de diferença entre períodos da nota alvo era cerca de 6% da energia do sinal.
3. **Energia e reconstrução:** mais harmônicos nas notas baixas aumentavam energia e picos; tanh atuava em todos os níveis, acrescentando não linearidade. Todo sopro/consoante era reconstruído sobre uma portadora periódica.

Não existe detector de F0, portanto não havia saltos de oitava de rastreamento a corrigir. Não existe FFT/STFT, janelamento ou overlap-add neste vocoder. A fase já era contínua entre blocos e foi preservada. Não foram encontrados NaNs, DC persistente ou vazamento entre canais nos testes novos. Esses testes não excluem problemas de driver/hardware.

## Correção

- Mapeamento dos parciais a cada 16 amostras, com interpolação linear de amplitudes em todas as amostras; fase continua em áudio-rate. Trabalho pequeno e limitado, independente do bloco do driver.
- Otimização somente de RobotEffect.cpp no gerador Visual Studio, inclusive Debug. Símbolos permanecem; verificação /RTC desativada somente nessa unidade por incompatibilidade com /O2. Demais arquivos mantêm Debug normal e o detector de alocações CRT continua funcionando.
- Dois polos de 4 ms suavizam o **envelope modulador**, sem cortar frequências graves do sinal.
- Compensação de energia pela densidade de harmônicos, proporcional à raiz da frequência alvo.
- Proteção de pico estéreo vinculada, linear até 0,55 e com joelho suave abaixo de 0,8; não há tanh permanente.
- Planicidade espectral, compensada pela largura das bandas, reconhece material de banda larga. Uma transição de 8 ms preserva esse material original dentro do efeito, substituindo a síntese nesses trechos. Não se soma um segundo monitor seco. Trata-se de uma heurística, não reconhecimento perfeito de fonemas.
- NOTE/MODE/bypass mantêm suavização de 20 ms e FORMANT de 30 ms; tipos, valores, visor e controles não mudaram. FORMANT desloca a posição de leitura do envelope espectral, sem mudar a grade de alturas.

Não foram adicionados buffers de lookahead nem filas de áudio. A nova interpolação acrescenta no máximo 16 amostras ao controle de amplitudes (0,36 ms em 44,1 kHz); os dois polos acrescentam aproximadamente 8 ms à resposta lenta do envelope. Isso não equivale à latência total de microfone–fones, que depende do dispositivo e não foi medida fisicamente. Consoantes rápidas e beatbox ainda exigem escuta.

## Signalsmith Stretch: avaliação, sem instalação

O projeto oferece pitch-shifting polifônico e ajuste de formantes, com licença MIT e implementação espectral. Pode ser interessante para preservar mais características de uma voz transposta. A própria documentação recomenda otimização e alerta para custo maior em Debug. Não foi instalado, integrado nem submetido a benchmark local. [README oficial](https://github.com/Signalsmith-Audio/signalsmith-stretch).

O código consultado (versão declarada 1.3.2) configura janelas de 120 ms/hop 30 ms no preset padrão e 100 ms/hop 40 ms no econômico; isso é tamanho de janela, não uma medição de latência fim a fim. Expõe inputLatency/outputLatency e splitComputation acrescenta um hop à saída. Transposição por razão não fixa sozinha uma nota absoluta: seria necessário estimar F0 e tratar regiões não vozeadas. Reduzir janelas pode prejudicar resolução nas notas graves. Diante da sobrecarga e modulação concretamente corrigidas aqui, não há benefício comprovado que justifique trocar o motor por essa dependência nesta etapa. [Código oficial consultado](https://github.com/Signalsmith-Audio/signalsmith-stretch/blob/main/signalsmith-stretch.h).

## Medições reproduzíveis

Executável Debug x64, host desta tarefa, blocos de 256 amostras. Matriz de 48 casos: 44,1/48 kHz × F0 de entrada 80/187 Hz × NOTE C/B × FORMANT -50/0/+50 × MODE 1/2. Fonte determinística com formantes em 600/1550/2600 Hz, pico de entrada 0,35. Trecho estável analisado após 300 ms.

O resíduo periódico é a energia de y[n] − y[n − sampleRate/NOTE], com interpolação fracionária, dividida pela energia de y. Mede irregularidade de vogal sustentada; não é SNR perceptual nem comparação com o hardware.

| Caso: entrada 80 Hz, NOTE C, FORMANT 0, MODE 2 | Antes | Corrigido |
| --- | ---: | ---: |
| 44,1 kHz: fração média do prazo do áudio | 156,09% | 2,47% |
| 48 kHz: fração média do prazo do áudio | 175,85% | 3,12% |
| 44,1 kHz: resíduo periódico | 0,06051 | 0,002544 |
| 48 kHz: resíduo periódico | 0,06054 | 0,002525 |
| 48 kHz: pico / RMS | 0,34130 / 0,05227 | 0,21737 / 0,03774 |

Na matriz corrigida: maior fração média 3,78%; maior p99 de bloco 0,403 ms; maior bloco isolado 0,861 ms; maior pico 0,658 e RMS 0,1084. Prazos dos blocos: 5,805 ms em 44,1 kHz e 5,333 ms em 48 kHz. São medidas de parede deste host, sujeitas ao escalonador; não garantem desempenho em qualquer PC.

Outras verificações:

- Diferença entre MODEs após normalizar energia: 0,5557; centroides espectrais de FORMANT -50/0/+50: 6,37/11,82/17,86 harmônicos. Mudanças não são apenas ganho.
- Ruído determinístico representando sopro: correlação praticamente 1 e ganho RMS aproximadamente 1 no trecho estável; sem amplificação. Não equivale a testar todas as consoantes humanas.
- Transição C→B com seno grave: maior salto adjacente <0,009; cauda após silêncio <6e-10.
- Blocos de 31 e 257 amostras produzem resultados idênticos; canais e instâncias independentes.
- NaN/Inf na entrada não contaminam o estado; DC sustentado não gera portadora permanente.
- Testes anteriores preservados: todas as notas amostradas, limites, taxas de 8/44,1/48/96/192 kHz, bypass seco exato após crossfade, silêncio, ausência de alocações, encoders/slots/EXIT.
- Integração real com o callback simulado: gravação wet/dry, reprodução sem reaplicar INPUT FX, overdub/Undo, cinco pistas, faders, TRACK FX, dispositivos, BPM, QUANTIZE e LOOP SYNC.

Artefatos locais: tmp/robot-6h2/baseline.csv, corrected.csv, build-final.log, ctest.log; log completo da suíte em build/Testing/Temporary/LastTest.log. A fonte anterior foi guardada em tmp/robot-6h2 para comparação. O comando de teste --robot-quality executa somente os testes determinísticos de qualidade; a execução normal inclui a regressão e mede o callback completo com cinco pistas, TRACK FX, monitor e 0/1/4 slots ROBOT.

**Resultado final:** aplicativo e testes compilados em Debug x64, sem warnings/erros na compilação final. CTest: **1/1 aprovado em 38,86 s**, incluindo todas as verificações listadas acima e nenhuma alocação detectada no callback.

| Callback completo, 256 amostras / 200 blocos por caso | Média | p99 | Máximo | Prazo |
| --- | ---: | ---: | ---: | ---: |
| 44,1 kHz, 0 ROBOT | 0,616 ms | 1,244 ms | 1,865 ms | 5,805 ms |
| 44,1 kHz, 1 ROBOT | 0,887 ms | 1,650 ms | 2,153 ms | 5,805 ms |
| 44,1 kHz, 4 ROBOT | 1,239 ms | 2,213 ms | 2,382 ms | 5,805 ms |
| 48 kHz, 0 ROBOT | 0,614 ms | 1,061 ms | 1,089 ms | 5,333 ms |
| 48 kHz, 1 ROBOT | 0,736 ms | 1,247 ms | 1,544 ms | 5,333 ms |
| 48 kHz, 4 ROBOT | 1,297 ms | 2,534 ms | 2,564 ms | 5,333 ms |

Esses tempos incluem o callback de produção via dispositivo simulado, cinco loops em PLAY, LPF/DISTORTION e monitoramento. Não incluem driver físico, latência de conversores ou interferência de outros programas. Não se afirma ausência universal de underruns.

## Teste manual restante

1. F5 → selecionar microfone/fones → ativar monitoramento com retorno baixo.
2. INPUT FX EDIT → B → knob 1 ROBOT; knobs 2/3/4: NOTE/FORMANT/MODE. EXIT → botão B liga/desliga. Nenhum slot é substituído automaticamente.
3. Sustentar vogais graves em C/C#/D, alternar para A/B e comparar os modos. Testar FORMANT -50/0/+50, sibilantes, sopros, ataques de beatbox e pausas.
4. Gravar com FX da pista ligado e ROBOT ON; parar, desligar ROBOT e reproduzir: o efeito deve continuar gravado. Comparar com nova captura OFF e verificar overdub/Undo.

**Limites:** não foi feita escuta física com microfone/fones. Nenhuma alegação de fidelidade sonora à MK2. A decisão entre voz sintetizada e ruído pode falhar em voz muito soprosa ou percussões; suavização pode amaciar ataques. O teste físico determinará se o timbre e a resposta ficaram adequados, e se restam artefatos do dispositivo.
