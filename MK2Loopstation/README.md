# MK2 Loopstation — etapa 6G-B

Aplicativo nativo C++17/JUCE para Windows 10/11 (x64), inspirado na RC-505 MK2. As cinco pistas gravam, reproduzem e fazem overdub independentemente, com um Undo e fader suavizado por pista. BPM, TAP e quantização inicial estão disponíveis; LPF A e DISTORTION C continuam exclusivos da pista 1. LOOP SYNC MEASURE está disponível; sem TEMPO SYNC, persistência entre execuções ou INPUT FX real.

## LOOP SYNC MEASURE — etapa 6G-B

Pressione **L** com a janela principal ativa para alternar LOOP SYNC nas cinco pistas (inicialmente OFF). O LCD indica **LS ON/OFF MEASURE** para a pista selecionada. Com ON, principal agenda REC/PLAY na próxima barra; **WAIT REC/PLAY** sinaliza a espera. STOP cancela. Encerrar REC aguarda compassos inteiros; retomada começa do início da frase na próxima barra, sem parar as demais.

A **120 BPM**, conte a partir do REC efetivo: solicite PLAY antes de 2 s na pista 1, entre 2–4 s na pista 2 e entre 6–8 s na pista 3 para obter 1/2/4 compassos. Teste também 4/5, overdub, Undo e faders. **Q é independente**: conserva sua função anterior quando LOOP SYNC está OFF; em ON, MEASURE governa o início/fim da captura.

Sem TEMPO SYNC, mudar o BPM com frases sincronizadas é bloqueado (**BPM LOCK: L OFF**). Frases antigas incompatíveis não são alteradas: **INCOMPAT: L OFF** orienta reproduzir livremente. Ativar ON durante uma frase livre tocando pede **STOP FIRST**. Algumas trocas de sample rate impedem novas capturas sincronizadas (**RATE GRID: L OFF**), preservando frases existentes. [Regras, precisão, arquivos e testes da 6G-B](docs/technical-reference.md#etapa-6g-b-loop-sync-modo-measure).

## Testar as cinco pistas

1. Abra **F5**, escolha microfone e saída e use fones. Monitoramento inicia desligado e não é necessário para gravar.
2. Em qualquer pista vazia, principal inicia **REC**; novamente inicia **PLAY**. Depois, cliques alternam **PLAY ↔ OVERDUB**. Grave outras pistas enquanto as anteriores tocam; cada fader controla somente seu loop.
3. **STOP** interrompe só a própria pista; principal retoma. **Shift + STOP**, somente parada, limpa apenas essa pista.
4. Para desfazer, clique em **TRACK** da pista desejada, confira **T1…T5** no visor e pressione **Ctrl+Z**. Só o último overdub concluído dessa pista é desfeito, inclusive várias voltas. Sem Undo disponível, nenhuma outra pista é afetada. Principal/STOP também selecionam a pista acionada; faders e atualizações automáticas não mudam o alvo.
5. **Q** alterna QUANTIZE (inicialmente OFF). A 120 BPM, solicite PLAY antes de 2 s para obter um compasso, entre 2–4 s para dois. **WAIT** conserva REC até a fronteira relativa ao início daquela captura. Selecione TRACK para conferir a espera da pista desejada. Com L OFF não há sincronização entre os loops; com L ON vale MEASURE.
6. **TAP TEMPO** abre andamento, ajustável pelo knob 4; Shift+rodinha ajusta décimos. Mudanças de BPM não alteram loops existentes.

RAM pré-alocada: aproximadamente **881 MiB** para cinco loops estéreo de até 60 s a 192 kHz, incluindo um Undo por pista. Falha de memória impede abrir o áudio e mostra um aviso. A mixagem mantém ganho unitário em níveis normais e limita suavemente somas altas, preservando a relação estéreo. Consulte [detalhes e validação da 6G-A](docs/technical-reference.md#etapa-6g-a-cinco-pistas-com-áudio-independente).

## Testar TRACK FX C / DISTORTION

1. Grave um loop na pista 1 e deixe em PLAY, com seu fader acima de zero. Para comparar somente o loop, deixe o monitoramento desligado em F5 e LPF A desligado. Use fones.
2. Clique em **C na seção TRACK FX, à direita**. O LED e o indicador ON/OFF do visor comandam o mesmo estado enviado ao DSP.
3. Clique em **TRACK FX EDIT**. O cabeçalho mostra **FX A-C DISTORTION**. Os quatro knobs abaixo do visor, da esquerda para a direita, controlam os campos indicados; arraste ou use a rodinha.

| Knob | Parâmetro (0–100) | Comportamento | Inicial |
| --- | --- | --- | --- |
| 1 | DRIVE | Ganho antes da saturação; aumente para mais harmônicos/distorção. | 60 |
| 2 | TONE | Baixo: som abafado. Alto: mais agudos no som distorcido. | 65 |
| 3 | LEVEL | Volume somente do ramo distorcido. | 50 |
| 4 | MIX | 0 = sinal anterior a C; 100 = totalmente distorcido. | 100 |

4. Alterne C para comparar com o som sem distorção. Com LEVEL 0 e MIX 100, C fica ligado mas sua saída fica silenciosa; MIX 0 conserva o sinal seco. Para uma comparação audível, comece pelos valores iniciais acima.
5. EXIT retorna ao visor anterior. Reabrir conserva os quatro valores. As setas no editor selecionam **A (esquerda)** e **C (direita)** sem mudar ON/OFF; clicar A/C continua alternando o respectivo efeito e selecionando-o para edição.
6. Ative também A para combinar os dois: o loop passa por LPF A → DISTORTION C → fader 1. Desligar C conserva o LPF se A ainda estiver ligado. B/D continuam apenas visuais.

O áudio original em RAM e o monitoramento do microfone não passam pelo DISTORTION. `juce::dsp::WaveShaper` aplica saturação tanh, seguida de filtro de tonalidade não ressonante; LEVEL limita o pico processado a 0,5 (0,25 no valor inicial). DRIVE corresponde a ganho 2–64; TONE a 500–12.000 Hz, limitado a 45% da taxa de amostragem. Há suavização de 40 ms nos parâmetros e 20 ms no bypass. Esses parâmetros são próprios desta etapa, sem emulação exata do algoritmo BOSS ou menus completos de TRACK FX.

Validação: Debug x64 compilado, testes de DSP/callback e controles pela interface aprovados. Uma senoide de 750 Hz/pico 0,1 gerou terceiro harmônico de aproximadamente 11,6% da fundamental com os padrões de C, demonstrando alteração da forma de onda, e o bypass restaurou exatamente o áudio original. A versão recebida já tinha roteamento funcional no teste; o primeiro algoritmo tinha saturação fraca em sinais baixos, enquanto a revisão posterior ainda não oferecia edição de parâmetros. A ausência de diferença relatada não foi reproduzida com o sinal controlado.

## Testar TRACK FX A / LPF na pista 1

1. Grave e reproduza um loop na pista 1 conforme os passos abaixo. Use fones e ajuste seu fader para um volume confortável.
2. Clique em **A na seção TRACK FX**, à direita do painel. O LED indica o LPF ligado; clicar novamente desliga, retornando ao áudio original.
3. Clique no botão circular **EDIT de TRACK FX**. O visor mostra LPF, ON/OFF, TRACK 1, CUTOFF e RESONANCE.
4. Arraste o **primeiro knob abaixo do visor** ou use a rodinha para ajustar **CUTOFF (0–100)**. Reduzir o valor deixa o loop mais abafado.
5. Use o **segundo knob** para **RESONANCE (0–100)**. O ganho é compensado ao aumentar a ressonância, portanto o volume pode diminuir. Knobs 3–4 ficam inativos nesta tela.
6. Compare ligado/desligado usando TRACK FX A, inclusive com o editor aberto. **EXIT** retorna à tela anterior. Os valores são preservados ao sair/reabrir, parar ou retomar o loop.

O LPF inicia desligado, com CUTOFF 70 e RESONANCE 0. Somente a reprodução da pista 1 é filtrada: o áudio armazenado, a gravação e o retorno direto do microfone permanecem independentes. INPUT FX, TRACK FX B/D e os botões FX individuais das pistas continuam apenas visuais. O editor tem apenas as páginas dos efeitos fixos A/C; não há seleção de outros tipos, bancos ou destinos.

O processamento usa `juce::dsp::StateVariableTPTFilter`, passa-baixas de segunda ordem. A escala CUTOFF é convertida exponencialmente em 40–18.000 Hz, limitada a 45% da taxa de amostragem; RESONANCE mapeia Q de 0,5 a 2,0 com compensação de ganho. Essas conversões são escolhas do protótipo, não uma reprodução do algoritmo BOSS. Parâmetros têm rampa de 40 ms e ON/OFF faz uma transição de 20 ms. Há proteção numérica e limite de pico no sinal filtrado; o bypass estabilizado conserva exatamente as amostras originais. Os parâmetros duram somente durante a execução.

## Testar a pista 1

1. Abra as configurações com **F5** e selecione microfone e saída. Use fones. O monitoramento pode permanecer desligado: ele não é necessário para gravar.
2. Com a pista 1 vazia, clique no botão circular: **REC vermelho**. Fale por alguns segundos.
3. Clique novamente: **PLAY verde**, repetindo a gravação. Cliques adicionais alternam PLAY e OVERDUB, preservando a duração original.
4. Arraste o fader da pista 1 para ajustar somente o volume do loop. O volume capturado e o retorno direto do microfone não são alterados.
5. Clique em **STOP 1**. O áudio permanece em RAM. Clique no botão circular para reproduzi-lo novamente desde o início.
6. Para gravar outro trecho, pare a pista e use **Shift + clique em STOP 1**. O loop é apagado e a pista volta a vazia. Shift + STOP durante REC ou PLAY é ignorado; manter STOP pressionado não apaga.

STOP durante REC encerra a captura e conserva o trecho parado. Uma captura com zero amostras retorna a vazia; silêncio efetivamente capturado é uma gravação válida. Sem entrada disponível, uma pista vazia não entra em REC. Um loop existente pode tocar com apenas a saída conectada.

O limite é **60 segundos** nas taxas de até 192 kHz. Ao atingir a capacidade, a gravação termina e entra em PLAY, ou permanece parada se não houver saída. Há duas versões estéreo pré-alocadas de 11.520.000 frames por pista (aproximadamente 176 MiB por pista com Undo; 881 MiB no conjunto com índices); em taxas acima de 192 kHz, a duração máxima diminui para respeitar essa capacidade. O conteúdo dura somente nesta execução: encerrar o aplicativo o descarta.

Os comandos são aplicados nos limites dos blocos de áudio. O período do loop corresponde exatamente às amostras capturadas, com duração livre em QUANTIZE OFF ou compassos completos em ON, sem blocos vazios acrescentados entre voltas. Pequenas rampas reduzem estalos: 20 ms no fader, 5 ms no início/parada e até 3 ms nas extremidades do loop, sem alterar sua duração. Não há garantia de emenda musical perfeita em qualquer ponto de corte.

Trocas, falhas ou reinícios de dispositivos param as cinco pistas e preservam os trechos capturados. A retomada é manual. Se a taxa de amostragem mudar, a reprodução usa interpolação linear para conservar a duração da gravação; não é criada outra cópia nem apagado o áudio.

## Configurações de áudio

Com a janela da Loopstation ativa, pressione **F5** para abrir **Configurações de áudio**. Se já estiver aberta, a mesma janela vem para frente. O atalho é tratado pelas janelas do aplicativo, sem registro global ou captura de teclas de outros programas. O **botão direito na borda vazia lateral do painel**, fora dos controles, continua abrindo a janela. Nenhum botão ou desenho foi acrescentado ao painel.

- Escolha **Microfone / entrada** e **Saída de áudio** nas listas. A conexão é aplicada imediatamente; **Nenhum** desativa aquele lado.
- A janela mostra as escolhas e o estado da conexão. **Reconectar / atualizar dispositivos** atualiza as listas e tenta abrir novamente os dispositivos selecionados.
- Se um dispositivo desaparecer, seu nome fica marcado como **indisponível**. A conexão é fechada e a escolha é preservada; reconecte e clique em Reconectar, ou escolha outro dispositivo. Erros de abertura também aparecem no estado, inclusive quando o Windows impede o acesso ao microfone.
- Fechar a janela de configurações mantém a conexão e o estado atual do monitoramento. Fechar o aplicativo libera os dispositivos. As escolhas duram somente durante a execução; na próxima abertura, são usados os dispositivos padrão do Windows.

O backend continua **WASAPI compartilhado**, com o mesmo `JUCE AudioDeviceManager`, até duas entradas e duas saídas, limitadas aos canais disponíveis no dispositivo. As listas apresentam os endpoints que o Windows disponibiliza ao aplicativo. Cada fader controla somente seu loop real. Nenhum deles controla o retorno direto do microfone.

### Monitoramento opcional

O controle **Monitorar microfone** começa desligado. O **Volume do retorno** começa em **10% (−20 dB)** e vai de 0 a 100%, sem amplificação acima do sinal de entrada. Use fones de ouvido para evitar microfonia; um volume baixo não elimina o risco com alto-falantes.

- Ao ativar, o áudio da entrada selecionada vai diretamente para a saída selecionada. Mono é duplicado em L/R; estéreo preserva L/R. Saída mono recebe a média dos dois canais, sem somá-los com ganho extra.
- Ativação e mudanças de volume usam uma rampa de 20 ms para reduzir estalos. Desativar silencia o retorno direto no próximo bloco, sem parar o loop. Sem entrada válida, o retorno direto recebe zeros.
- Trocar dispositivo, reconectar, reiniciar o stream ou receber falha/desconexão desliga o monitoramento. Ele precisa ser reativado manualmente. Sem entrada e saída conectadas, o controle fica indisponível.
- O buffer é escolhido entre os tamanhos suportados, buscando o menor a partir de aproximadamente 5 ms, sem aumentar um padrão que já seja menor. A taxa de amostragem negociada é preservada. Se a abertura falhar, o buffer original é restaurado. Se forem detectadas três interrupções em uma janela de aproximadamente dois segundos com buffer reduzido, a aplicação retorna ao buffer original e desliga o monitoramento. Drivers que não informam interrupções limitam essa detecção.

O tempo do buffer não representa a latência total de ida e volta: os dispositivos e o Windows também contribuem. O monitoramento não passa pelo buffer de gravação nem ganha uma fila adicional.

`AudioSystem` concentra a conexão e o ciclo de vida dos dispositivos; `AudioSettingsWindow` contém a interface de configuração e monitoramento. O callback implementado usa somente controles atômicos sem bloqueio, suavização de ganho e operações por amostra: não aloca memória, bloqueia, acessa arquivos ou atualiza componentes. A redução/restauração de buffer ocorre na thread de interface.

Os testes isolados podem ser compilados com `cmake --preset vs2022 -DMK2_BUILD_AUDIO_TESTS=ON`, `cmake --build build --config Debug --target MK2AudioTests` e executados com `ctest --test-dir build -C Debug --output-on-failure`. Usam dispositivos simulados e sinais controlados para verificar falhas, reconexão, canais mono/estéreo, ganho, rampas, silenciamento, ausência de entrada, recuperação de buffer e encerramento, sem alterar os dispositivos do Windows. Também verificam duração exata e repetições do loop, limites, limpeza protegida, retomada, mudança de taxa e mistura independente com o monitoramento. Em Debug/MSVC, um hook detecta alocações durante o processamento de `LoopTrack`.

## Edição visual dos slots de INPUT FX (banco A)

Clique em **INPUT FX EDIT**. Dentro do editor, clique em **A/B/C/D** para selecionar o slot, sem alterar seu ON/OFF. O cabeçalho identifica banco e slot: **FX A-A**, **FX A-B**, **FX A-C** ou **FX A-D**. Na primeira página, arraste o **knob 1 (FX TYPE)** ou use a rodinha para selecionar **LPF → BPF → HPF → PHASER**, na ordem do Parameter Guide. Os knobs 2–4 controlam RATE, DEPTH e RESONANCE. Use as setas físicas abaixo de EXIT/ENTER para mudar a página de parâmetros.

| Efeito | Página 1 — knobs 1–4 | Página 2 — knobs 1–4 | Página 3 |
| --- | --- | --- | --- |
| LPF, BPF, HPF | FX TYPE, RATE, DEPTH, RESONANCE | CUTOFF, STEP RATE, vazio, vazio | — |
| PHASER | FX TYPE, RATE, DEPTH, RESONANCE | MANUAL, STEP RATE, D.LEVEL, E.LEVEL | STAGE no knob 1 |

Cada knob controla somente o campo acima dele; campos vazios ficam inativos. As setas do cabeçalho indicam as páginas disponíveis, e a navegação para nos extremos. O visual do painel físico permanece igual.

Os parâmetros numéricos usam valores inteiros de **0 a 100**. STEP RATE aceita **OFF** e **0–100**; STAGE aceita **4, 8, 12, BI-PHASE**. A atualização do visor é imediata. RATE mantém a faixa numérica anterior e o valor de exemplo 50; notas/compassos não foram implementados em RATE ou STEP RATE. Os demais padrões seguem o guia: DEPTH/RESONANCE/CUTOFF/MANUAL/E.LEVEL = 50, D.LEVEL = 100, STEP RATE = OFF, STAGE = 8.

Cada slot guarda seu tipo selecionado e valores independentes para cada um dos quatro tipos. Isso permite usar o mesmo efeito em vários slots com configurações diferentes e recuperar seus valores ao alternar tipo ou slot. Os dados são mantidos durante a execução, sem gravação em disco.

Ao trocar de slot, o editor mostra a primeira página desse slot; clicar no slot já selecionado mantém a página. EXIT e reabertura preservam o último slot escolhido. Fora da edição, A/B/C/D voltam a alternar ON/OFF como antes e não mudam o slot selecionado. Os LEDs continuam indicando ON/OFF durante a edição; o slot selecionado é identificado no visor.

As atribuições iniciais de INPUT FX usam somente os tipos disponíveis: **A = LPF, B = BPF, C = HPF, D = PHASER**, todos OFF. A tela de reprodução INPUT FX mostra o tipo configurado em cada coluna. Somente o banco A é editável; esses parâmetros são independentes do LPF real de TRACK FX.

**EXIT** retorna à tela de reprodução anterior, de qualquer página. Reabrir INPUT FX EDIT mostra a primeira página do tipo selecionado; EDIT repetido durante a edição mantém a página atual. Fora dos editores, os quatro knobs ficam inativos. INPUT FX continua somente visual; o processamento real é exclusivo de TRACK FX A / LPF e C / DISTORTION descritos acima. Não há edição de outros bancos, configuração completa de TRACK FX, Modo Fácil ou WRITE funcional. F5 abre somente as configurações de áudio.

## Visor funcional

As duas setas físicas abaixo de EXIT/ENTER percorrem **MEMORY NUMBER → TRACK STATUS → LOOP TRACKS → LOOP STATUS → LOOP LEVEL → INPUT FX → TRACK FX**, na ordem das sete telas do manual, com retorno circular. A seta esquerda faz o percurso inverso. A inicialização mostra a memória **01 / Memory01 / 120.0 BPM**. Número/nome permanecem fixos; BPM é editável por TAP/knob 4.

A tela de status apresenta cinco ícones na ordem das pistas: preenchido para REC/PLAY/OVERDUB, contorno para parado com frase, e traço estreito para vazio. O ponto central identifica a pista atual quando o ícone contém frase. Nas cinco pistas, estado e presença da frase vêm do motor de áudio; LEDs e visor são atualizados pela interface a 30 Hz.

- **LOOP TRACKS**: os mesmos cinco ícones, com preenchimento por setores para mostrar o progresso real de cada pista ativa. Pistas paradas mostram contorno; pistas vazias mostram traço. O marcador identifica a pista atual.
- **LOOP STATUS**: mostra a pista atual e seu estado em letras grandes, por exemplo `TRK5: PLAY`. Os estados são REC, PLAY, OVERDUB e STOP. A pista atual é selecionada por TRACK, principal ou STOP; esse mesmo alvo é usado por Ctrl+Z.
- **LOOP LEVEL**: cinco medidores com duas barras e indicadores de pico, conforme a ilustração. Os níveis são simulados nas pistas ativas e acompanham os cinco sliders existentes. STOP, pista vazia ou volume zero deixam o respectivo medidor em zero.

Somente os medidores de nível permanecem simulados por timer de interface. Estado, duração e posição vêm do áudio das cinco pistas; não há ciclo visual de transporte nem progresso de oito batidas. STOP zera a posição visual, preservando a frase.

Layout e ícones seguem a página 8 do Manual do Proprietário. Os dois PDFs oficiais estão preservados em [references](references/); as páginas consultadas e as adaptações da simulação estão documentadas na [referência técnica permanente](docs/technical-reference.md).

As telas **INPUT FX** e **TRACK FX** apresentam quatro efeitos de exemplo, em colunas A/B/C/D da esquerda para a direita. O nome sobre retângulo claro indica **ON**; texto claro sobre o fundo azul indica **OFF**, conforme a ilustração oficial. Fora da edição de INPUT FX, os oito botões físicos A/B/C/D alternam seus respectivos indicadores, inclusive enquanto outra tela de reprodução está visível, sem mudar a página atual. Os dois grupos são independentes e começam em OFF.

| Tela | A | B | C | D |
| --- | --- | --- | --- | --- |
| INPUT FX (inicial) | LPF | BPF | HPF | PHASER (PHA/SER) |
| TRACK FX | LPF (real, pista 1) | FLANGER (FLN/GER) | DISTORTION (DIST/ORT, real, pista 1) | GRANULAR DELAY (GRN/DLY) |

As abreviações e quebras de linha seguem o estilo do visor do manual. O sufixo A em INPUT FX-A / TRACK FX-A é uma legenda fixa do banco ilustrado. Não há troca de banco. Os quatro slots de INPUT FX têm seleção e edição visual; TRACK FX A/C têm somente a edição mínima de LPF e DISTORTION reais.

## Comportamento visual

- Cada principal começa em **STOP/BLANK**, com anéis apagados. As cinco pistas seguem **REC (vermelho) → PLAY (verde) ↔ OVERDUB (âmbar)**, conforme o motor real.
- **STOP** para e preserva o loop da própria pista; principal retoma PLAY. **TRACK** indica presença de frase e seleciona o alvo de Ctrl+Z.
- INPUT FX EDIT/TRACK FX EDIT indicam edição; EXIT retorna. A/C de TRACK FX controlam LPF/DISTORTION da pista 1. TAP pulsa com o relógio musical real e abre o ajuste de BPM.
- ALL START/STOP, o botão UNDO/REDO, os FX inferiores, START/STOP de RHYTHM, MENU, LOOP, ENTER e WRITE mantêm o comportamento visual anterior. Ctrl+Z é o comando de Undo implementado; não há novos menus ou controles globais.
- Estados e volumes não são salvos entre execuções.

Arraste o fader com o mouse ou clique no trilho para posicioná-lo. O valor inicial é 88%, a faixa é de 0 a 100% e não há persistência entre execuções. Os cinco faders atuam nas respectivas reproduções reais, com suavização. A inscrição CLEAR: HOLD foi preservada no desenho, mas a limpeza real nesta etapa usa Shift + STOP da respectiva pista, somente parada.

O painel superior inclui os dois knobs de efeitos, grupos A/B/C/D, transportes, MENU/LOOP, EXIT/ENTER/WRITE, OUTPUT LEVEL, quatro knobs centrais e tela azul. Somente os quatro knobs centrais são operáveis, enquanto o editor está aberto; os demais continuam decorativos. A legenda MFX REVERB não implementa um efeito.

O painel conserva sua proporção ao redimensionar; espaço excedente fica preenchido pelo fundo escuro. Os cinco sliders continuam independentes e arrastáveis.

## Compilar e executar no Visual Studio 2022

1. No Visual Studio Installer, instale **Desenvolvimento para desktop com C++**, incluindo MSVC v143, Windows SDK e **Ferramentas C++ do CMake para Windows** (CMake 3.24 ou superior).
2. Abra o **Developer PowerShell for VS 2022** e navegue até a pasta que contém este README e o `CMakeLists.txt`.
3. Gere a solução:

   ```powershell
   cmake --preset vs2022
   ```

   A primeira configuração baixa o JUCE **8.0.6** e precisa de internet. A dependência permanece em `build/_deps`.

4. Abra `build/MK2Loopstation.sln` no Visual Studio 2022.
5. Selecione **Debug | x64** e defina **MK2Loopstation** como projeto de inicialização, caso necessário.
6. Pressione **F5** para compilar e executar.

Também é possível compilar e executar pelo terminal:

```powershell
cmake --build --preset debug
& '.\build\MK2Loopstation_artefacts\Debug\MK2 Loopstation.exe'
```

Para Release, use `cmake --build --preset release`; o executável ficará na subpasta `Release`.

## Organização

- `Source/Main.cpp`: ciclo de vida do aplicativo e janela nativa.
- `Source/AudioSystem.*`: seleção, conexão, falhas e encerramento dos dispositivos; callback de monitoramento opcional e ajuste de buffer.
- `Source/AudioSettingsWindow.*`: janela independente de configurações de áudio.
- `Source/LoopTrack.*`: buffer pré-alocado, transporte REC/PLAY/OVERDUB/STOP, Undo e quantização inicial, reprodução contínua, ganho suavizado e comandos/snapshots atômicos; cinco instâncias independentes são conectadas ao mesmo callback.
- `Source/TrackEffects.*` e `Source/TrackFxParameters.h`: insert reutilizável por pista, LPF e DISTORTION JUCE DSP, controles atômicos, suavização, bypass e limites seguros. Apenas TRACK FX A/C da pista 1 estão ligados ao áudio.
- `Tests/LoopTrackTests.cpp`: duração exata, emendas, limites, limpeza, retomada, troca de taxa e verificação de ausência de alocações no processamento.
- `Tests/AudioSystemTests.cpp`: testes de ciclo de vida com backend de dispositivos simulado.
- `Source/MainComponent.*`: desenho e proporções do painel principal.
- `Source/TopPanelComponent.*`: disposição e inscrições do painel superior.
- `Source/PanelControls.*`: knobs e botões reutilizáveis do painel superior; `ParameterKnob` usa o slider rotativo JUCE e preserva o acabamento de `PanelKnob`.
- `Source/DisplayModel.h` e `Source/MemoryDisplay.*`: modelo, navegação e renderização das sete telas do LCD, estados das pistas e dos oito indicadores de efeitos.
- `Source/InputFxEditor.h`: campos, páginas e valores independentes por slot e tipo, no banco A de INPUT FX.
- `Source/DisplaySimulation.h`: posições reais de áudio e animação dos níveis ainda simulados, separadas dos estados e do desenho do visor.
- `Source/VisualStyle.*`: iluminação dos LEDs, textura e desenhos dos números e caracteres do display.
- `Source/TrackComponent.*`: composição e proporções reutilizadas nas cinco pistas.
- `Source/TrackControls.*`: botões desenhados em JUCE e slider de volume com aparência própria.
- `CMakeLists.txt` e `CMakePresets.json`: dependência JUCE e configuração do Visual Studio 2022.

São utilizados os módulos `juce_gui_basics`, `juce_audio_devices`, `juce_dsp` e suas dependências. O desenho é feito com a API gráfica do JUCE, sem imagens externas ou navegador.

Referência de configuração: [documentação oficial de CMake do JUCE 8.0.6](https://github.com/juce-framework/JUCE/blob/8.0.6/docs/CMake%20API.md).


### Etapa 6H — roteamento dos TRACK FX

FX de cada pista agora habilita sua participação nos efeitos reais. TRACK abre a configuração FX da pista e mantém a seleção de Undo; selecionar uma pista não redireciona A/C.

Para escolher o destino: **TRACK FX EDIT → selecione A/C pelas setas → ENTER → knob 1: INSERT ALL ou TRACK1–5**. EXIT volta aos parâmetros; outro EXIT sai. Os botões superiores A/C continuam ligando/desligando LPF e DISTORTION compartilhados. Ambos começam em TRACK1 para preservar o comportamento anterior. Os parâmetros existentes não mudaram. Veja escopo, topologia e validação em [docs/technical-reference.md](docs/technical-reference.md#etapa-6h-fx-por-pista-e-insert-compartilhado).
