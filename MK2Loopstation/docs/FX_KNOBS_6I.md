# Etapa 6I — knobs grandes de INPUT FX e TRACK FX

## Referência oficial e escopo

Consultados `MK2_BEHAVIOR_REFERENCE.md`, `technical-reference.md` e o [Parameter Guide oficial](https://static.roland.com/assets/media/pdf/RC-505mk2_Parameter_eng04_W.pdf), preservado em `references/RC-505mk2_Parameter_eng04_W.pdf`. As tabelas e os pictogramas foram conferidos nas imagens das páginas 5, 6, 30, 34, 35, 36, 38, 40 e 42.

- Páginas 5/6: FX TARGET seleciona o slot A–D no banco ativo, independentemente do slot aberto no editor, da pista selecionada e de INSERT. O padrão é A. INPUT FX mantém os bancos A–D e SINGLE/MULTI existentes; TRACK FX mantém apenas o banco A já disponível.
- Página 30: FX KNOB MODE é compartilhado. IMMEDIATE é o padrão; HOOK exige alcançar/cruzar o valor do parâmetro antes de transmitir alterações.
- Página 34: o desenho de **knob** identifica o parâmetro de performance. A estrela identifica o alvo de FX SEQUENCE e não substitui essa atribuição. OSC BOT exemplifica a diferença: BALANCE no knob grande, NOTE no sequenciador.
- A tabela TRACK FX da página 6 menciona INPUT FX na descrição de FX TARGET; interpretamos essa repetição editorial no contexto da seção TRACK FX, sem vincular os dois alvos.

Nenhum processador, algoritmo, parâmetro sonoro, atalho, LED, armazenamento de loops ou regra de transporte foi substituído. Não foram implementados ASSIGN, MIDI, OUTPUT LEVEL, bancos adicionais de TRACK FX ou o editor completo 5G.

## Atribuições implementadas

| Grupo / efeito real | Parâmetro do knob grande | Faixa | Página |
| --- | --- | --- | --- |
| INPUT / ROBOT | NOTE | C–B, 12 notas | 36 |
| INPUT / OSC BOT | BALANCE | 0–100 | 38 |
| INPUT / REVERB | E.LEVEL | 0–100 | 42 |
| INPUT / STEP SLICER | DEPTH | 0–100 | 40 |
| INPUT / G2B | BALANCE | 0–100 | 35 |
| TRACK A / LPF | CUTOFF | 0–100 | 34 |
| TRACK C / DISTORTION | DRIVE, correspondente ao parâmetro oficial DIST | 0–100 | 38 |

O processador local DISTORTION conserva DRIVE/TONE/LEVEL/MIX; a associação de DRIVE a DIST não declara implementação completa do algoritmo DIST da BOSS.

**Limites preexistentes:** LPF/BPF/HPF/PHASER no catálogo de INPUT FX ainda são editores visuais. Seus parâmetros oficiais de performance seriam CUTOFF nos filtros e E.LEVEL no PHASER, mas não foi criado DSP nesta etapa. O knob grande fica desabilitado nesses tipos. TRACK B/D também ficam desabilitados: o alvo continua selecionado, sem encaminhar o movimento a A/C. A descrição e a dica do controle identificam `UNAVAILABLE: NO DSP`.

## Como usar

### INPUT FX TARGET

1. Clique em **INPUT FX EDIT**. A/B/C/D selecionam o slot a editar, como antes.
2. Avance com a seta direita até a última página: **BANK / INPUT MODE / FX TARGET**.
3. Knob pequeno 1: banco A–D; knob 2: SINGLE/MULTI; **knob 3: FX TARGET A–D**.
4. EXIT sai do editor. Ligue o slot desejado por seu botão ou atalho existente.
5. Arraste o knob grande esquerdo verticalmente, ou use a roda do mouse. O visor mostra banco, slot, tipo, ON/OFF, parâmetro e valor por aproximadamente 1,8 s, depois retorna à tela anterior.

FX TARGET é conservado individualmente por banco. Editar ou ligar outro slot não muda esse alvo. Girar o knob não liga um efeito desligado e não altera SINGLE/MULTI.

### TRACK FX TARGET e INSERT

1. Abra **TRACK FX EDIT**. As setas continuam escolhendo o editor de A/LPF ou C/DISTORTION.
2. Pressione **ENTER** para a página existente de roteamento, agora com dois campos.
3. Knob pequeno 1 continua ajustando **INSERT** do slot que está sendo editado. **Knob pequeno 2 ajusta FX TARGET** do banco A compartilhado.
4. EXIT volta ao editor; outro EXIT fecha. Use o knob grande direito para CUTOFF de A ou DRIVE de C.

FX TARGET não muda INSERT. O processamento continua respeitando ALL/TRACK1–5 e os botões FX das pistas. Selecionar T2 pelo botão TRACK não redireciona o knob ou os efeitos.

### IMMEDIATE e HOOK

**MENU → SETUP → seta direita → FX KNOB MODE → knob pequeno 1**. EXIT restaura a página anterior. A primeira página SETUP/INDICATOR continua funcionando como antes; a seta esquerda retorna a ela.

- **IMMEDIATE:** o próximo movimento aplica a posição física virtual. Mudar banco/alvo/tipo não movimenta o knob nem escreve um valor por si só.
- **HOOK:** o marcador branco continua mostrando a posição física. Uma marca fina externa indica o valor efetivo aguardando captura. O visor mostra, por exemplo, `HOOK 30 -> 80`. Antes do encontro, nenhum parâmetro é publicado ao áudio; depois, o controle acompanha o movimento. A marca externa desaparece após a captura.
- Alterações de banco, alvo, tipo, modo ou valor pelo editor rearmam a captura. O LCD conserva o valor efetivo até o encontro. Para NOTE, o encontro usa a resolução das 12 notas válidas, sem notas fracionárias.
- Roda: um valor por evento, inclusive um semitom em ROBOT. Arraste: curso relativo de 400 pixels, sem salto ao pressionar; a leitura sonora é arredondada para os valores inteiros oficiais.

Teste simples: em IMMEDIATE, deixe o knob INPUT em 20. Pelo editor OSC BOT, configure BALANCE=80. Ative HOOK no SETUP. Gire de 20 até 79: BALANCE continua 80. Ao alcançar 80 e ultrapassá-lo, o valor passa a acompanhar o knob. Edite BALANCE=20 pelo LCD e teste a captura no sentido descendente. Repita trocando FX TARGET ou BANK.

## Uso durante gravação

INPUT FX continua antes do monitoramento e da captura: mudanças do knob são ouvidas com retorno ligado e ficam gravadas nas pistas que recebem INPUT FX. Para reproduzir o resultado gravado, o efeito de entrada não precisa permanecer ligado. O knob de TRACK FX altera apenas a reprodução pelo roteamento existente; não reescreve a memória do loop. Nenhum movimento modifica duração, BPM, QUANTIZE ou LOOP SYNC.

Use OSC BOT com FX SEQUENCE ativa: o knob altera BALANCE e não os passos, a nota manual ou a fase do sequenciador. A ativação simultânea e os atalhos F6–F10 permanecem existentes.

## Organização e tempo real

- `FxKnobPickup` guarda apenas a posição física, identidade do alvo e observação do último valor para captura. Não é outro armazenamento de parâmetros sonoros.
- `InputFxEditor` e `DisplayModel::trackFx` continuam donos dos valores. LCD e knobs grandes escrevem exatamente nesses mesmos campos.
- `TopPanelComponent` publica pelas funções existentes; `MainComponent`, `AudioSystem` e os processadores não precisaram de modificações. As mensagens de parâmetros já usam palavras atômicas e mailbox limitada; não há acesso à interface pelo callback.
- A suavização de frequência de ROBOT, BALANCE, ganhos, DEPTH, CUTOFF e DRIVE continua nos processadores existentes. Mover o knob não recria processadores, buffers ou loops.
- A expiração da informação temporária usa tempo da interface, sem interferir no relógio musical de áudio.

## Validação de 19/10/2026

- **Compilação Debug x64: aprovada**, CMake + Visual Studio 2022/MSVC 17.14, Windows SDK 10.0.26100.0; alvos `MK2Loopstation` e `MK2AudioTests`. Nenhum warning/error encontrado no log final.
- **Teste focado `MK2AudioTests --fx-knobs`: aprovado**, incluindo verificação CRT sem alocação no callback.
- **CTest completo: 1/1 aprovado, zero falhas**, agregando todos os testes internos de `MK2AudioTests`, em 127,82 s (127,88 s total).
- Sinal constante no monitoramento deixou de ser constante ao mover BALANCE do OSC BOT de 0 a 100, confirmando mudança da forma de onda. LPF com seno de 6 kHz: razão de energia CUTOFF mínimo/máximo **1,70839 × 10⁻⁹**. DRIVE mínimo/máximo: soma das diferenças absolutas **321,797** em 4096 amostras. Esses números verificam atuação do caminho real; não medem fidelidade sonora à MK2.
- FX SEQUENCE continuou no segundo passo ao mover BALANCE, sem retornar ao primeiro. Concorrência: 1500 blocos de áudio simultâneos a 300 ciclos de edição de banco/alvo/knobs, sem valores não finitos. A suíte completa confirmou ausência de alocações no callback após o aquecimento do JUCE.
- Regressões aprovadas: ROBOT/OSC BOT/FX SEQUENCE/REVERB/STEP SLICER/G2B/LPF/DIST, todos os bancos INPUT, INSERT ALL/TRACK1–5, cinco pistas, captura seca/processada, faders, PLAY/STOP/retomada/limpeza, overdub/Undo, QUANTIZE, LOOP SYNC, BPM/TAP, monitoramento/dispositivos, F6–F10 e os três modos de LED. F5 não foi alterado; seu não consumo pelos atalhos de INPUT FX foi testado, sem alegação de novo teste manual da janela.
- Imagens do LCD (SETUP, HOOK e FX TARGET) e do painel foram geradas e inspecionadas. Geometria do painel e LEDs preservados.

Medições do callback completo em Debug, blocos de 256 amostras:

| Cenário | Taxa | Média | p99 | Máximo | Prazo do bloco |
| --- | --- | --- | --- | --- | --- |
| 5 loops + 2 OSC/SEQ + STEP SLICER + REVERB + TRACK FX | 44,1 kHz | 1,1691 ms | 3,3735 ms | 6,5207 ms | 5,8050 ms |
| Mesmo cenário | 48 kHz | 1,2488 ms | 3,5003 ms | 4,2147 ms | 5,3333 ms |
| Callback com 4 ROBOT | 44,1 kHz | 1,6297 ms | 4,3323 ms | 7,0251 ms | 5,8050 ms |
| Callback com 4 ROBOT | 48 kHz | 1,6487 ms | 4,7309 ms | 5,1206 ms | 5,3333 ms |

**Limitação medida:** médias e p99 ficaram abaixo do prazo; alguns máximos o ultrapassaram, incluindo 7,9212 ms no cenário de um ROBOT a 44,1 kHz e 5,4250 ms a 48 kHz. Os testes medem tempo de parede do callback no host Windows e não isolam escalonamento de CPU. Portanto, não se declara ausência garantida de underruns nem se atribui a causa desses picos sem investigação adicional. Não foi alterado DSP para mascarar esse resultado. Os novos controles são executados na thread da interface e usam os publicadores já existentes.

Logs: `tmp/6i-build.log`, `tmp/6i-focused.log`, `tmp/6i-regression.log` e `tmp/6i-full-output.log`. Comando da suíte: `ctest --test-dir build -C Debug --output-on-failure`.

Testes focados operam os componentes reais de interface e o callback de produção com dispositivo sintético. Cobrem IMMEDIATE, HOOK nos dois sentidos, ausência de publicação antes da captura, captura após edição externa, troca de alvo/banco/tipo, 16 combinações INPUT banco/slot, preservação dos demais valores, SINGLE/MULTI, roda e arraste, notas válidas, LCD e DSP, slots indisponíveis, independência da pista selecionada, preservação de INSERT, captura processada, overdub/Undo e publicação concorrente.

Também verificam mudança de forma de onda em OSC BOT, resposta do LPF a um sinal de 6 kHz, resposta de DRIVE em DISTORTION e continuidade do segundo passo da FX SEQUENCE ao mover BALANCE. O hook CRT mede alocações no callback após aquecer os buffers internos do JUCE.

A concorrência é verificada por inspeção dos acessos atômicos e teste simultâneo de publicações da UI e processamento; não foi utilizado ThreadSanitizer nem se afirma uma prova exaustiva de ausência de corridas. Os testes são sintéticos: ainda é necessária escuta real com microfone/fones para avaliar sensação do controle, estalos percebidos e latência física. O diagnóstico de atraso da etapa anterior e a pendência de ATTACK do OSC BOT continuam inalterados.

## Diferenças conhecidas

A apresentação temporária do parâmetro, a marca de captura e o acesso reduzido às configurações são adaptações à interface virtual existente, sem alegação de reprodução pixel a pixel da navegação de todos os menus da BOSS. O manual define a semântica de IMMEDIATE/HOOK, mas não a resolução do mouse nem nossa curva de arraste. Estados e valores continuam em RAM, sem nova persistência entre execuções. Não foi validada fidelidade sonora ao hardware físico.

## Arquivos modificados

- Novo `Source/FxKnobPickup.h`: captura reutilizável, independente de DSP.
- `Source/PanelControls.h/.cpp`: componente interativo dos knobs grandes, roda, marcador e referência de captura.
- `Source/InputFxEditor.h`: FX TARGET por banco e atribuições oficiais aos valores existentes.
- `Source/MemoryDisplay.h/.cpp`: alvo TRACK, modo compartilhado SETUP, leitura/escrita dos parâmetros e informação temporária.
- `Source/TopPanelComponent.h/.cpp`: conexão dos dois controles, atualização dos encoders e publicações existentes.
- `Tests/AudioSystemTests.cpp`: testes focados `--fx-knobs`, também incluídos na suíte completa.
- `CMakeLists.txt`: novo cabeçalho na solução.
- Este documento e apontamentos em `technical-reference.md` / `MK2_BEHAVIOR_REFERENCE.md`.
