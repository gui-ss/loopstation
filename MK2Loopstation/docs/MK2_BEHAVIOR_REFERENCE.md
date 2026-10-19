# RC-505 MK2 — Referência de comportamento

Registro: 15/10/2026. Referência para as próximas etapas da Loopstation virtual, especialmente **6E — Overdub**. Este estudo não implementa comportamentos nem amplia o escopo da próxima etapa.

## 1. Fontes e limites da análise

- **Vídeo:** `C:/Users/Guilherme/Downloads/【RC-505 MKII Tutorial】How to play a loop station for beginners.mp4`. Arquivo local acessível, duração aproximada de **09:01**, resolução 1280 × 720, 60 fps.
- **MP:** [Manual do Proprietário em português, revisão 04](../references/RC-505mk2_ptb04_W.pdf).
- **PG:** [Parameter Guide em inglês, revisão 04](../references/RC-505mk2_Parameter_eng04_W.pdf).
- **Aplicativo:** inspeção dos arquivos de `Source/` e confronto com [o histórico técnico das etapas](technical-reference.md). O código atual prevalece sobre descrições de etapas antigas.

O MP4 foi analisado **visualmente**, por quadros com timestamps: levantamento de todo o vídeo, intervalos menores na demonstração e nas configurações, e sequências de um segundo nos trechos de transporte e limpeza. As legendas em inglês estão incorporadas à imagem. **Não foi feita escuta nem transcrição da narração.** Assim, não se confirma pelo vídeo o resultado sonoro de um efeito, a soma audível de um overdub ou a ausência de interrupções no áudio. Cliques rápidos podem ocorrer entre os quadros examinados.

Os timestamps abaixo são posições aproximadas no arquivo, não tempos medidos de resposta do equipamento. Informações de legendas são identificadas como tais; uma recomendação do apresentador não é um padrão obrigatório do hardware. As páginas citadas são as páginas numeradas dos PDFs. Foram conferidas também as imagens das páginas 9 e 16 do MP e 23 do PG.

## 2. O que o vídeo demonstra

| Tempo | Evidência visual ou legenda | O que podemos concluir |
| --- | --- | --- |
| 02:00–02:20 | Diagrama separa INPUT FX, área central, TRACK FX e cinco faixas inferiores. | Há cinco conjuntos de controles de pista e dois grupos distintos de efeitos. |
| 02:20–02:25 | Legenda explica que INPUT FX processa o som recebido do microfone. | Essa é a função apresentada para o grupo de entrada; o resultado sonoro não foi avaliado. |
| 02:30–02:40 | Aparece a memória `28 / Megalovania`. | A demonstração usa uma memória preparada; seus efeitos e configurações não devem ser tratados como padrão de fábrica. |
| 02:45–02:55 | Operação na região INPUT FX EDIT/A–D, botão azul e mudança dos nomes de efeitos no visor; legenda explica BANK. | Demonstração de seleção de conjuntos de efeitos por banco. O procedimento completo é confirmado pelo MP, pp. 11 e 16. |
| 03:00–03:10 | Diagrama/legenda apresenta a área inferior como controles de gravação, reprodução e parada. | Confirma a organização funcional proposta pelo tutorial. |
| 03:12–03:21 | Pista 2 com centro vermelho; depois roxo, depois apagado, enquanto o anel externo permanece iluminado. | Existem indicações independentes no centro, no anel e no botão TRACK. Não é seguro atribuir todos os estados de transporte apenas às cores desses quadros. |
| 03:14–03:18 | Visor com parâmetros distribuídos sobre os quatro knobs; mão opera a região central. | Há edição paginada por knobs. Não foram transcritos todos os parâmetros dessa passagem nem identificado com segurança seu tipo de efeito. |
| 03:22–03:27 | Novo gesto em INPUT FX EDIT/A–D; em seguida, acionamento do botão principal da pista 5 e mudança de vermelho para roxo. | Há operação de outra pista e mudança visual após o acionamento. A sequência exata REC/PLAY/OVERDUB depende das configurações, não mostradas integralmente. |
| 03:30–03:40 | Legenda explica que TRACK FX processa som já gravado. | Distingue o destino de TRACK FX do grupo de entrada. |
| 03:45–03:55 | Acionamento na área TRACK FX A/B e mudança de indicadores. | Há controle de efeitos de pista durante a demonstração. Não confirma, isoladamente, o algoritmo nem seu resultado audível. |
| 04:10–04:50 | Performance com várias pistas; diferentes centros e segmentos dos anéis ficam ativos. Em 04:45–04:50 há indicação visual nas cinco pistas. | Demonstra uso dos cinco conjuntos durante uma performance. Não prova que todas as fontes audíveis sejam loops novos gravados nesse trecho. |
| 04:25; 04:35 | Operação na região de seleção de banco de INPUT FX; depois, no knob grande INPUT FX. | Banco e controle de efeito são utilizados durante a performance. Não foi determinada a curva ou faixa do parâmetro controlado. |
| 05:15–05:20 | Tela de memória/INPUT FX; número 28, nome Megalovania e BPM 240.0 legíveis. | Número, nome, andamento e efeitos coexistem na tela. O BPM pertence à memória demonstrada. |
| 05:25–05:45 | Seção de ajuste do microfone; legenda `MENU → MIXER → MIC IN`, visor MIXER com entradas e barras. | Demonstra ajuste de nível de entrada. A orientação visual de cerca de 80% da barra ao fazer um kick é do apresentador, não uma prescrição de ganho numérico 80. |
| 05:50–06:05 | `MENU → Right → CTL FUNC → PREFERENCE`; aparecem MODE PLAY, MODE UNDO, QUICK CLEAR e ALL CLEAR; legenda manda ativar os dois últimos. | Demonstra configuração de preferências dos controles. MODE PLAY/MODE UNDO aparecem como MEMORY nessa memória. |
| 06:10–06:35 | Legendas tratam de atribuições de funções dos botões. | O tutorial apresenta a possibilidade de personalização; não se obteve uma lista completa das atribuições usadas na performance. |
| 06:40–06:47 | Operações em STOP das pistas 1 e 2; seus indicadores de conteúdo/anel se apagam sucessivamente, enquanto outras pistas permanecem indicadas. | Evidência visual de limpeza individual nesse contexto. A amostragem não estabelece o número exato de cliques nem a duração de cada pressão. Ver a ambiguidade de QUICK CLEAR na seção 5. |
| 06:50–06:53 | Mão na região ALL START/STOP e apagamento subsequente dos anéis das demais pistas. | Compatível com a limpeza coletiva apresentada nesse segmento; o gesto preciso é descrito pelo PG, p. 23, não medido nos quadros. |
| 06:55–07:20 | Seção de metrônomo; `MENU → OUTPUT → ROUTING → INPUT/RHYTHM`; RHYTHM é desligado na página MAIN. | Trata-se de retirar o ritmo do destino MAIN, não necessariamente desligar o gerador de ritmo. |
| 07:25–07:28 | Legenda orienta avançar três páginas e ativar PHONES RHYTHM. Em 07:26–07:28 o visor mostra PHONES RHYTHM ON. | Demonstra encaminhar o ritmo aos fones separadamente da saída principal. Não foi verificado o som nas conexões físicas. |

**REC, PLAY, OVERDUB, STOP e UNDO no vídeo:** há operação dos botões principais e mudanças de indicadores, mas não foi possível reconstruir com segurança uma sequência completa e inequívoca de overdub a partir dos quadros. Também não foi identificada uma demonstração inequívoca de desfazer/refazer. A descrição normativa desses estados abaixo vem dos manuais. A aparência roxa de vários LEDs difere das cores padrão documentadas; pode envolver configuração e reprodução de cor do vídeo. Não foi verificada a configuração LOOP STATUS COLOR dessa memória.

## 3. O que os manuais oficiais confirmam

### 3.1. Os cinco conjuntos de controles de pista

As pistas 1–5 repetem os mesmos controles. Referências: **MP, pp. 5, 9–11; PG, pp. 2–3**.

| Controle por pista | Comportamento documentado |
| --- | --- |
| Botão principal circular | Em frase vazia, inicia a primeira gravação. Depois alterna reprodução e overdub, conforme REC ACTION. Em frase existente, permite reprodução/overdub. |
| STOP, com quadrado | Interrompe gravação, reprodução e overdub. Manter pressionado por dois segundos ou mais apaga a pista, na operação descrita no MP. STOP não equivale a CLEAR. |
| Fader vertical | Ajusta o volume de reprodução da pista; não deve ser confundido com ganho do microfone. |
| TRACK | Abre configurações da pista. Aceso indica frase existente; apagado, ausência de frase. Não significa simplesmente PLAY ligado/desligado. |
| FX | Habilita a aplicação dos efeitos à pista. A descrição resumida do MP p. 5 menciona TRACK FX; o procedimento do MP p. 11 e a tabela FX do PG p. 2 incluem INPUT/TRACK FX. Essa abrangência precisa ser considerada quando houver INPUT FX real. |
| Indicador circular externo | Pode representar estado, posição ou nível, conforme SETUP / INDICATOR. Não representa sempre apenas REC ou presença de áudio. |

A “pista atual” é o alvo das operações/edição. Na tela de reprodução, EXIT faz o STOP dessa pista piscar em verde (**MP, p. 5**). Isso é diferente de parar ou limpar uma pista.

### 3.2. Transporte, overdub e cores

O fluxo ilustrado no **MP, p. 9** é:

```text
VAZIA ── principal ──> REC ── principal ──> PLAY
                                           ↕ principal
                                        OVERDUB

REC / PLAY / OVERDUB ── STOP ──> PARADA
PARADA COM FRASE ── principal ──> reprodução da frase existente
```

O quarto acionamento do exemplo REC → PLAY → OVERDUB retorna a **PLAY**, não a vazio. OVERDUB acrescenta a nova performance à frase que está sendo reproduzida; pode ser repetido na mesma pista (**MP, p. 9; PG, p. 2, DUB MODE**).

As cores padrão do botão principal são vermelho para REC, verde para PLAY, amarelo para OVERDUB e apagado para STOP. As cores podem ser alteradas por LOOP STATUS COLOR (**MP, pp. 5 e 18**). Um botão apagado não basta para distinguir pista vazia de pista parada com conteúdo; é necessário consultar a presença de frase e os demais indicadores.

Não há um único comportamento universal para toda memória:

- **REC ACTION:** permite REC → PLAY → DUB ou REC → DUB → PLAY (**PG, p. 4**).
- **DUB MODE:** OVERDUB acumula a performance; REPLACE1 sobrescreve enquanto permite ouvir o material anterior; REPLACE2 sobrescreve sem reproduzir esse material (**PG, p. 2**). Somar camadas e substituir são operações diferentes.
- **START MODE / STOP MODE:** início imediato ou fade; parada imediata, fade ou final do loop. Na parada pendente, o botão principal pisca; outro STOP interrompe imediatamente, e não é permitido entrar em overdub enquanto se aguarda parar (**PG, p. 2**).
- **1SHOT / REVERSE:** 1SHOT não permite overdub; REVERSE ligado impede mudar para overdub após concluir a gravação (**PG, p. 2**).
- **LOOP SYNC / TEMPO SYNC / QUANTIZE:** controlam alinhamento, andamento e correção de gravação. QUANTIZE é ignorado durante overdub/reprodução. Nas condições descritas no guia, QUANTIZE OFF ainda pode quantizar o fim da primeira gravação; não significa, sozinho, gravação inteiramente livre (**PG, pp. 3–4**).

### 3.3. UNDO/REDO é um modo de operação

Procedimento do **MP, p. 9**: ativar UNDO/REDO, pressionar o botão principal da pista desejada para desfazer/refazer e pressionar UNDO/REDO novamente para sair desse modo. Nesse contexto, verde significa desfazer disponível, vermelho significa refazer disponível e apagado significa indisponível. Não reutilizar automaticamente a interpretação das cores de transporte.

O **MP, p. 5** também documenta a mudança de função dos controles da pista nesse modo:

| Controle | Função no modo UNDO/REDO |
| --- | --- |
| FX | MARK CLEAR: elimina uma marca. |
| TRACK | MARK SET: marca uma frase com overdub. |
| STOP | MARK BACK: altera o alvo de desfazer/refazer para a frase marcada; sem marca, retorna ao estado de pós-gravação. |
| Principal | UNDO/REDO da gravação ou overdub realizado. |

Isso não autoriza presumir um histórico ilimitado de camadas, sua estrutura interna ou os limites exatos de uma operação de undo. A configuração de funções também pode ser personalizada por CTL FUNC/PREFERENCE (**PG, pp. 14 e 23**).

### 3.4. INPUT FX, TRACK FX, bancos e edição

**INPUT FX** atua no sinal de entrada e pode ser gravado com a performance. **TRACK FX** atua nas pistas já gravadas. O MP p. 11 descreve essas duas aplicações; o PG pp. 5–6 detalha os destinos de inserção e as condições de roteamento.

- Cada grupo possui bancos **A–D**, com efeitos/slots **A–D**. A letra do banco e a letra do slot são identificadores diferentes; o slot A não é obrigatoriamente LPF.
- MODE **SINGLE** permite um efeito ativo; **MULTI** permite vários. SW MODE **TOGGLE** alterna a cada acionamento; **MOMENT** mantém o efeito ligado somente durante a pressão.
- INPUT FX INSERT seleciona entradas; TRACK FX INSERT seleciona ALL ou TRACK1–5. Habilitação por pista e encaminhamento para as saídas também importam. Um LED de slot aceso não prova, sozinho, que certo sinal percorra o efeito.
- O knob grande do grupo controla parâmetros do efeito alvo. A função depende do tipo de efeito e de FX TARGET; não é necessariamente volume ou MIX.

Procedimento de banco/edição confirmado no **MP, pp. 11 e 16**:

1. Selecionar a memória e pressionar EDIT do grupo desejado.
2. Para trocar banco, manter EDIT até um dos botões A–D ficar azul; escolher A–D e pressionar EDIT para concluir a seleção.
3. Para escolher o efeito a editar, manter EDIT pressionado e pressionar o A–D correspondente.
4. Usar as setas para páginas de parâmetros e os knobs 1–4 para os valores mostrados. Os campos variam com FX TYPE.
5. Usar EXIT para retornar. Salvar com WRITE se for necessário conservar as alterações na memória.

Nos botões de efeito, vermelho indica ligado, rosa identifica o efeito atual operável/editável quando ligado, e azul identifica o banco no modo de seleção (**MP, pp. 4–5**). Selecionar banco, selecionar efeito para edição e ligar/desligar efeito são ações distintas.

### 3.5. Visor, controles globais e configurações relevantes

- As sete telas de reprodução seguem a ordem **MEMORY NUMBER → TRACK STATUS → LOOP TRACKS → LOOP STATUS → LOOP LEVEL → INPUT FX → TRACK FX**. As setas alternam as telas; número/nome/BPM, estado, presença de frase, pista atual, progresso e nível são informações diferentes (**MP, p. 8; PG, p. 30**).
- ALL START/STOP inicia pistas em conjunto e interrompe gravação/reprodução em conjunto, com seleção das pistas que iniciam configurável por memória (**MP, pp. 4 e 10**).
- MENU acessa configurações do sistema; LOOP, configurações da memória. ENTER na tela de reprodução abre MIXER. EXIT retorna. WRITE é acessado pela combinação EXIT + ENTER no hardware (**MP, pp. 4, 8 e 12**).
- Ganho de entrada, volume da pista, OUTPUT LEVEL e roteamento de saída são controles diferentes. O mixer do tutorial não corresponde ao volume de monitoramento do aplicativo (**MP, pp. 8–9; PG, pp. 11–13**).
- O ritmo tem controles próprios. ROUTING permite tratar sua presença em MAIN e PHONES separadamente; INPUT THRU trata o encaminhamento direto da entrada. Não confundir silenciar o ritmo em MAIN com desligá-lo ou impedi-lo de ser gravado (**PG, pp. 3 e 12**).
- BOUNCE permite utilizar reprodução de pistas como entrada de gravação quando configurado. Isso é uma função específica, não uma consequência obrigatória de overdub (**PG, pp. 3–4**).
- WRITE salva a memória; STOP não substitui uma operação de salvamento (**MP, pp. 9 e 12**).

## 4. Diferenças em relação ao aplicativo atual

**Atualização 6G-B:** o aplicativo implementa agora LOOP SYNC MEASURE usando o relógio compartilhado, com esperas por compasso, SW por pista no motor e atalho L. TEMPO SYNC e outros modos continuam ausentes; consulte [as políticas e limitações da implementação](technical-reference.md#etapa-6g-b-loop-sync-modo-measure). As observações originais do vídeo e dos manuais abaixo continuam históricas.

**Nota posterior — etapa 6G-A:** a tabela abaixo é o retrato histórico do estudo anterior à 6E. Agora as cinco pistas têm áudio, overdub, Undo direcionado, faders e progresso reais; BPM/TAP e quantização inicial também estão implementados. TRACK seleciona o alvo de Ctrl+Z, sem abrir menu; TRACK FX permanece restrito à pista 1. Consulte [o registro atualizado da 6G-A](technical-reference.md#etapa-6g-a-cinco-pistas-com-áudio-independente) para escopo, adaptações e testes. As observações originais do vídeo/manuais não foram alteradas.

Esta comparação foi feita por **leitura do código**, sem iniciar o aplicativo, compilar ou executar testes nesta tarefa. As diferenças abaixo registram o escopo existente; não são autorização para implementar todas elas.

| Aspecto | Referência do equipamento | Aplicativo atual |
| --- | --- | --- |
| Cinco pistas | Cinco conjuntos funcionais; vídeo usa os cinco. | Só a pista 1 grava/reproduz áudio. Pistas 2–5 têm estados visuais. |
| Botão principal | Após a primeira gravação, PLAY ↔ OVERDUB no fluxo do MP p. 9. | `LoopTrack` tem EMPTY, REC, PLAY e STOP. Em PLAY, o clique não muda o transporte; não existe overdub de áudio. |
| Simulação das pistas 2–5 | Não há retorno automático a vazio no quarto clique do fluxo básico. | O ciclo visual antigo REC → PLAY → OVERDUB → BLANK ainda termina sem indicação de frase. Não copiá-lo para o motor real. |
| STOP e limpeza | STOP separado de CLEAR; pressão longa e preferências adicionais no hardware. | STOP preserva o loop; Shift + STOP limpa somente parado. Retomar começa no início. A inscrição CLEAR: HOLD é apenas parte do desenho. |
| UNDO/REDO | Modo que muda as funções dos controles de pista. | Botão visual; não há histórico de gravação/overdub nem MARK BACK. |
| Fader e monitoramento | Volume da pista e sinal direto são caminhos distintos. | Fader 1 controla a reprodução com suavização; monitoramento é independente, inicialmente desligado. |
| FX de cada pista / TRACK | Habilitação de efeitos e edição/indicação de frase. | Botões inferiores FX/TRACK ainda visuais; não fazem o roteamento documentado nem abrem configurações de pista. |
| INPUT FX | Processa entradas e pode entrar na gravação. | Banco A, slots A–D com LPF/HPF/BPF/PHASER configuráveis visualmente; sem processamento de entrada. |
| TRACK FX | Tipos, bancos, destinos e modos configuráveis. | LPF em A e DISTORTION em C, em série, somente na pista 1. B/D são visuais; não há configuração completa de bancos/destinos. |
| Edição dos slots | EDIT + A–D seleciona efeito; EDIT prolongado seleciona banco. | INPUT A–D seleciona slot sem alternar ON/OFF enquanto edita. TRACK A/C alterna ON/OFF e seleciona slot mesmo no editor; setas mudam A/C sem alternar. São adaptações atuais. |
| Knobs | Grandes knobs atuam no efeito alvo; pequenos knobs têm funções contextuais. | Quatro pequenos knobs editam os parâmetros implementados; knobs grandes/OUTPUT LEVEL não possuem essas funções reais. |
| Visor | Posição/nível correspondem à operação das pistas. | Sete páginas presentes. Estado da pista 1 vem do motor; progresso e níveis continuam simulados, com oito batidas por volta no modelo visual. |
| Memória e controles globais | WRITE, ALL START/STOP, ritmo, menus e atribuições reais. | Sem persistência entre execuções; controles globais citados permanecem visuais. EXIT/setas e editores implementados funcionam no escopo atual. |
| Dispositivos e roteamento | Entradas/saídas físicas, MAIN/SUB/PHONES e roteamento configurável. | Um AudioDeviceManager, microfone/saída do Windows e monitoramento. F5 local e clique direito na borda abrem configurações; não há equivalente completo ao roteamento físico. |

Arquivos usados para conferir essas diferenças:

- [LoopTrack.h](../Source/LoopTrack.h), [LoopTrack.cpp](../Source/LoopTrack.cpp): estados, comandos, RAM, duração, parada, leitura e gravação da pista 1.
- [AudioSystem.cpp](../Source/AudioSystem.cpp), [MainComponent.cpp](../Source/MainComponent.cpp): callback, monitoramento separado, comandos e atualização do estado visual real.
- [TrackComponent.cpp](../Source/TrackComponent.cpp), [TopPanelComponent.cpp](../Source/TopPanelComponent.cpp): interação das pistas, botões globais, seleção/edição de efeitos.
- [TrackEffects.cpp](../Source/TrackEffects.cpp): leitura do loop → LPF A → DISTORTION C → volume da pista → soma com retorno direto. Esses efeitos não escrevem no buffer gravado nem processam o monitoramento.
- [DisplaySimulation.h](../Source/DisplaySimulation.h): animação de progresso e níveis sem medição do áudio.

## 5. O que ainda precisamos investigar

1. **Overdub e undo na performance do vídeo.** A análise visual não comprova quais acionamentos correspondem a uma nova camada ou a desfazer/refazer. Uma revisão com escuta e identificação das configurações da memória seria necessária para essa conclusão.
2. **Cores e configuração da memória 28.** Não foram confirmados REC ACTION, DUB MODE, LOOP STATUS COLOR, atribuições de botões ou os destinos completos de efeitos. Não inferir esses parâmetros por LEDs roxos ou pelo nome da música.
3. **QUICK CLEAR: ambiguidade documental.** O PG p. 23, inclusive na imagem da página, diz literalmente “clears all track” ao descrever o duplo clique em STOP. No vídeo, 06:40–06:47, os indicadores das pistas 1 e 2 desaparecem separadamente, mantendo outras pistas. Registrar a divergência sem corrigir silenciosamente o manual nem implementar limpeza coletiva por suposição. A linha seguinte documenta ALL CLEAR por pressão longa em ALL START/STOP. Conferir esse ponto em referência adicional ou no hardware antes de reproduzir o atalho.
4. **Unidade de undo.** Os trechos consultados não especificam suficientemente como agrupar múltiplas voltas de um mesmo overdub, o que ocorre ao sair por STOP ou como um novo overdub invalida REDO. A semântica básica de desfazer existe; esses detalhes ainda precisam de confirmação ou de uma decisão explícita do projeto.
5. **Ganho e algoritmo de overdub.** Não foram confirmados coeficiente de soma, headroom, limitador, atenuação acumulada, tratamento de emendas ou precisão interna do hardware. Não alegar equivalência sonora usando uma fórmula presumida.
6. **Latência e transições.** O vídeo não permite medir compensação de latência de gravação, continuidade em amostras ou o instante efetivo de uma transição. Também não especifica o comportamento do nosso aplicativo quando o dispositivo muda durante overdub.
7. **Cobertura dos efeitos por pista.** Ao implementar INPUT FX real, conciliar o resumo do MP p. 5 com MP p. 11/PG p. 2 e verificar as combinações de INSERT, FX e ROUTING. A implementação atual de TRACK FX na pista 1 não é uma reprodução completa dessa matriz.

## 6. Considerações para a etapa 6E — Overdub

**Confirmado pelos manuais:** o overdub normal adiciona a performance à frase em reprodução; PLAY e OVERDUB alternam repetidamente; STOP interrompe sem ser sinônimo de apagar; UNDO/REDO usa um modo próprio; REC ACTION e DUB MODE são conceitos diferentes.

**Proposta de escopo mínimo, ainda não implementada:**

1. Acrescentar OVERDUB real somente à pista 1, mantendo o fluxo já adotado REC → PLAY. A partir de PLAY, o principal entraria em OVERDUB; o acionamento seguinte retornaria a PLAY.
2. Preservar a duração do loop e sua posição ao alternar PLAY/OVERDUB. A entrada nova seria acrescentada à frase existente durante as voltas, sem reiniciar a primeira gravação. Essa é uma decisão de implementação coerente com o uso em loop, não uma medição do algoritmo interno da BOSS.
3. Manter STOP, presença de frase e limpeza como operações separadas. Preservar inicialmente Shift + STOP somente parado, conforme o escopo já autorizado do aplicativo; não introduzir QUICK CLEAR a partir da ambiguidade acima.
4. Manter o microfone como fonte do novo material, separado do retorno de monitoramento e da saída de TRACK FX. Não regravar automaticamente o sinal processado de reprodução; BOUNCE seria outra etapa. Preservar LPF/DISTORTION na reprodução e o fader independente da captura.
5. Definir proteção de ganho e continuidade das transições, sem assumir que a BOSS usa normalização, saturação ou atenuação específica. Prever buffers fora do callback e conservar o único motor de áudio existente.
6. Alimentar LED/estado do visor com o estado real de overdub. Não usar o quarto clique da simulação para apagar a pista nem interpretar animações simuladas como posição medida do áudio.
7. Decidir explicitamente se UNDO fará parte da 6E. Caso entre no escopo, definir primeiro o limite de uma operação e a regra de REDO; não transformar o botão UNDO/REDO em desfazer global imediato sem registrar a diferença para o hardware. MARK BACK e atribuições completas podem permanecer fora dessa etapa.

Nenhum código de produto, motor de áudio ou interface foi alterado neste estudo. Não foram executados compilação, testes ou aplicativo. Os PDFs e o vídeo foram mantidos intactos; os quadros e renderizações usados na leitura são materiais temporários em `tmp/`.


### Atualização de implementação — etapa 6H

O retrato histórico acima permanece como registro do estudo. Desde 6H, TRACK abre a configuração mínima FX da pista e seleciona o alvo de Undo; não redireciona efeitos. FX por pista habilita a participação nos slots compartilhados. Conforme PG p. 6, INSERT pertence a cada slot: A/LPF e C/DISTORTION agora podem selecionar ALL ou TRACK1–5 independentemente. O editor existente usa ENTER para acessar INSERT e knob 1 para o destino. Não foram implementados bancos/edição completa da 5G, INPUT FX real ou roteamento MAIN/SUB. A ordem interna do hardware em combinações de INSERT individual e ALL continua não confirmada; a topologia adotada, limites, testes e instruções estão em [Etapa 6H da referência técnica](technical-reference.md#etapa-6h-fx-por-pista-e-insert-compartilhado).


### Atualização de implementação — etapa 6H.1

ROBOT é agora o primeiro INPUT FX real, selecionável em FX TYPE sem apagar os tipos/valores anteriores. PG p. 36 confirma NOTE C–B, FORMANT -50–+50 e MODE 1/2; o significado antigo/novo dos modos é oficial, mas os algoritmos não são publicados. O protótipo usa uma aproximação de vocoder com dois modos distintos, sem alegar equivalência sonora. INPUT FX processa o microfone antes de monitoramento e captura; FX OFF por pista grava seco, conforme PG p. 2. A reprodução não passa pelo INPUT FX. BPF e demais tipos de entrada anteriores continuam visuais. Consulte [Etapa 6H.1](technical-reference.md#etapa-6h1-input-fx-robot-em-tempo-real) para acesso, mapeamentos internos, validação sintética e limitações. A escuta real de voz/beatbox e a comparação com o hardware continuam pendentes; não há novas observações do vídeo.

### Atualização de implementação — etapa 6H.2

Após relato de chiados nas notas graves, foram reproduzidos excesso de custo em Debug e ondulação dos envelopes de vozes graves. A correção mantém parâmetros e roteamento, reduz cálculos repetidos, suaviza o envelope e trata material não vozeado. Consulte [diagnóstico, comparação técnica, medições e limites](ROBOT_6H2_QUALITY.md). Não foram acrescentadas observações do vídeo nem alegações de equivalência sonora; a escuta da versão corrigida continua necessária.

### Atualização de implementação — etapa 6H.3A

OSC BOT é selecionável nos quatro slots INPUT FX, com os seis parâmetros e padrões oficiais da p.38. Nenhum slot é sobrescrito automaticamente. O microfone excita uma síntese de nota fixa, processada antes do monitoramento/captura; a reprodução não reaplica o efeito. A implementação é uma aproximação de síntese subtrativa com tabelas limitadas em harmônicos. MOD SENS usa modulação de corte pelo envelope como decisão local, pois o manual não publica seu destino/curva. Consulte [acesso, DSP, testes e limitações](OSC_BOT_6H3A.md). Não há novas observações do vídeo nem sequenciamento/presets de músicas.


### Atualização de implementação — etapa 6H.3B

FX SEQUENCE controla NOTE de OSC BOT por slot A–D do banco A, com até 16 passos no callback e referência do MusicalClock. O LCD existente edita SW/SYNC/RETRIG/TARGET/RATE/MAX/VAL1–16 e INPUT MODE SINGLE/MULTI. 2MEAS é a duração de cada passo (oito batidas em 4/4). Slots adjacentes de OSC BOT em MULTI têm mistura paralela para conservar as duas vozes. Consulte [FX_SEQUENCE_6H3B.md](FX_SEQUENCE_6H3B.md) para presets de validação, acesso, testes Debug x64 e limitações explícitas: curva RATE numérico local, interpretação de SYNC/RETRIG, prioridade de frase e topologia MULTI não verificadas em hardware. Nenhum preset é carregado automaticamente, e não há novas observações do vídeo ou alegação de fidelidade sonora exata.


### Atualização de implementação — etapa 6H.3D

STEP SLICER é um INPUT FX real, antes de monitoramento/captura, com RATE, MAX, DEPTH, compressão e LENGTH/LEVEL independentes nos 16 passos. O seletor mínimo BANK A–D foi autorizado e acrescentado à última página dos editores; parâmetros, ON/OFF e estados de cada banco/slot são independentes. O painel e os atalhos existentes foram preservados, sem novos LEDs. Consulte [STEP_SLICER_6H3D.md](STEP_SLICER_6H3D.md) para arquitetura, acesso, testes, topologia OSC A/B → SLICER C → REVERB D, aproximações de RATE/compressor e limitações. Não há novas observações do vídeo nem validação auditiva física.


## Complemento 6H.4 — anéis externos

Confirmado pela tabela **PG p.30, SETUP / INDICATOR**, conferida visualmente: STATUS = vazio apagado / parado com frase aceso / REC e DUB piscando no tempo / PLAY posição de um compasso. LOOP POSITION = vazio, STOP e REC apagados / PLAY e DUB posição da frase inteira. LEVEL = vazio, STOP e REC apagados / PLAY e DUB nível da reprodução.

Na implementação 6H.4 os anéis e o visor recebem posição/níveis reais, por snapshot do áudio. Não confundir o anel externo com o botão central de estado. Cores, segmentação e resposta visual estão descritas com seus limites em [TRACK_LEDS_RECORDING_6H4.md](TRACK_LEDS_RECORDING_6H4.md). A consulta ao manual não confirma fidelidade sonora dos algoritmos aproximados de G2B nem resolve a latência física do computador.


## Atualização de implementação — etapa 6I

Os dois knobs grandes agora controlam parâmetros reais dos efeitos existentes, por FX TARGET independente de seleção de pista/slot de edição. INPUT mantém alvo por banco A–D; TRACK mantém banco A e processamento A/LPF e C/DISTORTION. MENU/SETUP ganhou a página mínima FX KNOB MODE compartilhada: IMMEDIATE (padrão) e HOOK com captura. As atribuições seguem o pictograma de knob do Parameter Guide, não a estrela de FX SEQUENCE: OSC BOT controla BALANCE; REVERB, E.LEVEL. Limites, uso, arquivos e resultados estão em [FX_KNOBS_6I.md](FX_KNOBS_6I.md). Não foram criados DSP adicionais, alterados LEDs/atalhos nem corrigido ATTACK do OSC BOT.
