# Etapa 6H.3D — STEP SLICER no INPUT FX

## Referências e limites da reprodução

Consultados `references/RC-505mk2_Parameter_eng04_W.pdf`, páginas 5 e 40, e `docs/MK2_BEHAVIOR_REFERENCE.md`. PDFs e arquivos sincronizados não foram modificados.

A [tabela oficial da MK2, p.40](https://static.roland.com/assets/media/pdf/RC-505mk2_Parameter_eng04_W.pdf) confirma os parâmetros, limites e padrões. A linha RATE mostra 4MEAS/2MEAS/1MEAS e o intervalo de mínima a fusa, com semínima como padrão, mas não enumera todos os valores intermediários. A [legenda oficial de notas da RC-505 V2, p.2](https://static.roland.com/assets/media/pdf/RC-505_V2_e02_W.pdf) explicita notas pontuadas e tercinas nessa família. Ela complementa a interpretação musical, sem ser apresentada como uma medição do firmware MK2.

No LCD, `T` significa tercina e `.` significa nota pontuada. A notação `♪3` fornecida no pedido corresponde a **colcheia de tercina**, representada como `1/8T`: três passos por semínima. Em 165 BPM, cada passo dura aproximadamente 121,212 ms (5.818,1818 amostras em 48 kHz). Fronteiras fracionárias são distribuídas entre amostras, sem arredondar o período inteiro repetidamente. A equivalência musical é documentada; a ordem exata de todos os valores intermediários no firmware MK2 e a configuração efetivamente mostrada no tutorial ainda dependem de comparação com o equipamento/vídeo.

Não foram implementados PATTERN SLICER, TRACK FX SLICER, LEDs novos ou outras etapas. O código DSP de ROBOT, OSC BOT e REVERB e o despachante F6–F10 foram preservados.

## Parâmetros

| Parâmetro | Faixa / padrão |
|---|---|
| RATE | 0–100; 4MEAS, 2MEAS, 1MEAS, 1/2, 1/4., 1/2T, **1/4 padrão**, 1/8., 1/4T, 1/8, 1/16., 1/8T, 1/16, 1/32 |
| STEP MAX | 1–16; **16** |
| STEP LENGTH | 1–100 por passo; **50** |
| STEP LEVEL | 0–100 por passo; **100** |
| DEPTH | 0–100; **100** |
| COMP THRESHOLD | -30–0 dB; **-30 dB** |
| COMP GAIN | 0–20 dB; **+6 dB** |

ON/OFF é o botão do slot. Não há parâmetro de pitch, delay, nota ou FX SEQUENCE do OSC BOT reutilizado no SLICER. Todos os 16 pares LENGTH/LEVEL permanecem armazenados mesmo ao reduzir STEP MAX. Valores são conservados por tipo, slot e banco durante a execução; não foi acrescentado salvamento em disco.

## DSP e relógio

`StepSlicerEffect` processa estéreo com fase e ganho de corte compartilhados. Usa um compressor de pico vinculado entre canais, depois ganho de compensação, proteção suave de pico vinculada entre canais e modulação de amplitude. A janela audível é LENGTH% do passo; LEVEL define seu ganho. DEPTH interpola entre ganho unitário e essa janela: zero remove somente a modulação, mantendo compressão/ganho. LEVEL zero com DEPTH 100 chega a silêncio após a curta transição. Não há lookahead nem atraso adicional de buffer.

Decisões locais, pois o algoritmo proprietário não é publicado: razão de compressão 4:1, ataque de 2 ms, recuperação de 80 ms; transição de corte de até 1 ms; suavização de profundidade de 10 ms e de limiar/ganho/bypass de 20 ms; joelho suave de proteção acima de 0,8, assintótico a 0,98. Não há alegação de equivalência sonora BOSS. A curta rampa pode reduzir a abertura plena em LENGTH extremamente pequeno e RATE muito rápido.

O callback passa os incrementos do **MusicalClock existente**, incluindo a grade utilizada por LOOP SYNC. RATE musical define a duração de um passo em batidas (4/4); STEP MAX define o número de passos da repetição. RATE numérico usa uma aproximação local explícita de 0,1–20 Hz: `0.1 * 200^(RATE/100)`. A curva numérica não é fornecida pelo manual.

Mudar BPM ou RATE conserva a posição fracionária do passo. Ao ligar o slot, o SLICER começa no passo 1 com crossfade; ao desligar, o sinal volta ao bypass exato após a rampa. Não reinicia MusicalClock, loops ou OSC BOT. Essa política de início é local, e funciona com ou sem frases tocando; não implementa um novo modo de LOOP SYNC ou quantização.

Parâmetros passam por palavras atômicas e leitura coerente com uma tentativa limitada por bloco. Se a UI estiver publicando, o áudio conserva o último conjunto consistente, sem esperar. Estados, arrays e buffers são preparados fora do callback. Não há mutex, alocação, arquivos ou acesso a componentes gráficos no novo processamento.

## Bancos e roteamento

O seletor BANK A–D foi autorizado nesta etapa. Cada banco contém quatro instâncias independentes de INPUT FX, incluindo parâmetros e estados DSP. A troca tem crossfade de 20 ms entre cadeias, sem somar novamente o microfone. Durante essa transição, duas cadeias são processadas; depois, somente a selecionada. Pedidos rápidos de troca são consolidados após a transição em andamento. Bancos inativos conservam estado e congelam sua fase; não continuam gerando áudio oculto. Ao retornar, o estado é retomado; troca de dispositivo reinicializa estados DSP, conservando configurações e frases como antes. Este é um seletor mínimo, não a implementação completa de SW de banco, seleção por pressão longa ou todos os menus do hardware.

O modo SINGLE/MULTI é conservado por banco. SINGLE limita a habilitação a um slot; MULTI conserva os slots simultâneos. A seleção de banco recupera os ON/OFF daquele banco. Nenhum preset musical é carregado automaticamente.

A topologia existente foi preservada: OSC BOT adjacentes compartilham a excitação e têm mistura paralela normalizada; efeitos posteriores processam o resultado em série. Para o cenário pedido:

`microfone → mistura OSC A/B → STEP SLICER C → REVERB D → monitor e entrada das cinco pistas`

Há um único caminho de monitoramento. FX OFF de uma pista continua capturando o microfone seco; FX ON captura a cadeia. O áudio reproduzido não passa por INPUT FX novamente. As caudas do REVERB D continuam sendo processadas mesmo quando o SLICER/OSC é desligado; desativar o próprio D mantém o bypass existente do REVERB. TRACK FX continua no caminho de reprodução.

A revisão anterior identificou no REVERB a limpeza integral de seus buffers ao concluir bypass e a leitura de seu ON/OFF separada da palavra de ativação de grupos. Esses comportamentos anteriores não foram reescritos nesta etapa, que proíbe modificar seu DSP. Portanto, a preservação de F10 não equivale a uma nova garantia de ativação atômica de pares contendo REVERB. O novo SLICER utiliza a mesma publicação atômica de grupo dos OSC/ROBOT.

## Como usar o LCD

1. Pressione INPUT FX EDIT. A/B/C/D selecionam o slot enquanto o editor está aberto, sem mudar ON/OFF.
2. Knob 1 em FX TYPE seleciona STEP SLICER, depois de REVERB.
3. Página 1: FX TYPE / RATE / STEP MAX / DEPTH.
4. Seta direita, página 2: COMP THRESHOLD / COMP GAIN.
5. Páginas 3–10: dois passos por página. Knobs 1/2 editam LENGTH/LEVEL do primeiro passo; knobs 3/4, do segundo. Assim, página 3 edita 1/2; página 4 edita 3/4; até página 10 editar 15/16. Os rótulos mostram os números.
6. Página 11: BANK A–D no knob 1 e INPUT MODE SINGLE/MULTI no knob 2. Essa página final também foi acrescentada aos demais tipos, sem mudar suas páginas anteriores. O cabeçalho mostra banco e slot.
7. EXIT retorna. Fora do editor, o botão do slot (ou F6–F9) alterna ON/OFF. F10 mantém a função anterior de A+B em MULTI. Arraste e roda dos knobs continuam disponíveis.

## Cenário musical — somente configuração manual/teste

Defina 165 BPM, banco A e MULTI. Configure OSC BOT em A e B e suas FX SEQUENCE como em `FX_SEQUENCE_6H3B.md`. Em C selecione STEP SLICER: RATE **1/8T**, DEPTH **50**, THRESHOLD **-15 dB**, GAIN **+6 dB**, MAX **16**; inicialmente LENGTH 50 e LEVEL 100 nos 16 passos. Em D selecione o REVERB já existente e ajuste como preferir.

Saia do editor; use F10 para A+B e F8/F9 para C/D. Ative o monitoramento em F5 e produza voz/beatbox no microfone usando fones. Alterne C para ouvir o recorte; mantenha D ligado para ouvir as caudas nos intervalos. Experimente LEVEL 0 em um passo com DEPTH 100 e compare LENGTH 20/80. Grave em uma pista com FX habilitado, desligue os INPUT FX e o retorno e reproduza: os cortes e a reverberação gravados devem permanecer.

## Validação

Na revisão 6H.4 (19/10/2026), Debug x64 compilou e a suíte agregada passou (1/1, 151,04 s), incluindo STEP SLICER, RATE 1/8T, bancos/slots, cadeia com dois OSC BOT e REVERB, captura nas cinco pistas e overdub/Undo. Resultados e medições atualizados em [TRACK_LEDS_RECORDING_6H4.md](TRACK_LEDS_RECORDING_6H4.md).

Escuta física com microfone/fones e comparação com hardware BOSS permanecem pendentes. Sinais sintéticos e tempos de callback não comprovam fidelidade auditiva, latência física ou ausência de underruns em todos os computadores.

## Arquivos

Novos: `Source/StepSlicerParameters.h`, `Source/StepSlicerEffect.h/.cpp`, `Tests/StepSlicerEffectTests.cpp`, este documento.

Integração: `Source/AudioSystem.h/.cpp`, `Source/InputFxEditor.h`, `Source/MemoryDisplay.h/.cpp`, `Source/TopPanelComponent.h/.cpp`, `Source/MainComponent.cpp`, `CMakeLists.txt`. Referências atualizadas: `docs/technical-reference.md` e `docs/MK2_BEHAVIOR_REFERENCE.md`.

Testes: `Tests/AudioSystemTests.cpp` e ajustes de contagem de páginas/catálogo em `Tests/RobotEffectTests.cpp`, `Tests/OscBotEffectTests.cpp`, `Tests/FxSequenceTests.cpp`, `Tests/ReverbEffectTests.cpp`. As verificações dos algoritmos anteriores permanecem.
