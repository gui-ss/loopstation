# Etapa 6H.3A — OSC BOT no INPUT FX

Implementado em 16/10/2026. Referências: Parameter Guide local em references/RC-505mk2_Parameter_eng04_W.pdf, p.38, com conferência visual dos padrões em negrito; docs/MK2_BEHAVIOR_REFERENCE.md e fluxo de INPUT FX existente. O [PDF oficial](https://static.roland.com/assets/media/pdf/RC-505mk2_Parameter_eng04_W.pdf) confirma os nomes, intervalos e padrões abaixo. O algoritmo proprietário e suas curvas internas não são publicados.

## Parâmetros e acesso

| Parâmetro | Intervalo | Padrão oficial | Encoder/página |
| --- | --- | --- | --- |
| OSC | SAW, VINTAGE SAW, DETUNE SAW, SQUARE, RECT | SAW | 2 / página 1 |
| TONE | -50 a +50 | 0 | 3 / página 1 |
| ATTACK | 0 a 100 | 50 | 4 / página 1 |
| NOTE | C1 a G9, incluindo sustenidos | C2 | 1 / página 2 |
| MOD SENS | -50 a +50 | 0 | 2 / página 2 |
| BALANCE | 0 a 100 | 50 | 3 / página 2 |

1. F5 abre as configurações existentes. Selecione microfone e fones e ative o monitoramento com retorno baixo.
2. INPUT FX EDIT → selecione um slot disponível, por exemplo C. No editor, A/B/C/D selecionam o slot sem alternar ON/OFF.
3. Na primeira página, knob 1 (FX TYPE) seleciona OSC BOT. O catálogo foi ampliado; nenhum slot é substituído automaticamente, incluindo B/ROBOT.
4. Ajuste OSC/TONE/ATTACK nos knobs 2/3/4. A seta direita abre NOTE/MOD SENS/BALANCE nos knobs 1/2/3. Arraste ou use a rodinha; EXIT retorna.
5. Fora do editor, pressione o botão do slot escolhido para ligá-lo. Falar, cantar ou sustentar uma vogal alimenta o envelope do sintetizador. A altura é definida por NOTE; não é preciso cantar afinado naquela nota.
6. BALANCE 100 permite ouvir somente o sintetizador; 50 mistura voz e sintetizador; 0 conserva o sinal direto. ATTACK menor dá resposta mais rápida a sílabas e beatbox.
7. Para capturar o efeito, mantenha FX da pista ligado. A gravação e o overdub recebem o sinal processado. Desligar OSC BOT depois não remove o efeito já gravado; a reprodução não o aplica novamente. FX OFF por pista conserva captura seca.

Os valores de cada tipo permanecem em seu slot ao trocar tipo, página ou slot. Isso mantém a política anterior de armazenamento em RAM, sem introduzir persistência em disco ou outros bancos.

## DSP e diferenças conhecidas

Síntese subtrativa controlada pela entrada, com portadora de nota fixa. Os seguintes mapeamentos são **aproximações locais**, não características sonoras comprovadas do hardware:

- **SAW:** série de Fourier de dente de serra.
- **VINTAGE SAW:** mesma família com queda harmônica mais acentuada, proporcional a 1/h^1,25. Não é uma simulação de circuito analógico ou de deriva térmica.
- **DETUNE SAW:** três serras, central e ±7 cents, somadas com ganho 1/3.
- **SQUARE:** pulso de 50%, harmônicos ímpares.
- **RECT:** pulso de 25%, sem componente DC. O manual não informa a largura do pulso.
- **NOTE:** convenção de altura científica/MIDI: C1=24=32,703 Hz, C2=36=65,406 Hz, A4=69=440 Hz, G9=127≈12.543,854 Hz. O estado de parâmetros é independente da interface, permitindo que uma implementação futura forneça NOTE pelo mesmo caminho. Não há sequenciador.
- **ATTACK:** seguidor de amplitude com constante de ataque 2 + 498×(ATTACK/100)² ms; 50 corresponde a 126,5 ms. Relaxamento interno fixo em 50 ms, sem expor um parâmetro adicional. Um detector anterior usa 3/30 ms; silêncio não sustenta o oscilador.
- **TONE:** duas seções passa-baixas em cascata sobre a portadora, com corte relativo à nota. A relação é 1,1 + 12×((TONE+50)/100)², limitada à taxa de amostragem.
- **MOD SENS:** o manual diz que a entrada controla a modulação, mas não define destino, polaridade ou curva. Aqui o envelope da entrada modula o corte em até duas oitavas: valores positivos abrem o timbre, negativos o fecham, zero remove essa modulação. A amplitude continua respondendo à entrada em todos os valores.
- **BALANCE:** interpolação linear entre entrada do slot e síntese. Não existe monitor seco paralelo adicionado por este efeito.

O detector retira um piso suave de 0,0002 e limita o envelope de síntese a 0,6. Tabelas normalizadas e filtros estáveis preservam margem de pico sem saturação permanente. Não é um redutor de ruído: ruído de microfone acima do piso também pode excitar o efeito.

## Anti-aliasing, transições e execução

Banco imutável compartilhado de 4 famílias × 10 resoluções × 4097 floats (aproximadamente 640 KiB), preparado uma única vez na construção, fora do callback. As resoluções têm 1/2/4/.../512 harmônicos e são escolhidas pela frequência atual. A interpolação entre tabelas usa apenas harmônicos que cabem na banda; a interpolação das amostras é linear. Trata-se de redução de aliasing, não garantia de aliasing matematicamente nulo.

Fases permanecem contínuas. NOTE, OSC, TONE, ATTACK, MOD SENS, BALANCE e bypass têm transições de 20 ms; alterações de OSC misturam as formas de onda continuamente, inclusive com mudanças repetidas. O coeficiente do filtro é calculado a cada 16 amostras e interpolado entre atualizações.

Em 44,1/48 kHz, toda a faixa C1–G9 é representável. Próximo ao limite de Nyquist, os harmônicos superiores são removidos e as formas de onda se aproximam de tons senoidais. Em taxas baixas que não comportam determinada nota, a síntese é atenuada entre 0,45 e 0,49 da taxa e silenciada acima disso, em vez de produzir uma nota rebatida. As notas não são redefinidas no visor.

Não há lookahead, FFT ou alocação por bloco no processamento. Detector, envelopes, filtros e fases são próprios de cada instância/canal; somente tabelas de leitura são compartilhadas. Um snapshot atômico de 64 bits leva os parâmetros ao callback sem locks. O módulo recebe a mesma otimização por arquivo já usada pelo ROBOT em Debug/Visual Studio; os demais arquivos mantêm as configurações anteriores.

## Integração

AudioSystem mantém o único AudioDeviceManager. Cada um dos quatro slots possui armazenamento de parâmetros e instância de OSC BOT, além do ROBOT já existente. O processamento segue a ordem A→B→C→D. Somente o tipo selecionado permanece ativo; ao trocar de tipo, suas rampas curtas de bypass concluem a transição. A interface não redireciona efeitos para outras pistas ao selecionar um slot.

O sinal processado alimenta o monitor existente uma única vez e o caminho de gravação/overdub conforme FX por pista. Playback passa apenas pelo roteamento TRACK FX anterior. Preparação, desconexão, falha de dispositivo e ausência de entrada reinicializam os estados apropriados. Não foram modificados LoopTrack, LOOP SYNC, quantização ou DSP do ROBOT/LPF/DISTORTION.

## Validação

Aplicativo e testes compilados em **Debug x64 / Visual Studio 2022**, sem erros ou warnings na compilação final. **CTest: 1/1 aprovado em 45,49 s.** Uma verificação adicional do painel confirmou BALANCE=100 no modelo e na renderização.

- Cinco formas de onda; C1, C4, C7 e G9 em 44,1/48 kHz; conversão de semitons e rótulos dos extremos.
- Energia fora das regiões harmônicas nas notas altas: pior razão 8,34×10⁻⁶ (aproximadamente -50,8 dB). Medida com FFT/Hann de 32768 amostras, incluindo margem para os componentes detunados; contém fuga espectral e não é uma medida absoluta de aliasing.
- TONE -50/0/+50 e MOD SENS -50/0/+50 alteram o centroide espectral de forma ordenada. ATTACK 0/50/100 produz tempos de subida distintos.
- BALANCE 0/50/100: seco exato, média seco/síntese e síntese. OFF retorna ao sinal seco após a rampa.
- Ausência de entrada/silêncio, cauda, valores não finitos, alterações rápidas de parâmetros, isolamento de canal, instâncias e independência do tamanho do bloco.
- Caminho do callback: monitoramento sem loop, gravação wet/dry, reprodução com OSC BOT OFF, FX OFF por pista, overdub/Undo, desconexão/reconexão.
- Páginas, limites, defaults, rodinha, seleção de slots, EXIT e preservação de B/ROBOT; visor renderizado e inspecionado.
- Regressão existente: ROBOT, LPF/DISTORTION, cinco pistas, faders, dispositivos, BPM/TAP, QUANTIZE e LOOP SYNC. Detectores de alocação não encontraram alocações no processamento.

Medição de parede do callback completo, 200 blocos de 256 amostras: cinco loops em PLAY, TRACK FX, monitoramento, três OSC BOT DETUNE SAW/C1 e um ROBOT ativos:

| Taxa | Média | p99 | Máximo observado | Prazo do bloco |
| --- | ---: | ---: | ---: | ---: |
| 44,1 kHz | 0,974 ms | 1,751 ms | 2,405 ms | 5,805 ms |
| 48 kHz | 1,067 ms | 1,818 ms | 2,422 ms | 5,333 ms |

São medições neste host com dispositivo simulado, não garantia de desempenho de qualquer PC/driver. Não foi feita escuta real com microfone/fones nem comparação auditiva com uma RC-505 MK2. Timbre, resposta de articulação e curva de MOD SENS permanecem sujeitos à avaliação do usuário; não se declara fidelidade sonora exata.

Logs: tmp/osc-bot-6h3/build-final.log, dsp.log, ctest.log e build/Testing/Temporary/LastTest.log. Os argumentos --osc-bot e --osc-panel do executável de testes permitem verificar respectivamente DSP/editor e ligação do painel.

## Arquivos

Criados:

- Source/OscBotParameters.h
- Source/OscBotEffect.h e Source/OscBotEffect.cpp
- Tests/OscBotEffectTests.cpp
- docs/OSC_BOT_6H3A.md

Alterados:

- CMakeLists.txt
- Source/AudioSystem.h e Source/AudioSystem.cpp
- Source/InputFxEditor.h
- Source/MemoryDisplay.h e Source/MemoryDisplay.cpp
- Source/TopPanelComponent.h e Source/TopPanelComponent.cpp
- Source/MainComponent.cpp
- Tests/AudioSystemTests.cpp
- docs/MK2_BEHAVIOR_REFERENCE.md e docs/technical-reference.md

Não há FX SEQUENCE, presets de músicas, novos efeitos adicionais, novos menus ou novos indicadores nesta etapa.
