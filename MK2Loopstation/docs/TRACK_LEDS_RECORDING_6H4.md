# Etapa 6H.4 — revisão, gravação e indicadores

## Referências e escopo

Parameter Guide oficial rev.04, p.35 (G2B), p.30 (SETUP / INDICATOR), pp.2–4 (transporte/FX/QUANTIZE/LOOP SYNC); Manual do Proprietário pp.5 e 9; `MK2_BEHAVIOR_REFERENCE.md`. As imagens das páginas 30 e 35 foram conferidas. PDFs e fontes sincronizadas não foram alterados.

O usuário esclareceu que percebe **atraso ou loop fora do tempo, inclusive sem INPUT FX**. Não informou o estado de Q/L nesse teste. Portanto, o defeito do G2B abaixo não explica sozinho o problema relatado.

## Revisão do código recebido

Foram inspecionados CMake, AudioSystem, LoopTrack/LoopStorage, MusicalClock, catálogo/editor e publicação de INPUT FX, OSC BOT/FX SEQUENCE, ROBOT, REVERB, STEP SLICER, G2B, atalhos F6–F10 e componentes das pistas. Os arquivos novos de G2B são G2BParameters.h, G2BEffect.h/.cpp e G2BEffectTests.cpp. A integração recebida inclui CMake, AudioSystem, InputFxEditor, MemoryDisplay, TopPanelComponent, MainComponent e AudioSystemTests. Sem histórico Git disponível, esta lista identifica a integração presente, não atribui cada linha a um autor.

G2B recebe o microfone antes do monitoramento e da captura. Tipos/gates/banco chegam por uma publicação atômica; instâncias são independentes por banco/slot. Reprodução não volta aos INPUT FX. OSC BOT consecutivos continuam agrupados em paralelo; demais efeitos seguem o encadeamento existente. F6–F10 e DSP dos demais efeitos não foram alterados.

## G2B: defeito comprovado e correção

O teste recebido falhou em 44,1 kHz: MODE 2 com entrada de 146,83 Hz produziu pico dominante em aproximadamente 63,7 Hz, em vez de 73,415 Hz. O delay da primeira cabeça reiniciava a cada 18 ms, sem relação com o período detectado; somente a separação da segunda cabeça era corrigida. O janelamento gerava bandas laterais dominantes.

Correção localizada: janela com extensão de dois períodos de entrada e cabeças separadas por um período; velocidade de leitura de metade da escrita. A extensão segue o detector com variação limitada, conservando continuidade. O buffer, preparado fora do callback, cobre 60 ms. Reset agora invalida o histórico em O(1), sem limpar todo o buffer no callback; perda de entrada e dispositivo também reinicia G2B.

BALANCE 0–100 (padrão 50) e MODE 1/2 (padrão 2) foram preservados. MODE 1 é divisor de frequência com onda quadrada e envelope; MODE 2 é transposição granular monofônica com janelas alinhadas ao período. São aproximações, não os algoritmos proprietários BOSS. A saída processada é mono a partir da soma estéreo; BALANCE 0/bypass preservam estéreo. Fontes polifônicas, estéreo em oposição de fase e sons sem fundamental clara continuam limitações.

Correções nos testes recebidos: tolerâncias que dispensavam a verificação das notas graves foram removidas; medição de sinal seco duplicado passou a usar seno puro, pois o segundo harmônico de uma voz transposta legitimamente coincide com a frequência original. Comparação estéreo ocorre depois dos 20 ms de crossfade de bypass. O teste de alocação passou a usar armazenamento fixo para suas próprias medições. Testes do editor respeitam sua página BANK e o catálogo com G2B.

## Diagnóstico da gravação

Caminho observado: microfone → INPUT FX (uma vez) → monitor com ganho próprio; a mesma saída processada alimenta cada captura quando FX da pista está ON. FX OFF por pista captura seco. Reprodução → fader/roteamento TRACK FX → mixagem final. Monitoramento e saída não voltam à captura.

Não há compensação da latência de entrada/saída do dispositivo nem do tempo interno variável de INPUT FX. Comandos de interface são consumidos no callback, não carimbados com o instante físico da performance. O atraso real depende também do driver, buffer, conversores e eventual monitoramento externo do Windows/interface. Não se pode concluir que a latência física foi resolvida com sinais sintéticos.

A janela F5 passa a informar sample rate, buffer e latências de entrada/saída **reportadas pelo driver**, sem somar o buffer novamente e sem tratar esses dados como medição física. Valores zero podem significar informação não disponível. Os valores não incluem FX.

G2B MODE 2 usa leituras entre 6 ms e 6 ms + dois períodos detectados (até cerca de 50 ms na faixa de detecção); não tem um atraso fixo compensável. Em teste de início de voz a 147 Hz/48 kHz, o limiar de saída de 0,02 foi atingido em 2,02 ms no MODE 1 e 9,81 ms no MODE 2. Isso é tempo de início de resposta desse sinal, **não** latência física total nem alinhamento perfeito de uma onda transposta. A estabilização em mudança de nota pode demorar mais.

O motor aplica tapers não destrutivos de até 3 ms nas pontas do loop, rampa inicial de 5 ms e fader suavizado. Eles podem suavizar um ataque exatamente na emenda, mas não inserem amostras nem mudam o período. Não foram removidos nem foi aplicado ganho/compensação arbitrária.

QUANTIZE congela a grade no início da captura e espera o próximo fim de compasso relativo a esse início. LOOP SYNC / MEASURE também pode esperar o próximo compasso para iniciar. Essas esperas são intencionais; precisam ser distinguidas de atraso de monitoramento. O transporte atual suporta REC → PLAY ↔ OVERDUB, não a configuração alternativa REC ACTION = REC → DUB. Não foi acrescentada essa funcionalidade; a transição imediata PLAY → DUB pode ser testada separadamente.

## Indicadores implementados

Conforme PG p.30:

| Estado | STATUS (inicial) | LOOP POSITION | LEVEL |
| --- | --- | --- | --- |
| Vazia | Apagado | Apagado | Apagado |
| Parada com frase | Aceso | Apagado | Apagado |
| REC | Pisca no andamento | Apagado; comprimento ainda desconhecido | Apagado |
| OVERDUB | Pisca no andamento | Posição real do loop | Nível real de reprodução |
| PLAY | Posição em um compasso | Posição real do loop completo | Nível real de reprodução |

O anel externo existente usa 32 segmentos vermelhos. O botão central mantém REC vermelho, PLAY verde, OVERDUB amarelo/laranja e STOP apagado. A contagem/curva luminosa dos segmentos, preenchimento progressivo e duty cycle de 50% do piscar são aproximações visuais; a tabela oficial especifica os modos, não esses detalhes. Nenhum controle foi reposicionado.

LOOP POSITION usa a razão publicada por LoopTrack entre posição de leitura e comprimento da frase no seu domínio de amostras. A interface não integra tempo nem assume 3/6/12 segundos. Mudanças de BPM em gravação livre não aceleram o anel nem o áudio. STOP/limpeza o apagam; retomada/LOOP SYNC seguem exatamente o playhead existente.

LEVEL mede pico estéreo por pista depois do fader/rampas e INSERT individual, antes do bus compartilhado ALL, monitor e proteção da soma final. Ataque imediato e release exponencial de 150 ms no domínio das amostras. O anel usa o maior canal, escala -60 a 0 dBFS; silêncio fica apagado. O visor LOOP LEVEL usa os mesmos níveis de cada canal, substituindo a antiga simulação. Não foi acrescentado ganho ao áudio.

**Limite do medidor:** efeitos não lineares INSERT ALL processam uma soma. Não existe atribuição exata de sua saída a cada pista; o medidor observa a contribuição anterior a esse bus. Isso evita duplicar processadores ou mostrar o nível da mistura em todas as pistas.

AudioSystem publica um frame com cinco estados/comprimentos/posições/níveis e a posição do MusicalClock, ao final do callback e das transições de dispositivo. Todos os campos da publicação são atômicos e lock-free, com versão comum. A interface tenta ler no máximo duas vezes e conserva o último frame válido em caso de concorrência. Nenhuma leitura rejeitada acessa memória comum não atômica. Não há fila, alocação, mutex, espera ou acesso à UI no callback.

A interface lê a 30 Hz; pode atrasar uma atualização visual, mas não acumula deriva. A posição mostra a amostra de processamento do motor, sem compensação inventada da latência física da saída. O antigo timer de simulação do visor foi removido.

## Como selecionar e testar

1. Clique **MENU**. Nesta etapa ele abre diretamente a página mínima **SETUP / INDICATOR**, sem árvore completa de menus.
2. Arraste ou use a rodinha do **primeiro knob abaixo do visor**: STATUS, LOOP POSITION ou LEVEL. A mudança vale para os cinco anéis.
3. **EXIT** restaura a página/editor anterior sem alterar parâmetros de efeitos. Seleção mantida em RAM nesta execução; padrão STATUS ao iniciar.
4. Para comparar loops livres de aproximadamente 3 e 6 segundos, deixe Q e L OFF e confirme no visor; grave T1 e T2, encerre cada REC no instante desejado, deixe ambas em PLAY. LOOP POSITION deve completar ciclos com a duração de cada gravação, incluindo frações de segundo realmente capturadas.
5. Em LEVEL, compare frase forte, fraca, silêncio e faders diferentes. STOP apaga LEVEL; em STATUS a pista parada com frase permanece acesa. Shift+STOP, apenas parada, limpa e apaga todos os modos.
6. Teste overdub e Ctrl+Z na pista explicitamente selecionada. Depois teste Q/L separadamente, observando WAIT REC/PLAY/END e as fronteiras de compasso.
7. Em F5, anote dispositivos, sample rate, buffer e latências reportadas. Use fones e um único caminho de monitoramento. Compare primeiro todos INPUT FX OFF com monitor ON/OFF; depois G2B MODE 1/2. Anote se o atraso ocorre já na fala ao vivo ou somente na repetição gravada, e se o ataque estava exatamente na emenda. Escuta externa/loopback é necessária para medir ida e volta real.

## Resultados de compilação e testes

Validação final em **19/10/2026**. CMake / Visual Studio 2022, plataforma **x64**, configuração **Debug**. Ambos os alvos `MK2Loopstation` e `MK2AudioTests` compilaram com sucesso. Mantida a configuração preexistente que otimiza apenas os arquivos DSP em Debug.

Execução: `ctest --test-dir build -C Debug --output-on-failure` → **1/1 suíte passou, 0 falhas, 151,04 s para AudioDeviceLifecycle (151,14 s total)**. O alvo agregado contém todos os cenários abaixo. Log preservado em [../tmp/6h4-tests.log](../tmp/6h4-tests.log).

- G2B: 55, 82,41, 110, 146,83, 220, 261,63, 329,63 e 440 Hz de entrada, ambos os modos em 44,1/48 kHz; fundamental na oitava inferior. O caso 220 → 110 Hz passou nos dois modos. BALANCE 0/50/100, bypass, espectros distintos, dinâmica, silêncio, transientes, NaN/DC/picos, trocas de parâmetros e blocos 1/480 com 44,1/48/96 kHz passaram. Todos os 16 destinos banco/slot foram exercitados.
- Gravação nova: **10.007 amostras**, todos os cinco loops, entrada estéreo com frequências diferentes por canal, seco e G2B. Comparação de cada amostra de duas repetições com a captura monitorada, incluindo tapers/rampa existentes, tolerância 2e-5; 44,1/48 kHz e blocos 17/257/1024. Sem deslocamento adicional de bloco, perda/duplicação de amostras, reprocessamento na reprodução ou alteração do período nesses cenários.
- Captura de OSC BOT, ROBOT e cadeia 2 OSC/FX SEQUENCE → STEP SLICER → REVERB, reprodução com INPUT FX OFF, overdub/Undo e isolamento do retorno/TRACK FX passaram nas integrações existentes. SINGLE/MULTI, F6–F10 e páginas/encoders foram incluídos na suíte agregada (alguns testes antes estavam disponíveis somente em execuções específicas).
- Transporte: REC → PLAY ↔ DUB, camadas de várias voltas, Undo da última camada, STOP/retomada/limpeza, cinco faders e reprodução simultânea, mono/estéreo, perda/reconexão de dispositivo. REC → DUB direto não é uma opção implementada; não há alegação de validação dessa configuração do hardware.
- QUANTIZE OFF/ON, 96.000/192.000 amostras a 120 BPM/48 kHz, conclusão dentro de um bloco e cancelamento passaram. LOOP SYNC de 1/2/4 compassos, **301 repetições sem deriva**, BPM fracionário, 44,1/96 kHz e proteção das frases nas mudanças de dispositivo passaram.
- Indicadores: vazia, parada com frase, REC, PLAY, DUB; cinco loops de **3/6/12/4/2 s**; fases independentes, wrap, mudança de BPM sem alterar duração, faders, silêncio/forte/fraco, STOP/limpeza/Undo, QUANTIZE/LOOP SYNC e meter após INSERT individual passaram. MENU, primeiro encoder, EXIT e os cinco anéis foram exercitados.
- Concorrência: **50.000 publicações** com leitor em outra thread; **1.968 leituras aceitas** na execução final, sem campos misturados. Publicação usa somente atomics lock-free. Isso não equivale a uma execução com ThreadSanitizer.
- Hooks de alocação do CRT Debug: nenhuma alocação detectada nos caminhos de processamento verificados, depois do aquecimento do scratch interno do AudioDeviceManager do JUCE. Buffers do novo processamento são fixos/pré-preparados; nenhuma espera, mutex, I/O ou acesso à UI foi acrescentado ao callback.
- Visor e painel renderizados e inspecionados: `tmp/indicator-lcd-6h4.png` e `tmp/indicators-panel-6h4.png`. As geometrias anteriores foram preservadas.

### Desempenho medido

Blocos de 256 amostras, medições desta máquina em Debug; não são garantia contra underruns físicos em todo dispositivo. As medições incluem instrumentação de teste, e não somente o algoritmo isolado.

| Cenário | Taxa | Média | p99 registrado | Máximo | Tempo disponível por bloco |
| --- | --- | --- | --- | --- | --- |
| G2B MODE 2, parâmetros variando | 44,1 kHz | 0,0344 ms | 0,0729 ms | 0,3014 ms | 5,8050 ms |
| G2B MODE 2, parâmetros variando | 48 kHz | 0,0332 ms | 0,0655 ms | 0,2054 ms | 5,3333 ms |
| 5 loops + 2 OSC/SEQ + SLICER + REVERB + TRACK FX + medidores | 44,1 kHz | 1,0844 ms | 1,6410 ms | 2,0664 ms | 5,8050 ms |
| Mesmo cenário | 48 kHz | 1,0589 ms | 1,5928 ms | 1,6267 ms | 5,3333 ms |
| Callback com 4 ROBOT ativos | 48 kHz | 1,5631 ms | 2,2658 ms | 4,2478 ms | 5,3333 ms |

O teste legado imprime PASS para controles/envelope ATTACK do OSC BOT. Isso **não resolve** o relato de diferença imperceptível no uso real; a pendência foi preservada sem alterar seu DSP.

## Conclusão do diagnóstico e limites

Foi corrigido o defeito demonstrável de reconstrução do G2B. **Não foi encontrado um erro de posicionamento/cópia na gravação nos cenários testados**, portanto LoopTrack/LoopStorage, duração, ganho global e compensação de latência não foram alterados. O atraso relatado também sem efeitos permanece para medição física; não é correto dizer que foi corrigido por esta etapa.

Sem escuta física com microfone/fones ou comparação com RC-505 MK2 real. G2B continua uma aproximação monofônica; consoantes, beatbox, saltos rápidos de nota e resposta subjetiva precisam de avaliação auditiva. Não foi instalada biblioteca adicional.

## Arquivos alterados nesta revisão

- `Source/G2BEffect.h/.cpp`: alinhamento de cabeças, capacidade e reset do histórico.
- `Source/AudioSystem.h/.cpp`: publicação coerente, níveis individuais, reset G2B em desconexões e informação de latência reportada em F5.
- **Novo** `Source/TrackIndicator.h`: modos, leitura atômica e mapeamento dos indicadores.
- `Source/TrackFxRouting.h`: observação da contribuição individual sem alterar a soma.
- `Source/TrackControls.h/.cpp`, `Source/TrackComponent.h`: segmentos do anel externo e atualização.
- `Source/MainComponent.h/.cpp`: leitura dos snapshots e envio ao painel/visor.
- `Source/TopPanelComponent.h/.cpp`, `Source/MemoryDisplay.h/.cpp`, `Source/DisplaySimulation.h`: página mínima INDICATOR e níveis reais; remoção do timer de simulação do visor.
- `CMakeLists.txt`: inclusão do novo cabeçalho na solução.
- `Tests/AudioSystemTests.cpp`, `Tests/G2BEffectTests.cpp`, `Tests/ReverbEffectTests.cpp`: cenários novos, integração da suíte e correção de pressupostos dos testes antigos.
- Este relatório, `docs/technical-reference.md`, `docs/MK2_BEHAVIOR_REFERENCE.md` e resultado histórico em `docs/STEP_SLICER_6H3D.md`.

Nenhuma implementação da etapa seguinte foi iniciada.

## Pendências preservadas

- ATTACK do OSC BOT sem diferença perceptível: registrado, não corrigido nesta etapa.
- Compensação de latência requer etapa própria com medição física/loopback, timestamps e política por rota de entrada/efeito; não alterar durações para mascarar atraso.
- Escuta com microfone/fones, comportamento em fala/percussão e fidelidade sonora BOSS não são certificados por testes automatizados.
