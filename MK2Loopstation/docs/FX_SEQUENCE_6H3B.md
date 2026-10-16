# Etapa 6H.3B — FX SEQUENCE / OSC BOT

## Referências e limites de fidelidade

Consultados `references/RC-505mk2_Parameter_eng04_W.pdf`, páginas 5, 34 e 38, e `MK2_BEHAVIOR_REFERENCE.md`. A tabela e os símbolos musicais da p.34 foram inspecionados na imagem do PDF. [Cópia oficial BOSS](https://static.roland.com/assets/media/pdf/RC-505mk2_Parameter_eng04_W.pdf).

Confirmado pelo guia: SW/SYNC/RETRIG inicialmente OFF; TARGET depende do efeito (NOTE é o alvo inicial de OSC BOT); RATE determina o ciclo **de cada passo**; MAX 1–16, inicialmente 16. RATE oferece 0–100, 4MEAS, 2MEAS, 1MEAS e figuras de mínima a fusa; semínima é o padrão. MULTI permite vários slots ligados. Os parâmetros do OSC BOT permanecem os da etapa 6H.3A.

Não se declara reprodução integral do algoritmo proprietário. O guia não fornece a curva temporal de RATE numérico, os valores iniciais de cada VAL para NOTE, a política de referência entre frases de durações diferentes nem a topologia DSP interna de vários sintetizadores. Decisões locais explicitadas abaixo precisam de comparação com hardware. Não foram acrescentadas observações do vídeo.

## Parâmetros e páginas do LCD

INPUT FX EDIT → A/B/C/D seleciona o slot sem alternar seu ON/OFF. Knob 1 da primeira página escolhe OSC BOT. As setas existentes percorrem oito páginas; cada coluna corresponde a um encoder, com arraste/rodinha. EXIT retorna à reprodução.

| Página | Encoder 1 | Encoder 2 | Encoder 3 | Encoder 4 |
|---|---|---|---|---|
| 1 | FX TYPE | OSC | TONE | ATTACK |
| 2 | NOTE | MOD SENS | BALANCE | — |
| 3 | SW | SYNC | RETRIG | TARGET: NOTE (fixo) |
| 4 | RATE | MAX | INPUT MODE: SINGLE/MULTI | — |
| 5 | VAL1 | VAL2 | VAL3 | VAL4 |
| 6 | VAL5 | VAL6 | VAL7 | VAL8 |
| 7 | VAL9 | VAL10 | VAL11 | VAL12 |
| 8 | VAL13 | VAL14 | VAL15 | VAL16 |

O cabeçalho FX SEQ mostra o slot e a página dentro das seis páginas de sequência. TARGET é somente leitura nesta etapa. Todos os VAL usam C1–G9, incluindo semitons, e começam localmente em C2; o manual não especifica um preset melódico inicial. Os valores além de MAX permanecem guardados. Alterar tipo, slot, sair do editor ou desligar o efeito preserva a programação **em RAM**, como as configurações anteriores; não foi adicionada persistência entre execuções.

RATE aparece como `0`…`100`, `4MEAS`, `2MEAS`, `1MEAS`, `1/2`, `1/4`, `1/8`, `1/16`, `1/32`. Em 4/4, são respectivamente 16, 8, 4, 2, 1, 0,5, 0,25 e 0,125 batidas por passo para os valores musicais. A barra é desenhada na mesma fonte pixelada. A curva numérica local é `0,1 × 200^(RATE/100)` passos por segundo (0,1–20 Hz), independente do BPM. **Essa curva não é uma calibração BOSS.** Não foram inventadas divisões pontuadas/quiálteras não enumeradas na tabela consultada.

## Transporte, sincronização e MULTI

- SW OFF devolve a NOTE manual do OSC BOT, com a suavização existente. SW ON inicia a programação no passo 1. Apenas o alvo temporário do DSP muda; a NOTE manual não é sobrescrita.
- MusicalClock fornece o incremento musical e temporal do mesmo snapshot de BPM/sample rate usado pelos loops. O sequenciador usa essa referência por amostra; não há Timer gráfico nem segundo relógio de tempo. Mudanças de BPM preservam a fração do passo. RATE musical segue BPM mesmo com SYNC OFF: SYNC aqui significa vínculo com a frase, não uma opção de sincronização ao BPM.
- Com SYNC OFF, a sequência percorre os passos continuamente, inclusive enquanto o botão do efeito está OFF. O botão continua controlando o áudio. Com RETRIG OFF, reativar revela a posição corrente.
- Com SYNC ON, início/retomada e cada retorno ao começo da frase de referência posicionam a sequência no passo 1. A referência local é a pista de menor número atualmente em PLAY/OVERDUB; mudança dessa referência também dá cue. Sem reprodução, a sequência continua livre para uso ao vivo. Nenhum loop é reposicionado, redimensionado ou tem sua velocidade alterada.
- RETRIG ON: a borda OFF→ON do efeito alinha a fase da sequência à posição atual da frase de referência, em vez de deslocar a frase. Sem frase, reinicia no passo 1. Para RATE musical, usa a posição em segundos convertida pelo andamento atual. Se o BPM mudou durante uma frase livre, isso **não reconstrói retrospectivamente** os BPM anteriores; é uma limitação da aproximação. A interpretação de SYNC em cada volta, a prioridade de referência e a relação de RETRIG com frases livres precisam de validação no equipamento.
- Cada slot possui seu próprio estado e MAX. As sequências de 9 e 6 passos permanecem independentes, embora recebam o mesmo relógio. Ao trocar/reinicializar dispositivo, o DSP reinicia de forma segura; a configuração permanece.
- INPUT MODE é compartilhado pelo banco A. MULTI preserva o comportamento anterior de vários botões ligados (por isso é o estado inicial de compatibilidade do aplicativo). SINGLE mantém apenas um ligado; mudar para SINGLE conserva o selecionado, se ativo, ou o primeiro ativo. Retornar a MULTI não liga efeitos automaticamente.
- Em MULTI, OSC BOT **adjacentes** e ativos recebem a mesma excitação e são combinados por mistura convexa, com pesos de bypass suavizados. Assim A e B em BALANCE 100 são ambos ouvidos, sem o segundo substituir a nota do primeiro e sem duplicar o retorno seco. Os demais processadores mantêm a ordem dos slots; um ROBOT ativo ou em cauda interrompe o agrupamento. Esta mistura é uma escolha local para sintetizadores simultâneos, **não uma afirmação sobre o roteamento interno da MK2**. OSC separados por outros slots não formam esse mesmo grupo paralelo.

Não foram adicionados bancos: permanece o banco A já implementado. A edição/ativação dos demais efeitos foi preservada. Sequenciamento de outros TARGET/efeitos não está disponível.

## Configurar o exemplo solicitado

1. Ajuste 165,0 BPM pelo TAP TEMPO/encoder de BPM existente. Se LOOP SYNC tiver travado o andamento por frases vinculadas, use o fluxo anterior para liberar a referência.
2. INPUT FX EDIT → botão A → FX TYPE OSC BOT. SAW, TONE 0, ATTACK 50; na segunda página NOTE C2, MOD SENS 0, BALANCE 100.
3. Na terceira página: SW ON, SYNC OFF, RETRIG OFF, TARGET NOTE. Na quarta: RATE 2MEAS, MAX 9, INPUT MODE MULTI.
4. Preencha VAL1–9 na tabela abaixo.
5. Ainda no editor, selecione B e escolha OSC BOT **explicitamente**. Isso não acontece na inicialização. Se B estiver com ROBOT, seus valores continuam guardados para voltar a esse tipo. Configure SAW, TONE 0, ATTACK 50, NOTE C5, MOD SENS 0, BALANCE 100. SW ON, SYNC OFF, RETRIG OFF, RATE 2MEAS, MAX 6. Preencha VAL1–6.
6. EXIT e ligue os botões A e B. Em F5, mantenha a saída nos fones e habilite o monitoramento. Faça sons sustentados no microfone: o envelope da entrada excita os osciladores; silêncio não vira um sintetizador contínuo autônomo. O tempo da sequência continua durante pausas da voz.

| Passo | A | B |
|---|---|---|
| 1 | C4 | Ab5 = G#5 |
| 2 | Bb3 = A#3 | G5 |
| 3 | Db4 = C#4 | Bb5 = A#5 |
| 4 | F4 | Ab5 = G#5 |
| 5 | Eb4 = D#4 | Bb5 = A#5 |
| 6 | F4 | Db6 = C#6 |
| 7 | Ab4 = G#4 | — |
| 8 | G4 | — |
| 9 | Bb4 = A#4 | — |

O LCD conserva a grafia com sustenidos do projeto. A 165 BPM, cada passo de 2MEAS dura aproximadamente **2,90909 s**; A completa seu ciclo em 26,1818 s e B em 17,4545 s. Os exemplos existem nos testes e nesta documentação, não são carregados automaticamente no produto.

Para começar os dois padrões juntos com os loops parados: configure RETRIG ON em ambos e ative os dois botões próximos um do outro (cada acionamento é recebido no próximo callback; não há comando novo para ativação simultânea). Para alinhá-los à reprodução existente, experimente SYNC/RETRIG conforme as regras e limitações acima. A igualdade com o arranjo da música não foi validada.

## Segurança e validação

`FxSequence` utiliza memória fixa por slot. A interface publica três palavras atômicas e um contador de versão; o áudio faz uma única tentativa limitada de leitura e conserva o snapshot anterior se houver edição concorrente. Não há mutex, espera ativa, alocação ou I/O. Há apenas cálculo limitado nas fronteiras e atualização do alvo de frequência quando a nota muda. Os osciladores conservam fase e suavização de 20 ms, sem reiniciar o envelope a cada passo. O bypass e o ganho da mistura continuam suavizados.

`LoopTrack` exporta apenas observações de posição/fronteira e um contador de início/volta. `AudioSystem` divide seus blocos fixos também nessas fronteiras quando SYNC precisa delas; comandos e estados permanecem no motor existente. Monitoramento recebe o mesmo sinal processado usado na captura (respeitando FX por pista). Reprodução e TRACK FX continuam fora da cadeia INPUT FX.

Compilação de aplicativo e testes **Visual Studio 2022 / Debug x64 concluída**, sem warnings no log final. CTest: **1/1 aprovado, 40,97 s** (a entrada agrega os testes abaixo e a suíte existente).

- Presets A/MAX9 e B/MAX6 a 165 BPM/2MEAS: ordem exata, fronteiras em amostras e retorno ao primeiro passo em 44,1/48 kHz, com blocos de 257 amostras e um ciclo completo de A mais um passo.
- SW, SYNC, RETRIG, mudança de BPM preservando fração do passo; durações de todos os RATE musicais oferecidos e RATE numérico 0/50/100 conforme a curva local; MAX=1; edição independente de VAL1–16.
- Sinal determinístico: a sequência muda realmente a frequência do OSC em uma oitava, medida por cruzamentos de zero, mantendo amostras finitas e pico limitado.
- Callback real do aplicativo com dispositivo simulado: A isolado, B isolado e A+B. A soma MULTI coincide com a mistura esperada das duas vozes independentes; nenhuma desaparece nem duplica o microfone. Captura simultânea nas cinco pistas e reprodução individual com INPUT FX OFF coincidem com o sinal processado capturado.
- Cue de SYNC na primeira amostra de PLAY e na volta do loop, REC livre/QUANTIZE/LOOP SYNC, STOP/retomada e alinhamento RETRIG à frase em reprodução.
- Painel real: páginas/encoders → motor, slots independentes, TARGET fixo, SINGLE/MULTI e LEDs existentes coerentes, EXIT e captura de imagem do LCD inspecionada. Glifo `/` acrescentado para distinguir `1/4` e numeração de página.
- Suíte anterior de ROBOT, OSC BOT, LPF/DIST, cinco pistas, faders, monitoramento/dispositivos, overdub/Undo, relógio/BPM/QUANTIZE/LOOP SYNC aprovada. F5 não foi alterado; não se afirma novo teste manual do atalho.
- Detector CRT não identificou alocações no callback de produção após o aquecimento do scratch interno do JUCE. A revisão do caminho novo confirmou memória fixa e atomics lock-free, sem locks/I/O.
- Carga medida em Debug: três OSC sequenciados, ROBOT, cinco loops e TRACK FX, blocos de 256 amostras. A 44,1 kHz: média **1,05449 ms**, p99 **1,5226 ms**, máximo **1,8226 ms**, prazo **5,80499 ms**. A 48 kHz: média **1,18569 ms**, p99 **1,787 ms**, máximo **1,791 ms**, prazo **5,33333 ms**. Medições locais com backend simulado, sem promessa de ausência de underruns em qualquer máquina/driver.

Logs: `tmp/6h3b-build.log`, `tmp/6h3b-tests.log` e `build/Testing/Temporary/LastTest.log`. Não houve teste físico de fala/canto/fones, latência acústica ou comparação sonora com a BOSS. Artefatos subjetivos de mudança de nota, ataque, mistura e andamento ainda exigem escuta do usuário.

## Arquivos

- Novos: `Source/FxSequence.h/.cpp`, `Tests/FxSequenceTests.cpp`, este documento.
- Motor: `Source/MusicalClock.h`, `Source/AudioSystem.h/.cpp`, `Source/LoopTrack.h/.cpp`, `Source/OscBotEffect.h/.cpp`; `Source/RobotEffect.h` recebe somente consulta de bypass para preservar sua cauda na mistura.
- Integração: `Source/InputFxEditor.h`, `Source/MemoryDisplay.h/.cpp`, `Source/TopPanelComponent.h/.cpp`, `Source/MainComponent.cpp`, `Source/VisualStyle.cpp` (glifo `/` apenas).
- Build/testes: `CMakeLists.txt`, `Tests/AudioSystemTests.cpp`, `Tests/OscBotEffectTests.cpp` (contagem de páginas atualizada).
- Referências: `docs/technical-reference.md`, `docs/MK2_BEHAVIOR_REFERENCE.md`. PDFs e `sources/` não foram modificados.
