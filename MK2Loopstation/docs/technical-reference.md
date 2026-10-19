# Referência técnica — BOSS RC-505 MKII

Documentos oficiais fornecidos pelo usuário e copiados sem alterações para o projeto:

- [Manual do Proprietário em português, revisão 04](../references/RC-505mk2_ptb04_W.pdf): original em `E:/Download/RC-505mk2_ptb04_W.pdf`.
- [Parameter Guide em inglês, revisão 04](../references/RC-505mk2_Parameter_eng04_W.pdf): original em `E:/Download/RC-505mk2_Parameter_eng04_W.pdf`.

Esses arquivos são referência permanente para visor, navegação, estados das pistas e parâmetros de efeitos. Consulte as páginas e tabelas correspondentes antes de implementar cada próxima etapa. Nenhum efeito deve ser inferido somente da fotografia do painel.

## Etapa 5A: páginas consultadas

**Manual do Proprietário, página 8 — “Alternar entre as telas de reprodução”**:

- Tela (1), NÚMERO DA MEMÓRIA: número central grande, nome abaixo, andamento no canto superior direito, legendas dos knobs no rodapé.
- Tela (2), STATUS DA PISTA: número de memória pequeno em negativo, nome ao lado e cinco ícones alinhados da esquerda para a direita.
- Ícone preenchido com cantos cortados: gravação, reprodução ou overdubbing. Esses três estados não recebem ícones ou cores diferentes nessa tela do manual.
- Mesmo contorno sem preenchimento: parado com frase existente.
- Traço horizontal estreito com pontas: nenhuma frase.
- Ponto central: pista atual. No projeto, acompanha a última pista acionada pelo botão principal ou STOP; é desenhado dentro dos ícones com frase.
- As setas físicas esquerda/direita percorrem as variações de tela. Nesta etapa o percurso é restrito às duas solicitadas, com retorno circular.
- Os indicadores de phantom power e bloqueio aparecem somente se essas funções estiverem ligadas. Não são exibidos no projeto, que não implementa essas funções.

**Parameter Guide, página 30 — SETUP**: DISPLAY MODE define a tela inicial, com MEMORY NUMBER como padrão. A tabela INDICATOR diferencia parado sem frase, parado com frase, gravação, overdubbing e reprodução. Nenhuma nova configuração de SETUP ou animação por andamento foi implementada.

**Parameter Guide, página 4 — LOOP**: CURRENT TRACK indica a pista alvo para operação/edição. Nesta etapa somente o marcador visual acompanha os controles existentes; não há menu de seleção/edição.

## Limites e adaptação da simulação existente

- A memória exibida é `01 / Memory01 / 120.0 BPM`. Número, nome e BPM são dados do modelo, sem seleção, edição ou tap tempo nesta etapa.
- As setas são ações momentâneas, sem ficar ligadas após a navegação.
- O ciclo da etapa 4 continua REC → PLAY → OVERDUB → STOP/BLANK. O quarto clique volta à indicação vazia.
- STOP separado mantém apenas a indicação visual de existência de frase se a pista já estava ativa, permitindo o contorno de “parado com frase”. Não existe áudio nem frase gravada.
- O display recebe estados, efeitos e volumes por callbacks. Até a etapa 5B não havia animação; a etapa 5C adiciona somente o timer visual descrito abaixo, sem relógio de transporte de áudio.
- As legendas MEMORY / MFX REVERB são inscrições do layout de referência; não habilitam menus ou efeitos.
- Na etapa 5A, as cinco telas adicionais ficaram fora do escopo. A etapa 5B acrescenta somente INPUT FX e TRACK FX, conforme descrito abaixo.

## Etapa 5B: telas de efeitos

**Manual do Proprietário, página 8, ilustrações (6) FX DE ENTRADA e (7) FX DE PISTA**:

- Cabeçalho compacto com número e nome da memória, BPM no topo e legendas MEMORY / MFX REVERB no rodapé, preservando o visor existente.
- Linha INPUT FX-A ou TRACK FX-A. O A após o hífen identifica o banco ilustrado; permanece fixo, sem implementar seleção de bancos.
- Quatro colunas de efeitos correspondentes aos botões físicos A/B/C/D, da esquerda para a direita. As letras abaixo das ilustrações são explicações do manual, não uma linha extra dentro do LCD.
- ON: retângulo claro preenchido com nome em negativo. OFF: nome claro diretamente sobre o fundo. Não foram acrescentadas legendas ON/OFF dentro do visor, pois o próprio contraste representa esses estados.
- INPUT: LPF, G2B, PTN / SL (PATTERN SLICER), DLY (DELAY).
- TRACK: BT / SCT (BEAT SCATTER), FLN / GER (FLANGER), TR / NS (TRANSPOSE), GRN / DLY (GRANULAR DELAY).
- Nomes completos conferidos na seção Input FX/Track FX List do Parameter Guide. São atribuições de exemplo, sem parâmetros ou processadores.

As setas agora percorrem MEMORY NUMBER → TRACK STATUS → INPUT FX → TRACK FX, circularmente, e a esquerda percorre a ordem inversa. Os oito indicadores começam em OFF, de acordo com o estado inicial dos botões físicos já existentes. Cada clique atualiza apenas o indicador correspondente, inclusive com sua página oculta; não muda a tela nem os estados das pistas. Não há implementação de SINGLE/MULTI, troca de banco ou exclusividade de efeitos: preserva-se a simulação visual independente da etapa 4.

Na etapa 5B, as outras três telas de reprodução ainda estavam fora do escopo; foram adicionadas na etapa 5C.

## Etapa 5C: sete telas de reprodução

**Manual do Proprietário, página 8, ilustrações (3) FAIXAS DO LOOP, (4) STATUS DO LOOP e (5) NÍVEL DO LOOP**; ordem também conferida em **Parameter Guide, SETUP / DISPLAY MODE**:

1. MEMORY NUMBER
2. TRACK STATUS
3. LOOP TRACKS
4. LOOP STATUS
5. LOOP LEVEL
6. INPUT FX
7. TRACK FX

- LOOP TRACKS reutiliza os cinco ícones octogonais e acrescenta um setor preenchido para o progresso. A ilustração mostra preenchimento partindo da parte inferior e percorrendo a esquerda, topo e direita. O marcador da pista atual permanece visível sobre o preenchimento. Parado com frase usa contorno; vazio usa traço. TRACK STATUS conserva seus ícones estáticos anteriores.
- LOOP STATUS conserva cabeçalho e rodapé e mostra TRK, número e dois-pontos e estado em tamanho grande. O texto PLAY está ilustrado no manual; REC, OVERDUB e STOP seguem os estados visuais solicitados pelo usuário. OVERDUB recebe letras menores para caber no mesmo campo.
- LOOP LEVEL conserva cabeçalho/rodapé e tem cinco pares de barras verticais, com marcas de pico acima, na ordem das pistas. O par de barras reproduz o desenho do manual; não representa sinal estéreo real nesta etapa.

**Adaptações explícitas por ausência de áudio:**

- A seleção da pista mantém a regra anterior: último botão principal ou STOP acionado. Nenhum controle de seleção ou menu foi acrescentado.
- Somente progresso e nível usam dados gerados. REC/PLAY/OVERDUB/STOP, presença visual de frase e volume continuam vindo dos controles do painel.
- Cada pista ativa tem um ciclo simulado de oito tempos (4 s no BPM atual de 120). REC → PLAY → OVERDUB conserva a fase; STOP/BLANK zera. O ciclo não altera o estado automaticamente nem representa duração gravada.
- Os níveis variam de forma determinística por pista/canal e são multiplicados pelo slider da pista, com pico de decaimento visual. Pista parada, vazia ou slider em zero não mostra atividade. Não há entrada de microfone, análise de áudio ou processamento.
- `DisplaySimulation` mantém os dados separados do modelo de estados. Um timer JUCE de 30 Hz avança a simulação pelo tempo decorrido, inclusive com a página oculta; só solicita repintura periódica nas duas telas animadas. O timer para quando não há pistas ativas.

Na etapa 5C não havia edição de parâmetros; a etapa 5D acrescenta somente o editor visual de LPF descrito abaixo. Mixer, áudio, processamento de efeitos, outros menus e atalhos continuam fora do escopo.

## Etapa 5D: edição visual de INPUT FX A / LPF

**Manual do Proprietário, página 16 — Editar as configurações de FX de entrada/FX de pista**: EDIT abre a tela, knobs [1]–[4] modificam os campos apresentados e EXIT retorna à reprodução. A ilustração mostra cabeçalho INPUT FX:FX A-A, linha horizontal, valores acima de quatro ícones de knobs e legendas abaixo. No exemplo original, a primeira coluna é FX TYPE, seguida de RATE, DEPTH e RESONANCE. O manual usa páginas para os parâmetros restantes.

**Parameter Guide, página 34 — Input FX/Track FX List / LPF**:

- RATE: opções sincronizadas (compassos/notas) e faixa numérica 0–100. O padrão indicado é uma nota, não 50.
- DEPTH, RESONANCE e CUTOFF: 0–100, padrão 50.
- STEP RATE: OFF, opções sincronizadas e 0–100. Não incluído nesta etapa.

**Adaptações limitadas desta implementação:**

- Somente INPUT FX, banco A, efeito A, tipo LPF fixo. Sem seletor de tipo/banco, troca para outros efeitos, Modo Fácil ou outras páginas de edição.
- Para os quatro knobs controlarem parâmetros, a página única contém RATE / DEPTH / RESONANCE / CUTOFF. LPF aparece no cabeçalho, e CUTOFF foi trazido para esta página. É uma adaptação explícita da distribuição ilustrada, não uma reprodução idêntica da primeira página do hardware.
- RATE usa apenas sua faixa numérica 0–100 e inicia em 50 como exemplo. Não implementa notas/compassos. Os outros três iniciam em 50, conforme o guia. Valores inteiros e limitados; unidades não são inventadas (não há Hz ou porcentagem no visor).
- Arraste vertical e rodinha do mouse nos quatro knobs físicos alteram os números e indicadores no LCD imediatamente. Fora da edição, os knobs não modificam parâmetros. Não aceitam atalhos nem edição de texto.
- A página de reprodução anterior permanece guardada enquanto a edição está aberta. EXIT retorna diretamente a ela. EDIT repetido é idempotente; as setas não abrem outras páginas durante a edição.
- Parâmetros permanecem na memória da aplicação entre EXIT e nova abertura; não são salvos entre execuções, e WRITE continua visual.
- Estados das pistas, volumes, indicadores A/B/C/D e simulação continuam atualizando durante a edição. As alterações de parâmetros não têm efeito sonoro nem alteram o transporte.

## Etapa 5E: seleção de tipos de INPUT FX A

**Parameter Guide, página 34 — Input FX/Track FX List**: LPF, BPF e HPF compartilham RATE, DEPTH, RESONANCE, CUTOFF e STEP RATE. PHASER usa RATE, DEPTH, RESONANCE, MANUAL, STEP RATE, D.LEVEL, E.LEVEL e STAGE. STAGE aceita 4, 8 (padrão), 12 e BI-PHASE; D.LEVEL inicia em 100, E.LEVEL em 50 e STEP RATE em OFF. Os demais parâmetros numéricos, exceto RATE, iniciam em 50.

**Manual do Proprietário, página 16**: FX TYPE retorna ao primeiro campo; knobs [1]–[4] operam os campos mostrados, setas acessam páginas e EXIT retorna à reprodução. A primeira página coincide com o agrupamento ilustrado: FX TYPE / RATE / DEPTH / RESONANCE. As páginas seguintes seguem a ordem dos parâmetros no Parameter Guide, agrupados em quatro; o guia não ilustra a disposição exata de cada página seguinte.

- Ordem dos tipos: LPF → BPF → HPF → PHASER, conforme a lista do guia. Seleção limitada a INPUT FX A, banco A.
- Filtros: página 2 = CUTOFF / STEP RATE / vazio / vazio.
- PHASER: página 2 = MANUAL / STEP RATE / D.LEVEL / E.LEVEL; página 3 = STAGE / vazio / vazio / vazio.
- Knobs sem campo ficam inativos. Setas do editor param na primeira/última página, com indicação no cabeçalho. A navegação circular das sete telas de reprodução permanece.
- Cada tipo guarda seus próprios valores durante a execução. Mudar tipo atualiza campos, limites e posições dos knobs; não muda o ON/OFF, estados de pistas ou outros efeitos. A tela INPUT FX mostra o tipo escolhido no campo A (PHASER abreviado PHA/SER).
- EXIT retorna à tela de reprodução de origem. Reabrir mostra a primeira página do tipo escolhido, com valores mantidos. EDIT repetido não redefine página ou valores.
- RATE mantém a faixa numérica 0–100 e o inicial de exemplo 50 da etapa anterior. STEP RATE agora tem OFF e 0–100. As opções de notas/compassos do guia não estão implementadas; não há processamento/sincronismo de áudio.
- A distribuição adaptada da etapa 5D foi substituída por esta navegação com FX TYPE. Nenhum controle físico foi redesenhado.

## Etapa 5F: quatro slots independentes no banco A

**Parameter Guide, página 5 — INPUT FX / BANK A–D / FX A–D**: distingue banco, estado SW de cada efeito e FX TYPE/parâmetros atribuídos aos quatro botões. **Manual do Proprietário, página 16**: a identificação FX A-A corresponde ao banco/efeito; descreve selecionar o efeito mantendo EDIT pressionado. Nesta etapa, por solicitação explícita do usuário, basta clicar A/B/C/D com o editor aberto, sem exigir a combinação com EDIT.

- Banco fixo A, slots A/B/C/D. Cabeçalho INPUT FX:FX A-A / A-B / A-C / A-D, mantendo fonte e disposição existentes.
- Cada slot mantém seu tipo e uma coleção própria de valores para LPF/BPF/HPF/PHASER. Alterações não afetam outros slots, mesmo usando o mesmo tipo. A retenção é somente durante a execução.
- Trocar de slot apresenta sua página inicial e atualiza os quatro knobs; repetir o clique no slot atual mantém a página. Assim, sair da página 3 de PHASER para um filtro nunca deixa uma página inválida.
- Selecionar o slot não muda ON/OFF. O toggle automático dos botões INPUT FX é desativado apenas enquanto o editor está aberto, preservando o estado e desenho dos LEDs; ao sair, o comportamento de toggle retorna. TRACK FX não participa da seleção e permanece visual.
- EXIT volta à tela de reprodução de origem. Reabrir mantém o último slot selecionado, em sua primeira página. Cliques A/B/C/D fora da edição não mudam essa seleção.
- Tipos iniciais: A=LPF, B=BPF, C=HPF, D=PHASER, todos OFF. Os exemplos G2B/PATTERN SLICER/DELAY de B/C/D foram substituídos pelos tipos disponíveis; não há implementação desses efeitos antigos nem migração de dados persistidos (não existe persistência).
- A tela de reprodução INPUT FX reflete tipo e ON/OFF de cada slot independentemente. Os quatro knobs, parâmetros, limites, páginas e opções numéricas permanecem como na etapa 5E.
- Sem redesenho do painel, áudio, novos tipos, edição de outros bancos ou TRACK FX.

## Etapa 6A: dispositivos de áudio do Windows

Esta etapa é infraestrutura do aplicativo desktop, sem implementar menus de áudio do equipamento. O painel e as sete telas mantêm o comportamento anterior, inclusive os medidores simulados.

- `AudioSystem` possui um `AudioDeviceManager`, restrito a WASAPI compartilhado, e inicializa os dispositivos padrão do Windows. A seleção de entrada e saída é independente; a janela separada abre com clique direito na borda vazia do painel.
- O nome solicitado é mantido separado da configuração efetivamente aberta. Se o dispositivo desaparecer ou o JUCE substituir um endpoint pelo padrão, a conexão é fechada e a seleção original continua visível. Reconectar reaplica a seleção; nenhuma troca silenciosa é adotada pela aplicação.
- Erros de abertura, ausência de dispositivos e erro assíncrono do driver são tratados sem encerrar a interface. A atualização do estado ocorre na thread de interface; as notificações do callback usam apenas flags atômicas.
- O callback ignora a entrada e preenche todas as saídas com zeros. Não há monitoramento, gravação, reprodução de loops, análise de nível real ou efeitos. A conexão continua ao ocultar as configurações e termina ao encerrar o aplicativo.
- Referências de API: [AudioDeviceManager](https://docs.juce.com/master/classjuce_1_1AudioDeviceManager.html) e [AudioIODeviceCallback](https://docs.juce.com/master/classjuce_1_1AudioIODeviceCallback.html), conferidas também no código local da versão JUCE 8.0.6.

## Etapa 6B: monitoramento e F5

- O mesmo `AudioDeviceManager` da etapa 6A passa a abrir até dois canais de entrada e saída. Seu callback encaminha os canais de entrada com ganho de 0–1: mono duplicado, estéreo L/R, e média de L/R para saída mono. Canais ausentes/saídas extras recebem silêncio. Não há gravação, processamento de efeitos, filas adicionais nem ligação aos transportes visuais das pistas.
- Monitoramento inicia desligado; volume inicial 0,1 (−20 dB). As duas opções são atômicas e verificadas como lock-free na compilação. `juce::SmoothedValue` aplica rampa de 20 ms à ativação e ao volume. Desativar corta no próximo bloco. Estado do ganho suavizado pertence somente ao callback/ciclo de vida de áudio.
- Troca de dispositivos, reconexão, parada, erro e `audioDeviceAboutToStart` desligam o monitoramento. Isso também evita retomada automática com outro endpoint quando o backend JUCE reinicia por mudança no Windows. A ativação exige entrada e saída efetivamente conectadas.
- O tamanho de buffer é reduzido, quando possível, para o menor suportado com duração de pelo menos 5 ms. Um padrão já menor é preservado. Falha ao abrir restaura a configuração original; três interrupções detectadas em aproximadamente dois segundos restauram o buffer original e desligam o retorno. Toda reconfiguração acontece na thread da interface. Mantém-se WASAPI compartilhado; não há garantia de latência total nem de detecção de todas as falhas de drivers.
- F5 é tratado por `keyPressed` nas janelas JUCE, sem hooks nem `RegisterHotKey`. A janela é única e reaproveitada; o clique direito na borda vazia continua disponível. Somente a janela de configurações recebe novos controles, preservando o painel MK2.
- Referências de API: [AudioIODeviceCallback](https://docs.juce.com/master/classjuce_1_1AudioIODeviceCallback.html) e [SmoothedValue](https://docs.juce.com/master/classjuce_1_1SmoothedValue.html), conferidas no código local JUCE 8.0.6.

## Etapa 6C: gravação e reprodução real na pista 1

**Manual do Proprietário, página 5**: o botão principal indica REC em vermelho e PLAY em verde; STOP interrompe gravação/reprodução e o fader controla o volume da pista. Nesta etapa, por solicitação do usuário, não há overdub e a limpeza exige **Shift + clique em STOP com a pista parada**, em lugar de manter STOP pressionado como no equipamento.

- `LoopTrack` reutilizável, com uma instância ligada ao callback existente de `AudioSystem`. Buffer estéreo de 11.520.000 frames preparado na construção, fora do callback: até 60 segundos em taxas de até 192 kHz. Ao atingir o limite, começa a reprodução; nenhuma gravação vai para disco.
- Comandos passam por uma caixa atômica que não sobrescreve comandos pendentes. A limpeza verifica STOP tanto na interface quanto no processamento. Um snapshot atômico de estado/comprimento alimenta os LEDs e o visor a 30 Hz, sem operações de interface na thread de áudio.
- As transições ocorrem no início do bloco; o período do loop é exatamente o número de frames capturados. Tapers de até 3 ms nas extremidades e rampas de transporte/volume reduzem descontinuidades sem encurtar o período. Gravação de zero frames retorna a EMPTY; silêncio capturado é uma gravação válida.
- STOP preserva o conteúdo, inclusive ao interromper REC. Retomar começa no início. Clicar durante PLAY não altera o transporte. Trocas/falhas de dispositivo param e preservam o loop; mudança de taxa usa interpolação linear para preservar sua duração.
- O monitoramento continua independente. Entrada mono é duplicada, estéreo preservado e saída mono recebe a média dos canais. Sem entrada, um loop existente ainda pode tocar. O fader 1 controla somente sua reprodução, com suavização de 20 ms.
- Pistas 2–5 permanecem visuais. Progresso e medidores do LCD continuam simulados; os estados da pista 1 agora são reais. Painel, F5, configurações e editor de INPUT FX foram preservados.

## Etapa 6D: TRACK FX A / LPF real na pista 1

**Parameter Guide, página 34 — Input FX/Track FX List**: LPF está disponível nos dois grupos; CUTOFF e RESONANCE usam escala 0–100. O guia não especifica a conversão para Hz/Q nem o algoritmo DSP interno. O protótipo mantém os nomes/escalas, usando DSP JUCE, sem alegar equivalência sonora com o hardware. RATE, DEPTH e STEP RATE não foram implementados no processamento.

- `TrackEffects` é um insert reutilizável associado a `LoopTrack`, após a leitura/interpolação do loop e antes do fader e da soma ao monitoramento. Há apenas LPF de segunda ordem (`juce::dsp::StateVariableTPTFilter`) nesta etapa. Nenhuma escrita do efeito no buffer gravado.
- TRACK FX A liga/desliga. TRACK FX EDIT abre uma única página mínima de LPF, identificada TRACK 1, com knobs 1/2 para CUTOFF/RESONANCE. Knobs 3/4 e setas ficam sem função nessa página. EXIT retorna à tela anterior. Abrir INPUT FX EDIT fecha esse editor e vice-versa; os parâmetros e estados dos grupos são independentes.
- CUTOFF 0–100 corresponde a 40–18.000 Hz exponencial, limitado a 0,45 × taxa de amostragem. RESONANCE corresponde a Q 0,5–2,0; ganho 0,5/Q compensa o aumento de ressonância. Padrões do protótipo: OFF, CUTOFF 70, RESONANCE 0. O efeito não pode auto-oscilar por uma configuração de Q ilimitada.
- Snapshot de parâmetros em um inteiro atômico lock-free. Rampas de 40 ms nos parâmetros, atualização limitada de coeficientes a cada 16 amostras e crossfade de 20 ms no ON/OFF. O filtro continua aquecido em bypass; após a rampa, retorna as amostras secas exatamente. Estados de canal são alocados na construção, fora do callback. Prepare/reset preservam os controles e reinicializam o histórico DSP com o ciclo de vida do áudio.
- Compensação, verificação de finitude, proteção contra denormais e limite de pico ±1 no ramo filtrado. Não há locks, disco, alocação ou interface no processamento. Nenhum efeito sobre a captura, retorno direto do microfone ou pistas 2–5.
- Testes com sinais conhecidos verificam atenuação acima do corte, passagem de graves, isolamento estéreo, suavização, extremos/taxas de 8–192 kHz, bypass exato, preservação da RAM e monitoramento independente. O hook de alocação Debug/MSVC cobre também o DSP e a leitura dos parâmetros no processamento.

## Etapa 6D.1: DISTORTION configurável no TRACK FX C

- Roteamento confirmado: `TopPanelComponent` envia A/C e parâmetros pelo mesmo callback de controle a `AudioSystem::setTrack1Effects` → `LoopTrack::setEffects` → `TrackEffects`. A leitura/interpolação do loop passa por LPF A, DISTORTION C e fader; só então é somada ao monitoramento. Nenhuma alteração do áudio capturado ou do buffer original.
- Investigação: o primeiro algoritmo `0.5 * tanh(2*x)` era praticamente linear em sinais baixos. Entretanto, o código recebido nesta revisão já usava `0.25 * tanh(16*x)`: antes de modificá-lo, um teste com senoide de 750 Hz/pico 0,1 através do callback real de AudioSystem mediu H3/H1 = 0,131197 com C ligado. Portanto, não foi reproduzida falha de roteamento nessa versão; não é possível atribuir com certeza a causa da ausência de mudança ouvida pelo usuário sem reproduzir suas condições. A edição do slot C e seus quatro parâmetros ainda não existiam.
- DSP: `juce::dsp::WaveShaper<float>` com tanh, seguido por `FirstOrderTPTFilter` passa-baixas não ressonante para TONE. Valores 0–100: DRIVE = ganho 2–64 exponencial, TONE = 500–12.000 Hz exponencial (máximo 0,45 × sample rate), LEVEL = ganho 0–0,5 no ramo processado, MIX = proporção linear 0–100%. Padrões: OFF, DRIVE 60, TONE 65, LEVEL 50, MIX 100. São escolhas deste protótipo, não emulação do algoritmo BOSS nem implementação dos parâmetros completos do manual.
- Rampas de 40 ms nos quatro parâmetros e 20 ms em ON/OFF. Filtro TONE permanece aquecido durante bypass; estados de canal são preparados fora do processamento. Um snapshot atômico de 64 bits, lock-free em x64, transporta os dois efeitos e todos os parâmetros juntos. Coeficientes são atualizados somente quando mudam, a cada 16 amostras; sem alocações, locks, disco ou interface no callback.
- LEVEL limita o pico totalmente processado a 0,5 (0,25 no padrão). MIX 0 e C desligado retornam exatamente ao sinal anterior a C após as rampas, mantendo LPF A caso esteja ativo. A soma independente com o monitoramento mantém o comportamento anterior.
- C liga/desliga e seleciona C para EDIT. O editor pixelado mostra FX A-C DISTORTION, ON/OFF e DRIVE/TONE/LEVEL/MIX alinhados aos quatro knobs. As setas no editor alternam A/C sem mudar ON/OFF; clicar A/C continua alternando o respectivo efeito. EXIT conserva valores e retorna à tela de reprodução de origem. B/D permanecem visuais.
- Validação Debug x64: senoide de 750 Hz/pico 0,1 no callback de AudioSystem resultou em H3/H1 seco ≈ 1,1e-8 e processado ≈ 0,116354, demonstrando mudança não linear da forma de onda; bypass restaurou exatamente as amostras. Testes cobrem resposta de DRIVE/TONE, LEVEL, MIX, LPF+C simultâneos, estéreo, extremos/taxas 8–192 kHz, transições e ausência de alocações. Testes de interface cobrem knobs por mouse/rodinha, retenção, navegação, LED/ON/OFF, EXIT, transporte real com microfone, faders, limpeza, reconexão, monitoramento e F5. A captura real foi feita com fader zerado; o conteúdo da saída foi validado com sinais controlados no callback.

## Etapa 6E: overdub e um nível de Undo na pista 1

Referências: [estudo de comportamento](MK2_BEHAVIOR_REFERENCE.md), Manual do Proprietário pp. 5 e 9 e Parameter Guide p. 2 (DUB MODE). O atalho Ctrl+Z é a adaptação explicitamente solicitada nesta etapa; não implementa o modo UNDO/REDO nem MARK BACK do hardware.

- Transporte real: EMPTY → REC → PLAY ↔ OVERDUB. PLAY/OVERDUB compartilham posição, duração e processamento de reprodução, sem reiniciar transporte ou DSP nessa troca. O estado publicado pelo motor alimenta o LED existente, inclusive amarelo/laranja em overdub.
- A captura adiciona somente os canais de microfone à posição corrente da frase. Mono é duplicado; estéreo preservado. A leitura continua passando por LPF A, DISTORTION C e fader. Monitoramento continua independente. A duração original e os frames gravados permanecem fixos; em mudança de taxa, a entrada é integrada na grade de frames da gravação original. Não há compensação medida de latência do hardware nem alegação de equivalência sonora à BOSS.
- Uma operação começa ao entrar em OVERDUB e termina ao voltar a PLAY, pressionar STOP ou interromper o dispositivo. Ausência de entrada conclui o overdub e mantém PLAY quando a saída existe. STOP/retomada preservam a frase e o Undo; a retomada começa no início, como antes.
- Ctrl+Z na janela principal envia o comando pela mesma caixa atômica existente. Só é aceito em PLAY/STOP, com overdub concluído. Restaura a frase anterior à operação inteira, mesmo que tenha durado muitas voltas; outro Ctrl+Z não desfaz camadas mais antigas. Uma nova operação substitui o único nível disponível quando começa a escrever. Uma operação sem amostras não consome o Undo anterior. Limpar a pista invalida Undo. Sem REDO.
- `LoopStorage` reserva dois bancos estéreo e índices fora do callback: aproximadamente 176 MiB para os bancos na capacidade máxima. Snapshot copia no máximo 704 bytes de índices; Undo troca os índices. A primeira alteração de uma página copia 256 bytes de índices, e a primeira escrita de cada trecho copia 64 frames estéreo (512 bytes). Nenhuma transição copia ou limpa a frase inteira. Dados e índices pertencem à thread de áudio, sem locks ou thread auxiliar.
- Entrada do overdub suavizada em 5 ms; encerramento suaviza retrospectivamente as últimas 64 adições, preservando o material anterior. Soma linear em níveis normais, com joelho suave acima de 0,9 e pico abaixo de 1 no acumulador; não é normalização automática. Undo usa transição de 5 ms com interpolação limitada aos sinais envolvidos. O monitoramento somado à saída mantém seu comportamento anterior.
- Validação: Debug x64 e suíte `AudioDeviceLifecycle`, com casos específicos de transporte, duas camadas, várias voltas por operação, restauração estéreo, Undo parado, duração/posição contínuas, amostras/entradas ausentes, reinício de dispositivo, taxas diferentes, limites de ganho, limites de páginas/trechos e ausência de alocações no callback. Integração usa monitoramento ligado, ambos os efeitos e fader zerado durante overdub, depois compara o áudio seco com a soma esperada. Não foi realizada escuta nem validação interativa de teclado nesta etapa; Ctrl+Z e F5 devem ser conferidos manualmente.

Teste manual: grave a pista 1; clique para PLAY, novamente para OVERDUB e acrescente som por várias voltas; clique para PLAY. Faça outra camada e pressione Ctrl+Z: só a segunda operação deve desaparecer. Confira STOP/retomada e tente Ctrl+Z durante REC/OVERDUB (sem ação). Confira fader, LPF A/DISTORTION C, F5 e monitoramento; para limpar, pare e use Shift + STOP. O botão visual UNDO/REDO continua sem função nesta etapa.

## Etapa 6F-A: BPM real, TAP TEMPO e relógio musical

Referências: [estudo de comportamento](MK2_BEHAVIOR_REFERENCE.md), Manual do Proprietário p. 11 (TAP abre ajuste de andamento; knob 4; faixa 40,0–300,0) e Parameter Guide pp. 3–4 (sincronização/quantização, mantidas fora desta etapa). A tela TEMPO reutiliza a área e a tipografia do visor; não se presume reprodução de uma ilustração de tela que não está nessa página.

- `MusicalClock` pertence ao `AudioSystem` existente. O BPM em décimos é o único controle autoritativo; o modelo visual recebe uma cópia somente para exibição. O callback integra `amostras × BPM / (60 × sampleRate)`, com acumulação compensada e publicação atômica da fase. A posição fornece batida absoluta, compasso 4/4, batida 0–3 no compasso e fração. Não depende de timers gráficos, da pista 1 ou de monitoramento.
- Alterar BPM preserva a fase e passa a valer no bloco de áudio seguinte; o valor no visor muda imediatamente. Reiniciar/trocar dispositivo preserva a fase usando a nova taxa para os próximos blocos. Sem callback, ou após erro do dispositivo, a fase congela; editar BPM continua possível. Não há avanço pela hora do computador durante a desconexão.
- TAP usa `std::chrono::steady_clock` na thread da interface, no mouse-down. O primeiro toque estabelece referência; os seguintes estimam BPM pela média dos últimos até cinco intervalos, descartando os extremos quando há pelo menos três. Toques menores que 80 ms são ignorados como bounce; intervalos de 80–200 ms reiniciam a sequência; pausas maiores que 1,5 s também reiniciam, sem alterar o BPM vigente. Intervalos válidos são 200–1500 ms. Timestamps não finitos/negativos são ignorados; tempo não crescente reinicia a sequência. Ajuste manual limpa o histórico do TAP. O TAP não reposiciona o relógio nem inicia pistas.
- TAP abre uma tela TEMPO sobre a página/editor existente. Só o knob 4 fica habilitado para andamento: arraste com resolução de 0,1 BPM; rodinha altera 1 BPM por evento; Shift+rodinha altera 0,1 BPM. EXIT restaura a página/editor anterior, com parâmetros de efeitos preservados. EDIT de efeitos também pode sair de TEMPO. Não se implementou pressão longa para restaurar andamento, WRITE de memória, ritmo ou outros menus.
- O LED TAP pulsa com a fração da batida publicada pelo callback. A interface consulta os snapshots a 30 Hz, portanto o desenho tem resolução de atualização visual, enquanto o relógio é calculado pelas amostras. Sem dispositivo, o LED para de pulsar.
- LOOP TRACKS usa a posição real de reprodução/overdub da pista 1, embutida no mesmo snapshot atômico de estado e duração. Durante a primeira gravação, não existe ainda uma duração final para exibir progresso percentual. Pistas 2–5 mantêm transporte visual e mostram referência de oito batidas do relógio, sem alegação de loops reais. O timer visual permanece somente nos medidores de nível simulados; ele não calcula mais andamento ou progresso.
- O relógio não é passado ao processamento de `LoopTrack`. Não há mudança de velocidade/afinação, quantização ou correção automática de início/fim de REC. Tampouco há metrônomo, sincronização das pistas, novos efeitos ou outro AudioDeviceManager.
- Testes de tempo usam timestamps e amostras sintéticos, incluindo limites, pausas, bounce, irregularidade, ajuste decimal, mudança de taxa, fase contínua, compasso e partições de blocos. A integração grava livremente 4.093 frames com mudança de BPM durante REC e compara amostras de reprodução exatamente a 40/90/120/140/300 BPM. Também verifica parada, reconexão e posição real independente do BPM. Os testes de painel cobrem encaminhamento de TAP, knob 4/rodinha, limites, sete telas, EXIT, retorno ao editor FX e pulsação. Não se mede latência visual/acústica nem precisão de cliques humanos nesses testes.

Teste manual: com dispositivo conectado, pressione TAP repetidamente em um ritmo regular e observe o BPM. Na tela TEMPO, ajuste o knob 4 para 90,0/120,0/140,0; use Shift+rodinha para décimos. EXIT retorna. Grave uma frase de duração livre, altere o BPM enquanto ela toca e confirme que sua duração e afinação se mantêm; confira também overdub, Ctrl+Z, STOP/retomada, fader, efeitos, monitoramento e F5. O TAP pulsa no novo andamento mesmo com todas as pistas paradas.

## Etapa 6F-B: quantização da gravação inicial da pista 1

Referências: [estudo de comportamento](MK2_BEHAVIOR_REFERENCE.md) e Parameter Guide pp. 3–4, LOOP SYNC / IMMEDIATE e QUANTIZE. O hardware possui condições de sincronização e valores OFF/MEASURE distintos do controle simplificado ON/OFF pedido para o aplicativo. Esta etapa reproduz o comportamento solicitado de início imediato e fim por compassos, sem implementar LOOP SYNC completo, ritmo, MIDI ou TEMPO SYNC.

- QUANTIZE inicia OFF. A tecla Q, sem modificadores, alterna a opção somente na janela principal; não há atalho global. Repetição automática enquanto a tecla permanece pressionada não alterna várias vezes. F5 e Ctrl+Z permanecem independentes.
- Em OFF, REC/PLAY e a duração livre continuam como antes. Em ON, REC começa no bloco que recebe o comando. O pedido de PLAY agenda o primeiro limite inteiro de compasso 4/4 que ainda não foi ultrapassado, relativo ao início da gravação, e mantém a captura até lá. Se o pedido chega exatamente na fronteira, pode concluir imediatamente. Se chega depois, aguarda o compasso seguinte. Novos cliques durante WAIT não deslocam a fronteira.
- `MusicalClock::process` fornece o tamanho do compasso em frames com o mesmo BPM lido para avançar o relógio naquele bloco. `LoopTrack` fixa esse tamanho e a opção ao iniciar a captura. BPM/Q alterados durante REC passam a valer para a próxima gravação inicial; essa é uma política explícita do aplicativo para evitar que uma edição mova a fronteira pendente. O relógio global continua no BPM atual e nunca é reiniciado pela gravação.
- Para um BPM constante, o alvo é `ceil(N × sampleRate × 240 / BPM)` frames, com tolerância numérica muito menor que uma amostra. Não se arredonda cada compasso separadamente, evitando erro acumulado. Em 120 BPM e 48 kHz: 1 compasso = 96.000 frames/2 s; 2 compassos = 192.000 frames/4 s. A fase global não precisa coincidir com o início da gravação.
- A conclusão ocorre dentro do laço de amostras, inclusive no meio de um bloco: a amostra seguinte à última capturada já pertence a PLAY. Não há bloco silencioso adicional, redução/alongamento da frase ou alteração da velocidade. Os tapers e rampas existentes continuam reduzindo descontinuidades, sem mudar o período.
- O snapshot atômico agora inclui `awaitingBoundary` e `recordingQuantized`, junto do estado, comprimento e posição. Enquanto aguarda, o LED continua REC. O visor mostra Q ON/OFF e WAIT; se a opção tiver mudado durante uma captura ainda sem espera, mostra também a configuração efetiva dessa REC. No editor de efeitos, a notificação de Q ocupa temporariamente o cabeçalho por 1,5 s; WAIT permanece até concluir/cancelar. Os campos de parâmetros não são alterados. Na tela TEMPO e nas telas de reprodução, a indicação é persistente.
- STOP durante REC/WAIT cancela o agendamento e conserva os frames já capturados como frase parada, mesmo que não formem um compasso completo. Perda de entrada e reinício/troca de dispositivo seguem essa política de preservação. Retomar não requantiza a frase parcial. Mudança de sample rate conserva a duração pela reprodução já existente; não se continua uma única captura misturando taxas.
- O limite de gravação quantizada é o último compasso completo que cabe no buffer e no teto de 60 s. A 90 BPM, por exemplo, cabe automaticamente um máximo de 22 compassos (aproximadamente 58,667 s). Se uma capacidade de teste for menor que um único compasso, prevalece a segurança do buffer e a gravação termina nessa capacidade, sem preencher silêncio ou ultrapassar memória.
- Overdub, Undo, STOP/retomada, limpeza com Shift+STOP, fader, monitoramento e TRACK FX não recebem quantização. Nada é realinhado em gravações já existentes. O callback acrescenta apenas cálculos constantes no início/pedido de encerramento e comparações por amostra; não aloca memória nem utiliza locks.
- Validação Debug x64: suíte passou com QUANTIZE OFF, 1/2 compassos a 120 BPM, fronteira exata/um frame antes/depois, REC e PLAY no mesmo bloco, repetição periódica estéreo, BPM fracionário, cliques repetidos, mudanças de BPM/Q, STOP, entrada ausente, mudança de dispositivo/taxa, limite de 60 s e buffer menor que um compasso. Também passou overdub e Undo de uma frase quantizada, preservação do relógio global e dos testes anteriores. O monitor de alocações não detectou alocações no processamento. Os testes de visor verificam ON/OFF, configuração efetiva e REC WAIT; não foi realizada escuta nem teste físico da tecla Q.

Teste manual: selecione 120,0 BPM, pare e limpe a pista 1; pressione Q antes de gravar e confira Q ON. Inicie REC e solicite PLAY antes de 2 s: aparecerá WAIT e o loop terá 2 s. Solicite entre 2 e 4 s para obter 4 s. Durante WAIT, STOP deve conservar o trecho parcial e cancelar a espera. Para voltar à gravação livre, pare/limpe, pressione Q para OFF e grave outra frase. Confira também PLAY ↔ OVERDUB, Ctrl+Z, fader e efeitos.

## Etapa 6G-A: cinco pistas com áudio independente

Referências: [estudo de comportamento](MK2_BEHAVIOR_REFERENCE.md), Manual do Proprietário pp. 5, 9–10 e Parameter Guide pp. 2–4. Cinco conjuntos de transporte/fader e presença de frase são confirmados pelo manual. A seleção de Undo por TRACK + Ctrl+Z é a adaptação deste aplicativo, não o modo UNDO/REDO do hardware. Não há LOOP SYNC entre pistas.

- `AudioSystem` possui cinco instâncias da mesma `LoopTrack`, no mesmo callback/AudioDeviceManager. Cada uma mantém RAM, estado, posição, mailbox, fader suavizado, quantização latched por captura e um Undo próprios. `LoopTrack`, `LoopStorage`, `MusicalClock` e os algoritmos de efeitos não precisaram ser duplicados nem alterados.
- Qualquer pista: EMPTY → REC → PLAY ↔ OVERDUB; STOP preserva e retomar começa no início. Shift + STOP só limpa a respectiva pista quando parada. É possível capturar várias pistas ou fazer overdub enquanto outras tocam. A entrada somente leitura de todas é o microfone; nenhuma captura lê o barramento de saída. Fader e efeitos continuam exclusivamente na reprodução.
- TRACK seleciona a pista-alvo sem alterar o transporte. Principal e STOP também selecionam a pista acionada. O visor identifica **T1…T5** na faixa já usada por Q, mantém o marcador da pista atual e mostra a pista em LOOP STATUS. Nos editores FX, a seleção aparece temporariamente no cabeçalho por 1,5 s, preservando os parâmetros. Atualizações assíncronas de estado/progresso e movimentos de fader não roubam a seleção.
- Ctrl+Z envia Undo somente à pista selecionada no instante do atalho. Mudar de seleção depois de enviar não redireciona o comando. Se essa pista não tem Undo disponível ou está em REC/OVERDUB, nenhuma outra pista é usada como alternativa. Uma operação pode incluir várias voltas; sem REDO/histórico adicional.
- LEDs de transporte e presença de frase vêm dos cinco snapshots reais; TRACK aceso indica frase. LOOP TRACKS recebe cinco posições reais, sem a antiga simulação de oito batidas das pistas 2–5. Só os medidores de nível continuam simulados, conforme o escopo anterior.
- BPM/QUANTIZE são compartilhados, mas cada captura fixa sua opção e grade no próprio início. Q inicia OFF. WAIT/configuração efetiva no visor referem-se à pista selecionada. A quantização continua relativa ao início de cada captura e não alinha pistas entre si. O relógio avança uma única vez por bloco, independentemente da quantidade de pistas.
- LPF A e DISTORTION C continuam somente na pista 1, em série e sem alterar a RAM. Nas outras quatro instâncias os efeitos permanecem em bypass. Os FX inferiores continuam visuais; não foi implementada nova matriz de roteamento.
- Mixagem: acumulação em float, somando as reproduções ao monitoramento independente. Abaixo de pico 0,8, ganho unitário; acima, um joelho suave reduz a soma para pico estritamente abaixo de 0,98. O ganho é igual nos canais esquerdo/direito para preservar a imagem estéreo. Não há redução automática de ganho por número de pistas. A proteção final pode atuar também numa soma alta com monitoramento; nunca retorna ao áudio gravado.
- **RAM:** teto de 60 s por pista até 192 kHz, como antes na pista 1. Cinco pistas × duas versões × dois canais × 11.520.000 frames × float32 = 921.600.000 bytes (~879 MiB), mais ~1,73 MiB de índices COW: cerca de **881 MiB**, além do restante do aplicativo. Capacidade fixa evita redimensionar buffers ao trocar taxa/dispositivo. As dez versões são alocadas e tocadas antes de registrar o callback; não há cópia integral por overdub/Undo. O teto de áudio tem verificação em compilação (<900 MiB). Se houver `bad_alloc`, todas as alocações parciais são liberadas, o áudio não é aberto e o aplicativo exibe aviso e estado de memória insuficiente; liberar RAM e reiniciar. Taxas acima de 192 kHz mantêm o limite de frames, reduzindo a duração máxima.
- Trocas/falhas de dispositivo param e preservam todas as pistas e seus Undos. Reconectar não inicia reprodução nem monitoramento automaticamente. F5 e clique direito continuam abrindo a mesma janela.

Validação desta etapa: compilação Visual Studio 2022 **Debug x64** do aplicativo e `MK2AudioTests`, seguida de CTest **1/1 aprovado (15,24 s)**. Sinais sintéticos verificaram cinco gravações livres de durações diferentes, captura enquanto outras tocam, soma estéreo, faders individuais, overdubs de três voltas por pista, Undo direcionado mesmo trocando seleção após enfileirar, recusa de Undo durante overdub/limpeza enquanto toca, STOP/retomada/limpeza isolados, efeitos exclusivos da pista 1 e preservação após troca de dispositivo. Quantização testada com inícios desencontrados nas cinco pistas e fronteiras exatas de 96.000/192.000 frames (120 BPM, 48 kHz), cancelamento de uma espera sem parar as demais, e proteção da soma estéreo/mono. Testes dos controles verificaram encaminhamento, LEDs reais, seleção e faders. Passaram também os testes anteriores de transporte, duas camadas/Undo, limites, DSP LPF/DISTORTION, BPM, TAP, visor, monitoramento e dispositivos.

O detector CRT não encontrou alocações nos blocos/transições de cinco pistas após preparar o buffer interno do `AudioDeviceManager`. Esse buffer é alocado pelo próprio JUCE no primeiro envio; não é uma alocação adicionada ao nosso callback. Comandos sintéticos de zero frames são entregues diretamente ao callback de `AudioSystem`, porque o wrapper de dispositivo do JUCE exige blocos positivos e seu diagnóstico de Debug aloca strings. Não foram adicionados locks ao código de processamento. Os testes não medem latência física, microfonia, qualidade audível ou acionamento físico de F5/Ctrl+Z.

Teste manual:
1. Abra F5 e escolha entrada/saída; use fones e comece com monitoramento OFF. Grave uma frase em cada pista, deixando as anteriores tocar.
2. Em qualquer pista, principal alterna PLAY/OVERDUB; STOP para só ela; principal retoma. Mova cada fader e confira seu respectivo loop.
3. Adicione camadas em duas pistas. Clique em TRACK da desejada, confira T1…T5 no visor e use Ctrl+Z: apenas o último overdub dessa pista deve desaparecer. Clique em TRACK de uma pista sem Undo e confirme que nenhuma outra muda.
4. Pare uma pista e use Shift + STOP nela; confira que as demais mantêm o conteúdo.
5. Pressione Q antes de novas gravações; a 120 BPM, peça PLAY antes de 2 s para um compasso, ou entre 2–4 s para dois. WAIT conserva REC até a fronteira da própria pista. Q OFF retorna à gravação livre.
6. Compare TRACK FX A/C: só a pista 1 muda. Confira monitoramento independente e F5.

Arquivos da etapa: `Source/AudioSystem.*`, `Source/Main.cpp`, `Source/MainComponent.*`, `Source/TrackComponent.*`, `Source/TopPanelComponent.*`, `Source/MemoryDisplay.*`, `Source/DisplaySimulation.h`, `Tests/AudioSystemTests.cpp`, `Tests/TempoPanelTests.cpp`, `CMakeLists.txt`, `README.md`, `AGENTS.md` e estas referências. Sem alteração de desenho, PDFs, armazenamento sincronizado ou algoritmos `LoopTrack`/efeitos.

## Etapa 6G-B: LOOP SYNC, modo MEASURE

Referências consultadas antes da implementação: Parameter Guide oficial, **pp. 3–4**, inclusive as tabelas renderizadas, e [estudo de comportamento](MK2_BEHAVIOR_REFERENCE.md). Na p. 3, LOOP SYNC/MEASURE inicia REC/PLAY no começo do compasso e ajusta a duração em compassos; MEASURE/FREE permite comprimentos distintos. TEMPO SYNC é outra função, que adapta a velocidade/afinação ao andamento da memória. Na p. 4, REC/QUANTIZE aplica-se à gravação, não a overdub/reprodução; OFF nessa tabela ainda pode corrigir o fim conforme as condições ali enumeradas. Portanto, o antigo Q ON/OFF do aplicativo não representa integralmente aquele menu do hardware.

### Operação e limites adotados

- **L**, sem modificadores e somente com a janela principal ativa, alterna o LOOP SYNC das cinco pistas. Inicialmente OFF. Repetição automática da tecla é ignorada. Internamente há um SW individual por pista (`setLoopSync`); menus individuais não foram implementados.
- O rodapé do LCD mostra **LS ON/OFF MEASURE** para a pista selecionada. Durante agendamento mostra **WAIT REC**, **WAIT PLAY** ou **REC WAIT END**. Nos editores de efeitos, avisos usam somente o cabeçalho, conservando os campos dos knobs. Nenhum controle físico, efeito, janela ou geometria do painel foi acrescentado.
- Em ON, principal numa pista vazia agenda REC para a próxima fronteira de compasso; um pedido exatamente na fronteira pode começar imediatamente. Solicitar PLAY durante REC agenda a conclusão em um número inteiro de compassos. A amostra seguinte à última capturada já é reprodução, inclusive dentro do mesmo bloco. Repetir o principal durante uma espera não adia a operação nem inicia overdub por engano.
- STOP cancela uma espera de início. Durante REC, cancela a espera de fim e preserva o trecho parcial parado. Uma frase parcial que não caiba na grade mostra **INCOMPAT: L OFF** ao tentar reproduzir sincronizada; desligar LOOP SYNC permite ouvi-la livremente, sem alteração de seu conteúdo. Shift+STOP continua somente parado e por pista.
- Em MEASURE, retomar uma frase começa **do início da frase na próxima barra compartilhada**, sem reposicionar as outras pistas. Não espera o fim do maior loop e não implementa LOOP LENGTH. A origem do transporte compartilhado permanece; uma retomada pode mudar a associação do primeiro compasso daquela frase com o ciclo de uma frase maior, conforme a escolha da próxima barra.
- PLAY ↔ OVERDUB e Undo mantêm a posição/duração. Não há quantização de overdub. O Undo continua limitado à última operação da pista selecionada por TRACK/principal/STOP.
- Ativar ON numa frase livre já tocando **não faz seek**: **STOP FIRST** pede parar/retomar para aderir à grade. Para uma frase existente parada, exige-se que a duração original corresponda exatamente a um número inteiro de compassos da grade (apenas tolerância numérica de 0,00001 frame). Se incompatível, a operação é recusada e sinalizada; não há crop, preenchimento, regravação ou alteração de velocidade.
- **Q continua separado:** com LOOP SYNC OFF, mantém exatamente a gravação livre/quantizada da 6F-B, inclusive o arredondamento pelo BPM nominal quando outras pistas estão sincronizadas. Em LOOP SYNC ON/MEASURE, início e fim seguem o modo MEASURE independentemente de Q. O valor de Q não é alterado. Essa é a política explícita do escopo pedido, não a reprodução completa das prioridades dos menus REC/QUANTIZE, ritmo e MIDI do equipamento.
- Uma gravação iniciada mantém sua grade até terminar; desligar L durante REC não muda a duração já agendada. Desligar L durante uma espera de início a cancela. Desligar L durante playback mantém o cursor e continua livremente.
- BPM manual e TAP usam a mesma guarda atômica. Enquanto houver referência usada por captura/espera/frases sincronizadas, inclusive paradas, mudanças são recusadas: **BPM LOCK: L OFF**. Ajuste para o mesmo BPM é permitido. Desligue LOOP SYNC ou limpe as frases sincronizadas para alterar o BPM. Os loops preservam sua velocidade/afinação; reativar ON depois de uma alteração pode exigir voltar ao BPM original ou gravar frases compatíveis.

### Transporte, precisão e dispositivos

- O único `MusicalClock` continua integrando amostras no callback. A referência MEASURE acrescenta somente um período fixo e uma origem de compasso a esse relógio; não cria um contador/timer musical independente. Sua origem é estabelecida para a próxima barra quando a primeira operação precisa de sincronismo e conservada enquanto houver material vinculado, mesmo com tudo parado ou dispositivo desconectado. Limpar uma pista não remove a referência usada pelas demais.
- **BPM fracionário:** o período de um compasso é arredondado uma única vez para o inteiro de amostras mais próximo na taxa em que a referência nasce. Todas as novas frases usam múltiplos desse mesmo período, evitando que arredondamentos separados de 1/2/4 compassos se afastem após muitas repetições. O relógio avança nessa grade enquanto ela está vinculada. A aproximação é de no máximo meia amostra por compasso em relação ao BPM nominal; o valor nominal do visor não é reescrito. Essa decisão digital do protótipo é documentada, não atribuída ao algoritmo interno da BOSS.
- A posição de playback sincronizado é derivada da fase compartilhada no início/fim de cada bloco e avança normalmente entre amostras. A correção remove somente erro numérico; não reposiciona frases tocando para aderir a outro andamento. Os pontos de transição são atendidos no próprio laço de amostras, sem um bloco silencioso adicional.
- Troca/falha de dispositivo cancela esperas e para/preserva frases. O relógio conserva a origem e adapta o passo à taxa nova; a interpolação de mudança de sample rate já existente preserva a duração das frases antigas. Uma gravação interrompida não é continuada misturando taxas.
- **Limitação explícita na troca de taxa:** se a referência preservada não corresponder a um número inteiro de amostras por compasso na taxa nova, nova REC sincronizada é recusada com **RATE GRID: L OFF**. Frases antigas continuam podendo ser retomadas em MEASURE. Use OFF para gravar livremente, volte à taxa anterior, ou limpe/desvincule todas as frases para estabelecer uma nova referência. Não se aplica time-stretch nem se adiciona/remove amostra escondida a cada volta.
- O limite de RAM continua o da 6G-A; captura sincronizada termina no último compasso inteiro que cabe. Se nem um compasso couber, mostra **BAR > CAPACITY** e não grava uma frase truncada. REC de zero frames e cancelamento da última espera não deixam o BPM bloqueado.
- `LoopTrack`/`AudioSystem` mantêm os mesmos buffers, Undo COW, callback e AudioDeviceManager. Novos controles/snapshots são atômicos; agendamento/validação usam trabalho limitado e sem alocação, locks, arquivos ou componentes gráficos no callback. LPF/DISTORTION permanecem exclusivamente na pista 1 e sem alterações de algoritmo/roteamento.

### Verificação e teste manual

**Resultado final:** aplicativo e testes compilados em Visual Studio 2022 **Debug x64**; CTest **1/1 aprovado em 25,54 s**. A prévia renderizada do LCD foi inspecionada; glifos Q, W e > foram completados no mesmo alfabeto pixelado para os avisos não perderem caracteres. O detector CRT passou sem alocações no processamento após preparar o buffer interno do JUCE, como documentado na 6G-A.

Os testes focados exercitam o callback real com sinais sintéticos: cinco pistas de 1/2/4/1/2 compassos; durações de 96.000/192.000/384.000 frames a 120 BPM/48 kHz; esperas e cancelamento; retorno ao começo da frase na próxima barra; repetição de amostras após o ciclo comum; overdub/Undo com fase preservada; SW individual OFF; Q OFF/ON e arredondamento legado; bloqueio de BPM; frases incompatíveis; 301 ciclos de quatro compassos a BPM fracionário em teste acelerado a 1 kHz, além de casos a 44,1/96 kHz; troca de taxa, origem preservada, limite de capacidade, captura vazia e operação offline. Testes de LCD cobrem ON/OFF, MEASURE, espera e motivo de recusa. A suíte mantém as regressões da 6G-A e do DSP. Não se afirma qualidade por escuta nem equivalência completa ao hardware.

1. Com fones, escolha entrada/saída em F5 e ajuste **120,0 BPM**. Comece com pistas vazias/paradas e pressione **L**; confira LS ON MEASURE.
2. Inicie REC da pista 1: WAIT REC aguarda a barra. Depois de REC começar, peça PLAY antes de 2 s para obter 1 compasso. Na pista 2, peça entre 2–4 s para 2 compassos; na pista 3, entre 6–8 s para 4. Faça gravações também nas pistas 4/5.
3. Deixe todas tocar; teste STOP/retomada individual, overdub, TRACK + Ctrl+Z e faders. Use TRACK para conferir a espera/estado da pista desejada.
4. Durante WAIT REC/PLAY, pressione STOP: a operação deve ser cancelada. Durante REC WAIT END, STOP preserva o trecho parcial, que pode exigir L OFF para reproduzir.
5. Tente mudar o BPM: deve aparecer o bloqueio. L OFF libera o modo independente e permite a alteração; não muda o áudio existente. Confira também que TRACK FX A/C continuam afetando só a pista 1.

Arquivos: `Source/MusicalClock.h`, `Source/LoopTrack.h/.cpp`, `Source/AudioSystem.h/.cpp`, `Source/Main.cpp`, `Source/MainComponent.cpp`, `Source/VisualStyle.cpp` (glifos Q/W/> que faltavam aos avisos), `Source/MemoryDisplay.h/.cpp`, `Source/TopPanelComponent.h/.cpp`, `Tests/AudioSystemTests.cpp`, `Tests/TempoPanelTests.cpp`, `README.md`, `AGENTS.md` e referências técnicas. Sem implementação de IMMEDIATE, LOOP LENGTH, TEMPO SYNC, menus completos, FX individuais/INSERT, slots/bancos adicionais ou novos efeitos.

## Organização

- `DisplayModel.h`: dados da memória, ordem das sete páginas, estados/volumes das cinco pistas e dos dois grupos de quatro efeitos.
- `DisplaySimulation.h`: conserva posições reais publicadas pelo áudio; somente níveis e picos continuam simulados.
- `MemoryDisplay.*`: renderização, navegação e atualização das sete páginas do LCD; ícones reutilizados entre TRACK STATUS/LOOP TRACKS e efeitos entre INPUT/TRACK.
- `TrackComponent` → `MainComponent` → `TopPanelComponent` → `MemoryDisplay`: fluxo explícito das mudanças de estado.
- O mesmo fluxo encaminha os sliders a `MemoryDisplay::updateVolume`, sem alterar a pista atual.
- Botões A/B/C/D de `TopPanelComponent` → `MemoryDisplay::updateEffect`: lê o estado visual real do botão e atualiza o grupo/coluna correspondente.
- `VisualStyle::lcdText`: caracteres desenhados em pixels, independentes das fontes instaladas.
- `ParameterKnob` em `PanelControls.*`: slider rotativo JUCE reutilizando o acabamento existente; arraste/rodinha sem caixa de texto ou teclado.
- `TopPanelComponent::setInputEditing` conecta EDIT/EXIT e ativa os quatro knobs; `DisplayModel` guarda parâmetros e estado de edição sem mudar a ordem das sete telas.

- `InputFxEditor.h`: campos, limites, textos, páginas e valores independentes por slot e tipo, compartilhados entre visor e knobs.


## Etapa 6H: FX por pista e INSERT compartilhado

### Referência e delimitação

Consultados `references/RC-505mk2_Parameter_eng04_W.pdf`, páginas 2 e 6 (incluindo a tabela renderizada), e `docs/MK2_BEHAVIOR_REFERENCE.md`, seções 3.1/3.4. O PG p. 2 define FX OFF/ON por pista e informa que TRACK abre suas configurações. Na p. 6, **INSERT está dentro de FX A–D**, portanto pertence ao slot compartilhado, não à pista selecionada e não apenas ao banco. Os destinos são ALL e TRACK1–5. A ressalva sobre saídas MAIN/SUB/ROUTING não é implementada: o aplicativo continua com uma única saída estéreo/mono escolhida em F5. INPUT FX continua visual.

### Operação

- **FX de cada pista:** habilita/desabilita sua participação em TRACK FX; o LED e a configuração no LCD refletem o mesmo valor usado pelo motor. Inicialmente ON nas cinco pistas; os slots A/C continuam inicialmente OFF.
- **TRACK:** seleciona a pista (inclusive como alvo de Undo) e abre sua configuração mínima de FX no LCD. Knob 1 alterna FX OFF/ON; EXIT retorna às telas de reprodução. Não altera INSERT, parâmetros de efeitos ou transporte.
- **TRACK FX EDIT:** mantém os parâmetros e controles anteriores. Setas selecionam A (LPF) ou C (DISTORTION), sem alternar ON/OFF. Os botões superiores A/C continuam ligando/desligando seus slots.
- **ENTER no editor TRACK FX:** alterna entre parâmetros e INSERT. No campo INSERT, knob 1 (arraste/rodinha) escolhe ALL ou TRACK1–5 para o slot mostrado. Setas permitem editar o outro slot sem trocar seu estado ON/OFF. EXIT volta aos parâmetros; outro EXIT sai do editor. Esse acesso é uma adaptação mínima do protótipo, não a implementação integral da navegação/bancos do equipamento.
- A e C guardam destinos independentes em RAM. Ambos iniciam em TRACK1, preservando o resultado anterior da 6G-B; o padrão de fábrica indicado no manual é ALL. Selecionar TRACK2–5 **não** muda esses destinos.
- Não foram adicionados tipos, bancos, parâmetros DSP, menus gerais ou funções aos knobs grandes. B/D continuam visuais, como antes. Configurações continuam sem persistência entre execuções.

### Processamento e limites

`AudioSystem` possui agora um único `TrackFxRouting`, com um único LPF e um único DISTORTION em `TrackEffects`. `LoopTrack` conserva o armazenamento e transporte, exportando reprodução antes do fader e seu ganho suavizado; não possui mais cópias de processadores. Blocos internos fixos de 256 frames usam aproximadamente 15 KiB de scratch previamente reservado, independentemente do tamanho do callback do dispositivo. O relógio continua avançando uma única vez por callback e cada trecho recebe o deslocamento correto na grade.

- INSERT individual processa somente sua pista antes do fader. INSERT ALL processa a soma das pistas com FX habilitado, após seus faders. Pistas com FX OFF seguem pelo caminho seco. O monitor é somado separadamente, e gravação/overdub continuam recebendo exclusivamente o microfone original.
- Para o mesmo destino, a ordem é LPF A → DISTORTION C. Destinos individuais diferentes são independentes. Quando há destino individual junto com ALL, a inserção individual precede o barramento comum, inclusive C individual → A ALL. Essa topologia explícita evita duplicar processadores ou distribuir artificialmente uma saída não linear entre pistas. O manual consultado confirma os destinos, mas **não confirma a ordem interna do DSP BOSS nessa combinação mista**; a topologia é uma decisão do protótipo, não uma alegação de equivalência ao hardware.
- DISTORTION em ALL processa uma soma, não cinco distorções independentes. Assim, retirar uma pista do barramento pode mudar naturalmente a saturação da soma restante; isso não muda os estados FX das outras pistas. O nível final mantém a proteção de pico existente.
- FX por pista usa rampa de 20 ms. Troca de INSERT faz transição curta ao seco (20 ms), troca/limpa os estados dos filtros e retorna ao processamento (20 ms), sem carregar resíduos do destino anterior. As suavizações e limites dos parâmetros A/C permanecem. Esse breve bypass de troca abrange o conjunto compartilhado.
- Sem buffers dinâmicos, locks, cópias de loops, interface, arquivos ou nova instância de AudioDeviceManager no callback. O Undo continua por pista e os efeitos não modificam RAM gravada. F5, monitoramento, BPM, Q e L não mudaram de comandos.

### Verificação 6H

**Resultado final:** aplicativo e testes compilados com Visual Studio 2022 em Debug x64; CTest **1/1 aprovado em 67,56 s**. Prévias renderizadas do LCD de TRACK/FX e INSERT foram inspecionadas; os títulos identificam imediatamente a pista ou slot, sem o aviso temporário de Q encobri-los. Testes sintéticos no callback real cobrem cada slot em TRACK1–5, ALL como soma compartilhada, destinos independentes/mistos, FX OFF por pista, faders, captura simultânea com outras pistas/FX/monitor ativos, overdub/Undo, mono e ausência de entrada. A verificação de preservação de RAM antes atribuída ao processador dentro de LoopTrack foi transferida para esse teste de integração das cinco pistas. Permanecem os testes de resposta/bypass/harmônicos dos algoritmos, dispositivos, transporte, QUANTIZE e LOOP SYNC, além do detector de alocações no callback. Testes de interface exercitam FX físico, TRACK/configuração, ENTER/INSERT, knobs, seleção independente, parâmetros preservados e EXIT. Não houve validação por escuta nem comparação sonora com o hardware.

### Teste manual

1. Grave frases em duas ou mais pistas; abra F5 e mantenha o retorno opcional conforme necessário.
2. Ative TRACK FX A, abra seu EDIT e ajuste CUTOFF como antes. Pressione ENTER e escolha TRACK2 no knob 1. O LPF deve atingir somente T2 com FX ligado.
3. Clique TRACK3: deve abrir FX da pista 3 sem redirecionar A. Desligue/ligue FX em T2 e confira o LED e o som.
4. Abra novamente TRACK FX EDIT, selecione C pela seta direita, ative C e use ENTER/knob 1 para escolher TRACK3. A e C devem atuar nos destinos separados. Os quatro parâmetros de DISTORTION permanecem na página anterior.
5. Ajuste INSERT ALL e deixe várias pistas tocando. FX OFF em uma pista deve deixá-la seca; as outras continuam na soma processada. Experimente A e C simultâneos, seus faders, STOP/retomada, gravação em outra pista, overdub e Ctrl+Z no alvo escolhido por TRACK.

Arquivos alterados: `Source/TrackFxRouting.h` (novo), `Source/AudioSystem.h/.cpp`, `Source/LoopTrack.h/.cpp`, `Source/TrackEffects.h/.cpp`, `Source/MainComponent.cpp`, `Source/TrackComponent.h/.cpp`, `Source/TopPanelComponent.h/.cpp`, `Source/MemoryDisplay.h/.cpp`, `Source/DisplayModel.h` (comentário), `Tests/AudioSystemTests.cpp`, `Tests/LoopTrackTests.cpp`, `CMakeLists.txt`, `README.md`, `AGENTS.md`, `docs/MK2_BEHAVIOR_REFERENCE.md` e este registro. PDFs e arquivos sincronizados não foram alterados.


## Etapa 6H.1: INPUT FX ROBOT em tempo real

### Referências confirmadas

Consultados o Parameter Guide oficial preservado em `references/RC-505mk2_Parameter_eng04_W.pdf`, página 36 (tabela ROBOT também renderizada e inspecionada), página 5 (INPUT FX/INSERT) e página 2 (FX por pista), além de `docs/MK2_BEHAVIOR_REFERENCE.md`. A tabela define **NOTE C–B**, **FORMANT -50–+50** e **MODE 1/2**; os padrões destacados são C, 0 e 2. MODE 1 usa o algoritmo da série RC anterior e MODE 2 um algoritmo novo, mas o guia não publica seus métodos DSP. Não foram inventados controles de DRIVE, MIX, oitava ou outros parâmetros de ROBOT.

Base técnica da aproximação: Julius O. Smith, [Vocoder Analysis — Introduction to Digital Filters](https://www.dsprelated.com/freebooks/filters/Vocoder_Analysis.html): decomposição por filtros em bandas, extração de envelopes e ressíntese por osciladores. A implementação abaixo escolhe uma grade harmônica fixa, em vez de tentar reconstruir a frequência original. Essa escolha e os dois modos são adaptações do protótipo, não algoritmos atribuídos à BOSS.

### Ativação e controles

1. Em **F5**, escolha o microfone e os fones na saída e habilite o monitoramento existente. Seu volume permanece independente e conserva o valor ajustado. Ativar ROBOT não liga o retorno automaticamente.
2. Clique **INPUT FX EDIT**, depois **B** para selecionar o slot. B já contém BPF visual: sua seleção não foi substituída automaticamente e os valores anteriores permanecem guardados.
3. No primeiro encoder, ajuste **FX TYPE = ROBOT**. Os outros três passam a editar **NOTE**, **FORMANT** e **MODE**, por arraste ou rodinha. NOTE oferece as 12 classes cromáticas de C a B; FORMANT mostra valores com sinal, inclusive -50 e +50; MODE aceita somente 1 ou 2.
4. Pressione **EXIT** e clique **INPUT FX B** para ligar/desligar o efeito. No editor, A/B/C/D continuam apenas selecionando o slot sem alternar ON/OFF. Não é preciso gravar para ouvir ROBOT no retorno.
5. Para gravar com ROBOT, mantenha **FX da pista ligado**. FX OFF nessa pista grava o microfone seco, conforme a habilitação por pista do guia, sem desligar o efeito ouvido no monitoramento. Reproduzir o loop não o envia novamente ao ROBOT.

ROBOT pode ser escolhido em qualquer um dos quatro slots do banco A, com tipo e valores independentes. Não foi adicionado outro banco nem apagado o armazenamento dos tipos LPF/BPF/HPF/PHASER. Esses tipos de INPUT FX continuam visuais; os efeitos reais de TRACK FX não mudaram. Ao voltar de ROBOT para um tipo visual, seu DSP faz a transição ao seco. Selecioná-lo novamente recupera NOTE/FORMANT/MODE. Não há persistência entre execuções, como nas etapas anteriores.

### Roteamento e segurança

O único `AudioSystem` continua usando seu AudioDeviceManager e callback. A entrada selecionada passa pelos slots ROBOT ativos, em ordem A–D, uma vez por amostra. Esse resultado alimenta o monitor e, por meio da habilitação FX de cada pista, a captura inicial e o overdub. O retorno não usa um caminho seco paralelo adicional: apenas a transição curta ON/OFF mistura seco/processado. O monitor nunca entra no buffer gravado; TRACK FX continua exclusivamente na reprodução. Não foi criado ROBOT em TRACK FX.

A gravação utiliza o mesmo fluxo processado mesmo com monitoramento desligado. FX OFF na pista usa o microfone original, com rampa de 20 ms ao mudar a habilitação. Ausência de entrada conserva o tratamento anterior de REC/overdub e limpa os estados do vocoder; reconexão limpa estados, conserva frases e deixa o monitor desligado. Os nomes/dispositivos selecionados, faders e volume do retorno não são alterados pelo efeito. O protótipo aplica INPUT FX à entrada mono/estéreo selecionada; não acrescenta a matriz completa MIC/INST/MAIN/SUB/INSERT do hardware.

`RobotEffect` usa arrays fixos para 24 filtros de análise, envelopes estéreo, até 64 harmônicos e uma tabela de seno preparada antecipadamente. Coeficientes dos filtros são preparados com JUCE DSP fora do callback. Não há FFT, análise de pitch por janelas, lookahead, alocação, mutex, acesso a disco ou interface dentro do processamento. Dois buffers estéreo fixos de 256 frames reaproveitam os blocos internos existentes para entrada processada/captura. Controles via palavra atômica mantêm NOTE/FORMANT/MODE/ON coerentes por slot.

- NOTE e MODE têm suavização de 20 ms; FORMANT, 30 ms; ON/OFF, crossfade de 20 ms, com bypass seco exato após a rampa.
- O sinal processado é limitado suavemente a ±0,8 por canal antes da proteção final de soma já existente. Não se cria nenhum caminho de saída de volta à entrada. Isso não substitui o uso de fones para evitar microfonia acústica.
- Sem input, não há portadora autônoma audível. Os canais de análise são independentes; mono é duplicado como antes. Estados antigos não são reaproveitados após desconexão ou conclusão de bypass.

### Algoritmo e limitações explícitas

A voz é ressintetizada a partir do envelope espectral: **não é um pitch shifter transparente**. NOTE fixa a grade harmônica em C3–B3 (aproximadamente 130,81–246,94 Hz), escolha interna do protótipo porque a tabela oficial não informa a oitava. Não há controle de oitava adicional.

FORMANT remapeia o envelope entre bandas logarítmicas, sem transpor a portadora. O mapeamento interno -50/0/+50 corresponde a deslocamento de -0,75/0/+0,75 oitava do envelope. Ele pode mudar também o nível perceptivo como consequência da distribuição de energia, mas não é um simples ganho nem uma transposição da nota. A relação numérica entre FORMANT e deslocamento no hardware não é publicada e não foi presumida.

- **MODE 1:** envelope agrupado em 12 bandas efetivas e síntese com padrão harmônico de amplitudes uniformes, produzindo um caráter mais grosseiro/metálico.
- **MODE 2:** envelope de 24 bandas e pesos harmônicos derivados de pulso de 25%, com pré-ênfase para articulação (`sqrt(2) * sin(pi*h/4) / h^0.15`). O padrão de harmônicos e a resolução espectral diferem do modo 1. A troca interpola os dois resultados com fase contínua.

As duas opções são aproximações tecnicamente distintas, **não reproduções dos modos proprietários antigo/novo da BOSS**. A análise é causal, sem atraso de bloco adicionado; filtros e seguidores de envelope têm resposta temporal dependente da frequência (ataque nominal de 3 ms e relaxamento de 18 ms), além da latência dos dispositivos. Não foi medida a latência acústica de ida e volta. Fricativas, transientes de beatbox, vozes polifônicas e sinais fora da faixa analisada podem perder naturalidade/definição. A faixa de análise é 80 Hz a min(8 kHz, 0,42 × sample rate), e o custo aumenta com a taxa e o número de slots ativos. Não foi garantido prazo de callback em todos os computadores/taxas.

Não foi realizado teste de fala/canto/beatbox e escuta com microfone e fones físicos. Os testes comprovam transformação e roteamento; **qualidade vocal convincente, inteligibilidade subjetiva e fidelidade sonora à MK2 continuam sem validação por escuta**.

### Resultado dos testes

Aplicativo e testes compilados em **Visual Studio 2022, Debug x64**. CTest: **1/1 aprovado em 75,69 s**. O LCD foi renderizado e inspecionado com ROBOT, C#, +50 e MODE 1; glifos #/+ foram acrescentados ao alfabeto pixelado existente, sem alterar a geometria do painel.

- NOTE C/E/G/B: a energia migra da frequência de entrada para a grade harmônica selecionada.
- FORMANT -50/0/+50, com vogal sintética: centroides espectrais normalizados de **6,32499 / 11,6756 / 17,6572 harmônicos**, mantendo a frequência nominal da portadora; a mudança não se reduz a ganho.
- MODE 1/2: diferença entre espectros normalizados de **0,55533**, demonstrando alteração de distribuição harmônica além de volume.
- Bypass exato após suavização, silêncio, independência estéreo, limites e mudanças de parâmetros nas taxas 8/44,1/48/96/192 kHz; detector CRT sem alocações no processamento/atualização dos controles de ROBOT.
- Callback de produção com retorno ativo e nenhuma frase gravada; áudio processado vivo; um único caminho de retorno; captura processada e seca em pistas simultâneas; reprodução preservada sem reaplicar ROBOT; gravação com ROBOT OFF; overdub/Undo com TRACK FX ativo; falta de entrada e reconexão.
- Interface real → motor: seleção B, FX TYPE, valores e faixa assinada, rodinha, LCD, ON/OFF, seleção sem alternar o estado, EXIT e recuperação dos valores de BPF/ROBOT.
- Regressões existentes das cinco pistas, faders, LPF/DISTORTION/INSERT, dispositivos, monitoramento, BPM/TAP, QUANTIZE e LOOP SYNC passaram. F5 e sua implementação não foram alterados; não se afirma uma nova verificação manual desse atalho.

Para avaliação por escuta: com ROBOT em B e retorno ligado, fale/cante/faça beatbox; compare OFF/ON, C/E/G/B, FORMANT -50/0/+50 e MODE 1/2. Grave uma frase com efeito e outra sem, desligue o retorno e reproduza ambas. Confira que o efeito gravado permanece ao desligar B, que a frase seca permanece seca ao ligar B, e que TRACK FX ainda pode processar as reproduções de acordo com INSERT.

Arquivos: novos `Source/RobotParameters.h`, `Source/RobotEffect.h/.cpp` e `Tests/RobotEffectTests.cpp`; alterados `Source/AudioSystem.h/.cpp`, `Source/InputFxEditor.h`, `Source/MemoryDisplay.h/.cpp`, `Source/TopPanelComponent.h/.cpp`, `Source/MainComponent.cpp`, `Source/VisualStyle.cpp`, `Tests/AudioSystemTests.cpp`, `CMakeLists.txt`, `README.md`, `AGENTS.md`, `docs/MK2_BEHAVIOR_REFERENCE.md` e este registro. PDFs e materiais sincronizados permaneceram intactos.

## Etapa 6H.3A — OSC BOT no INPUT FX

Consulte [OSC_BOT_6H3A.md](OSC_BOT_6H3A.md) para parâmetros e padrões oficiais, acesso às duas páginas do visor, aproximações de síntese/modulação, integração antes da gravação e resultados de Debug x64. OSC BOT e ROBOT têm valores independentes por slot; selecionar o novo tipo não apaga configurações anteriores. FX SEQUENCE e presets permanecem fora do escopo.


### Atualização de implementação — etapa 6H.3B

FX SEQUENCE controla NOTE de OSC BOT por slot A–D do banco A, com até 16 passos no callback e referência do MusicalClock. O LCD existente edita SW/SYNC/RETRIG/TARGET/RATE/MAX/VAL1–16 e INPUT MODE SINGLE/MULTI. 2MEAS é a duração de cada passo (oito batidas em 4/4). Slots adjacentes de OSC BOT em MULTI têm mistura paralela para conservar as duas vozes. Consulte [FX_SEQUENCE_6H3B.md](FX_SEQUENCE_6H3B.md) para presets de validação, acesso, testes Debug x64 e limitações explícitas: curva RATE numérico local, interpretação de SYNC/RETRIG, prioridade de frase e topologia MULTI não verificadas em hardware. Nenhum preset é carregado automaticamente, e não há novas observações do vídeo ou alegação de fidelidade sonora exata.


### Atualização de implementação — etapa 6H.3D

STEP SLICER é um INPUT FX real, antes de monitoramento/captura, com RATE, MAX, DEPTH, compressão e LENGTH/LEVEL independentes nos 16 passos. O seletor mínimo BANK A–D foi autorizado e acrescentado à última página dos editores; parâmetros, ON/OFF e estados de cada banco/slot são independentes. O painel e os atalhos existentes foram preservados, sem novos LEDs. Consulte [STEP_SLICER_6H3D.md](STEP_SLICER_6H3D.md) para arquitetura, acesso, testes, topologia OSC A/B → SLICER C → REVERB D, aproximações de RATE/compressor e limitações. Não há novas observações do vídeo nem validação auditiva física.


## Etapa 6H.4 — indicadores e diagnóstico da gravação

PG p.30 define o anel externo independentemente do botão REC/PLAY/DUB: STATUS (padrão) fica apagado vazio, aceso parado com frase, pisca no tempo em REC/DUB e mostra um compasso em PLAY. LOOP POSITION fica apagado em vazio/STOP/REC e acompanha a posição real em PLAY/DUB. LEVEL fica apagado em vazio/STOP/REC e mede reprodução em PLAY/DUB.

AudioSystem publica `TrackIndicatorFrame` coerente e lock-free no fim de cada callback; MainComponent apenas lê a 30 Hz. Níveis por canal são pós-fader/INSERT individual, pré-bus ALL e pré-monitor, com release de 150 ms. LOOP LEVEL deixou de ser simulado. MENU abre a página mínima SETUP/INDICATOR; knob 1 seleciona modo, EXIT restaura a página anterior. Sem árvore completa de menus ou persistência em disco.

G2B MODE 2 teve correção do alinhamento das duas cabeças/janelas de leitura: extensão de dois períodos e separação de um período de entrada. Reset invalida histórico em O(1). Não foram modificados DSP dos demais efeitos, atalhos F6–F10 nem o armazenamento/duração dos loops. A latência física relatada também sem efeitos **não foi considerada resolvida**; F5 mostra buffer e latências reportadas pelo driver. Diagnóstico, testes, limites e instruções: [TRACK_LEDS_RECORDING_6H4.md](TRACK_LEDS_RECORDING_6H4.md). ATTACK do OSC BOT continua pendente.


## Atualização de implementação — etapa 6I

Os dois knobs grandes agora controlam parâmetros reais dos efeitos existentes, por FX TARGET independente de seleção de pista/slot de edição. INPUT mantém alvo por banco A–D; TRACK mantém banco A e processamento A/LPF e C/DISTORTION. MENU/SETUP ganhou a página mínima FX KNOB MODE compartilhada: IMMEDIATE (padrão) e HOOK com captura. As atribuições seguem o pictograma de knob do Parameter Guide, não a estrela de FX SEQUENCE: OSC BOT controla BALANCE; REVERB, E.LEVEL. Limites, uso, arquivos e resultados estão em [FX_KNOBS_6I.md](FX_KNOBS_6I.md). Não foram criados DSP adicionais, alterados LEDs/atalhos nem corrigido ATTACK do OSC BOT.
