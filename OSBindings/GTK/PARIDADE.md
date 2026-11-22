# Paridade de funções: Clock Signal para Mac → GTK4/Libadwaita

Lista tirada do código do frontend do Mac (`OSBindings/Mac`). Cada item tem o
equivalente GNOME planejado e a fase em que entra. `[x]` = feito.

## Abrir e gerenciar máquinas
- [x] Abrir arquivo (Ctrl+O) com detecção automática da máquina — fase 1
- [x] Arrastar arquivo para a janela: abre (sem máquina) ou insere mídia (com máquina) — fase 1
- [x] Várias máquinas ao mesmo tempo, uma janela por máquina (documentos do Mac) — fase 1
- [ ] Abrir recentes (Open Recent) — fase 4
- [ ] Associação de tipos de arquivo (disquetes, fitas, cartuchos: .dsk, .tzx, .adf, .woz, .st, ...) — fase 5
- [x] Inserir mídia com a máquina ligada (Insert…) — fase 1
- [ ] Recarregar automaticamente quando o arquivo muda no disco (reinsere mídia ou reinicia a máquina) — fase 4
- [ ] Reverter para o salvo (reabrir o arquivo do zero) — fase 4
- [x] Fechar janela para a máquina (Ctrl+W) — fase 1

## Nova máquina (Machine Picker)
- [ ] Janela "Escolher uma máquina" com as 20 máquinas: Amiga, Amstrad CPC, Apple II, Apple IIgs,
      Archimedes, Atari ST, BBC Micro, Commodore TED (Plus 4), Electron, Enterprise, Macintosh, MSX,
      Oric, PC Compatível, Tandy CoCo, Thomson MO, Vic-20, ZX80, ZX81, ZX Spectrum — fase 3
- [ ] Todas as configurações de cada uma (modelo, memória, região, controladora de disco, DOS,
      segundo processador, Mockingboard, MSX-MUSIC, BeebSID, unidade C-1540/1541, CGA/MDA, etc.) — fase 3
- [ ] Lembrar a última escolha e as configurações de cada máquina — fase 3

## ROMs
- [x] Usar ROMs embutidas quando o núcleo tem — fase 1
- [x] Pasta de ROMs do usuário (~/.local/share/clksignal/ROMImages, compatível com a do Qt) — fase 1
- [x] Janela de ROMs faltando, com a lista e área para arrastar e soltar a ROM (confere CRC e copia) — fase 2
- [x] Mensagem de erro quando a ROM arrastada não é a esperada — fase 2
- [x] ROM com cabeçalho ou sobra no fim também é aceita (o Mac corta e confere o CRC) — fase 2
- [x] Extra do Linux: lista item por item (todas/qualquer uma/opcional), com tamanho e CRC — fase 2
- [x] Extra do Linux: soltar várias ROMs ou uma pasta inteira de uma vez, com aviso de quantas entraram — fase 2
- [x] Extra do Linux: botão e menu "Abrir pasta de ROMs" — fase 2

## Vídeo
- [x] Tela com o renderizador OpenGL do núcleo (efeitos de sinal composto/CRT) — fase 1
- [x] Manter a proporção da tela da máquina ao redimensionar (a imagem fica com barras pretas; o GTK4 não permite travar a proporção da janela como o Mac) — fase 1
- [x] Tela cheia (F11) — fase 1
- [ ] Tipo de sinal: TV (composto), composto monocromático, S-Video, monitor RGB (conforme a máquina) — fase 4
- [ ] Recorte dinâmico (Dynamic Crop) — fase 4
- [ ] Salvar captura de tela em PNG na pasta Imagens — fase 4

## Som
- [x] Saída de áudio com baixa latência, mono/estéreo — fase 1
- [ ] Controle de volume que aparece sobre a tela (como o HUD do Mac) e é lembrado — fase 4
- [x] Acompanhar mudança de clock do alto-falante da máquina — fase 1

## Entrada
- [x] Teclado físico (mapeado por posição) — fase 1
- [ ] Teclado lógico (mapear caracteres digitados por símbolo) — fase 4
- [x] Teclado como joystick — fase 4
- [ ] Colar texto (Ctrl+V digita o texto na máquina) — fase 4
- [ ] Gamepads/joysticks USB (Joystick Manager do Mac) — fase 4
- [ ] Mouse: capturar ao clicar e soltar com atalho (aviso no título) — fase 4
- [x] Soltar todas as teclas ao perder o foco — fase 1

## Controles da máquina
- [x] Reset suave e reset completo — fase 1
- [ ] Apple II: colorido/monocromático e pixels quadrados — fase 4
- [ ] Atari 2600: Reset, Select, preto e branco, dificuldade dos jogadores — fase 4
- [ ] ZX80/81: tocar fita, controle automático do motor, carregar fitas rápido — fase 4
- [ ] Oric: composto/monocromático/SCART, carregar rápido — fase 4
- [ ] Macintosh: iniciar rápido — fase 4
- [ ] Carregar rápido (Load Quickly) nas máquinas que suportam — fase 4

## Atividade
- [ ] Painel com os LEDs das unidades (acender, apagar e piscar), mostrado só quando há LEDs — fase 4

## App
- [x] Sobre (créditos ao Tom Harte, licença MIT, port não oficial) — fase 1
- [ ] Ícone, .desktop e metainfo (AppStream) — fase 5
- [ ] Traduções pt_BR, en, es (interface atual já traduzida; falta revisar no fim) — fase 5
- [ ] AppImage — fase 1 (prévia) e fase 5 (final)

## Fora do escopo (só existe por ser macOS)
- Menus Serviços, Ocultar, Trazer tudo para frente; Copy do menu Edit sem função no Mac.
- "Save…/Save As…" estão no menu do Mac, mas o próprio Mac não implementa (lança erro).
