# 6I.1 — Bancos e seleção de slots FX

Referência: [Owner’s Manual, páginas 4–5, 11 e 16](https://static.roland.com/assets/media/pdf/RC-505mk2_eng03_W.pdf). A página 16 distingue seleção de banco por EDIT longo e seleção de efeito por EDIT + A–D. As páginas 4–5 definem OFF, RED, PINK e BLUE. O tempo de 650 ms usado aqui é uma escolha da interface virtual, não uma especificação do equipamento.

## Como usar com um único mouse

O texto impresso EDIT e o botão circular logo abaixo aceitam a mesma interação, tanto em INPUT FX como em TRACK FX.

1. Clique curto em EDIT: abre a edição do slot selecionado.
2. Mantenha EDIT pressionado por pelo menos 650 ms, até o LCD mostrar BANK SELECT. Solte o mouse. O banco atual fica azul.
3. Clique A, B, C ou D: seleciona imediatamente o banco daquela seção, restaurando seus parâmetros e estados ON/OFF no motor existente.
4. Clique EDIT novamente: confirma e deixa o modo de bancos, permanecendo no editor.
5. Com o editor aberto, clique A–D para escolher o slot sem mudar ON/OFF. Use os quatro encoders e as setas como antes.
6. EXIT retorna. Fora da edição, A–D ligam/desligam os efeitos do banco ativo.

Exemplo INPUT FX B-C: segure INPUT EDIT, solte, clique B, clique EDIT para confirmar, clique C. O visor identifica B-C. Para D-D, escolha banco D e depois slot D. Repita na seção TRACK FX para seus bancos independentes.

Alternativa à combinação física de dois botões: pressione EDIT e arraste até A–D antes de completar os 650 ms; solte sobre o slot. O editor abre nesse slot, sem alternar ON/OFF. O deslocamento cancela a contagem do long press. Se o azul já apareceu antes do arraste, A–D continuam sendo bancos, até confirmar com EDIT. Arrastar para uma área vazia cancela o clique curto. EXIT também sai de BANK SELECT, conservando o banco já escolhido.

## LEDs e estado

- OFF: efeito desligado; anel sem brilho luminoso.
- RED: efeito ligado.
- PINK: efeito ligado selecionado para edição; fora do editor, efeito ligado atribuído ao FX TARGET. Um slot desligado permanece apagado mesmo selecionado.
- BLUE: banco atual durante BANK SELECT; os outros três ficam apagados. Os estados reais ON/OFF continuam armazenados e voltam a aparecer ao confirmar.

O LED deriva do modelo de estado; JUCE não alterna o botão antecipadamente. Trocar banco não altera FX TARGET de outra seção nem o alvo de Undo. A identidade de captura HOOK inclui o banco, inclusive em TRACK FX. Durante BANK SELECT os encoders ficam temporariamente indisponíveis para impedir alterações acidentais.

## Correção e limites

Antes, EDIT tinha somente onClick, sem long press; o texto impresso não recebia cliques. INPUT possuía os quatro bancos, mas dependia da última página do LCD. TRACK possuía somente um conjunto de parâmetros, títulos fixos A-A/A-C e edição limitada a A/C. A cor rosa de D era fixa, e até botões desligados tinham brilho.

Agora, INPUT continua usando os 16 slots e instâncias de áudio existentes. TRACK mantém quatro configurações independentes em RAM, restaurando parâmetros, ON/OFF, INSERT, slot selecionado e FX TARGET ao trocar banco. Somente o banco ativo alimenta os processadores compartilhados já existentes. Não foram criados processadores, alterados algoritmos DSP, menus gerais ou seleção completa de tipos da etapa 5G.

Em cada banco TRACK, A continua LPF e C continua DISTORTION. B/FLANGER e D/GRANULAR DELAY são os placeholders visuais preexistentes: acessíveis, com estado próprio e identificação correta, mas o editor informa NO DSP e não permite alterar por engano parâmetros do LPF. Não representam novos efeitos funcionais. As configurações permanecem em RAM durante a execução, sem nova persistência em disco. O histórico interno de filtros não é salvo por banco TRACK: a cadeia existente aplica sua suavização ao receber a configuração ativa.

## Validação

Os testes 6I.1 em MK2AudioTests exercitam eventos de mouse de EDIT, timer de long press, soltura, arraste e cancelamento; percorrem os 16 slots de cada seção; verificam parâmetros/ON-OFF independentes, INSERT, FX TARGET, cores e identificadores; trocam bancos durante reprodução e medem o áudio resultante de LPF/DISTORTION. O alvo `--fx-banks` permite executar somente esse grupo. A suíte completa mantém a cobertura de áudio e dos knobs 6I.

Resultados de execução registrados após a compilação e a validação final. Os sinais sintéticos e snapshots da interface não substituem escuta física com microfone/fones.
