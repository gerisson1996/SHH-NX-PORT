# Perfil do port: Silent Hill: Homecoming (shh-nx)

Segue o "Mapa universal de otimização, ports Xbox 360 -> Switch" (v1.0). Uma mudança por vez; cada item tem
evidência antes e depois, e só fica se for medido e repetível. O plano técnico do renderizador está em
`docs/plano-renut.md`.

## Fase 0: a base (2026-10-29)

| Campo | Valor |
|---|---|
| Jogo | Silent Hill: Homecoming, Xbox 360 (arquivos do próprio usuário, `assets/game_root`) |
| Repositório | `C:\dev\shh-nx` (git); versão de referência: commit `1af54758` |
| Base | ReXGlue (recompilação estática do PowerPC), SDK em `sdk/` |
| Engine | Nuts & Bolts (Double Helix): Direct3D do Xbox 360, tiling preditivo em 2 tiles (EDRAM) |
| Gráficos no PC | Renut (renderizador no nível do Direct3D) sobre plume/D3D12; referência: Xenos emulado |
| Gráficos no Switch | Renut sobre plume/deko3d, sem driver Vulkan (`REXGLUE_SWITCH_NO_NVK`) |
| Shaders | XenosRecomp -> SPIR-V (DXC do Vulkan SDK 1.9.0.5512) -> GLSL -> pacote; uam compila variantes no console, cache em `sdmc:/switch/shh-nx/deko3d_shaders.bin` |
| Toolchain Switch | devkitA64 GCC 16.1.0, Release `-O3`, `-mearly-ra=none` |
| Toolchain PC | clang++ (Release) |
| Build | `tools\build_switch.bat <nvk_sdk> nonvk` -> `out/switch-deko/shh.nro`; ELFs em `out/switch/elf_archive/` pelo hash do NRO |
| Modo | portátil, 1280x720 (1x) |
| Meta | 30 fps estáveis |
| Estado | jogável: menu e gameplay no console |

## Fase 1: linha de base (console, portátil)

Cena de referência: boot -> título -> menu -> pesadelo do hospital (o mesmo trecho do `--input_script` do PC).

| Medida | Valor | Fonte |
|---|---|---|
| Boot até o título | 24,8 s (PC 25-28 s: logos e aviso do próprio jogo) | `shh_001 (4).log` |
| Menu / título | 30 fps; render thread 0,78 ms, GPU 2,73 ms por quadro | idem |
| Gameplay | 17-24 fps (relato do usuário) | falta log da cena pesada |
| CPU no boot | 57% de 400%; thread principal 65% esperando no kernel | `rex_profile (3).log` |
| I/O no boot | 8722 leituras, 48,8 MB em 1,2 s (15 s de janela) | `shh_001 (4).log` |

Falta: três repetições da cena de gameplay com o profiler (FPS médio, 1% low, CPU/GPU por quadro).

## Itens (prioridade do mapa)

### SHH-01: sombras dinâmicas ausentes (P1, recurso ausente)
- Observado: no pesadelo do hospital a lâmpada pendurada escurece a parede no Xenos; no Renut a parede fica
  iluminada por igual. No PC e no console.
- Hipótese inicial: caminho do shadow map (draws só de profundidade, resolve para textura, leitura com comparação).
- Teste (PC, `--input_script=25:START,29:A,33:A,37:A,41:A,45:A`, captura aos 60 s, comparada com o Xenos):
  - build das 09:12: parede iluminada por igual, sem sombra (`out/jogo_loc_60s.png`);
  - build atual, `sa2_renut_colour_handover` desligado: sombra igual à do Xenos (`out/sh_nohand_60s.png`);
  - build atual com o padrão (ligado), controle: sombra igual à do Xenos (`out/sh_hand_60s.png`, `out/sh_cmp2.png`).
- Conclusão: o colour handover não é a causa; as sombras voltaram com as mudanças ainda sem commit desde as 09:12
  (o mais provável: os hooks de BeginTiling/EndTiling de `shh_draw.cpp`, que também corrigiram a faixa branca).
- Estado: corrigido no PC; falta confirmar no console (a mesma cena, comparada com o print do PC).

### SHH-02: 17-24 fps na gameplay (P2)
- Linha de base (NRO `80c7a92f`, `shh_001 (6).log`, hospital, ~5 min, GPU com overclock de 537-921 MHz):
  24-30 fps; **GPU 20-42 ms por quadro**, render thread 4-9 ms, CPU total 110-183% de 400%.
  Antes (12:27, sem `shh_renut_bound_fetch_rows`): render thread 35-63 ms, GPU 30-58 ms, 15-26 fps.
- Por quadro no pior trecho (23,5 fps): ~390 draws do jogo, ~740 no deko3d, ~110 só de profundidade, ~14 resolves
  de cor, ~62 uploads de textura, ~7 blits; upload ring 4 MB.
- Gargalo: **GPU** (a render thread cabe com folga em 33 ms; o usuário também vê 30 fps estáveis com overclock de GPU).
- Próximo teste: sensibilidade à resolução (`renut_resolution = "480p"`): se o tempo de GPU cair bem, o custo é de
  pixel (passes de tela cheia, sombras, SSAO); se não cair, é por draw/estado.
  - 1ª tentativa (`shh_004.log`): inválida. O pacote GLSL tem o passe `scale1` (só 1280x720), então o jogo ficou
    em 1x ("made for 1280x720 only"). Mesmos 16-30 fps, GPU 17-57 ms.
  - 2ª: pacote sem `scale1`/`fragscale` (`out/deko3d_shaders/pkg_scale/deko3d_glsl.bin`, 893 shaders, todos
    compilam no uam). As variantes novas compilam no console na primeira vez: medir a partir da segunda rodada.
    Resultado (`shh_001 (7).log`): ainda 1x. O renderer só escala quando todos os pacotes em uso aceitam, e o pacote
    embutido no NRO tinha `scale1`. Serviu de medida de clock: GPU fixa em 614 MHz -> GPU 30-48 ms (a ~900 MHz:
    20-30 ms), o tempo acompanha o clock.
  - 3ª: NRO `b146bff6` com o pacote sem `scale1` embutido (o antigo guardado em
    `app/out/renut_switch/scale1_backup/`). Resultado (`shh_001 (8).log`): ainda 1x. Causa: `PackageScales()` procura o
    código de escala de textura (`>> 26u) & 7u`), que nenhum dos 893 shaders do SHH lê; o SHH só lê o
    `resolution_scale` (127 shaders). A detecção passou a aceitar esse também.
  - 4ª: NRO `31187d7e`, `renut_resolution = "480p"` (`shh_001 (9).log`: "853x480 (0.667x)"), GPU fixa em 614 MHz,
    mesmo trecho: **GPU 17-33 ms, 27,8-29,9 fps**. Em 720p, no mesmo clock: GPU 30-48 ms, 21-29 fps.
  - Conclusão: com 44% dos pixels a GPU cai ~35-40%: boa parte do custo é por pixel (passes de tela cheia, sombras,
    que seguem a resolução), o resto é por draw. 480p a 614 MHz quase segura 30 fps.
- Estado: em diagnóstico.

### SHH-03: boot longo (P2)
- Evidência: título em 24,8 s; só ~6 s são do port (tela preta antes do logo, ~4 s lendo o `SHADERS.PAK`).
  Entre o `SHADERS.PAK` e o primeiro swap passam ~7,4 s.
- Diagnóstico: o perfil no console revelou que o worker de prebuild de pipelines usa apenas 0,4% de CPU,
  descartando contenção de pipelines como causa dos 7,4 s. A thread principal do jogo passa o tempo
  em inicialização interna e esperas de sincronização da engine do jogo.
- Mudança: `shh_fast_boot` otimizado: período de clique de A reduzido de 0,5 s para 0,25 s (hold 0,10 s),
  respondendo na metade do tempo para pular logos e transições assim que aceitam input.
- Antes/depois no PC: título em ~26 s -> 19,3 s. Console: título em 31,7 s (com logos).

### SHH-04: setter marks fora do lugar (P1)
- Evidência: `Setter marks: device chunk +12544 (0x3100) changed with no mark; the marks are off`.
  `kMarkSnapshotEnd = 13088` é o tamanho do SA2; o do SHH é 12712.
- Estado: pendente.

### SHH-06: áudio fora de sincronia no início (P1, relato do usuário 2026-10-29)
- Observado: o áudio não acompanha a imagem no começo do jogo (NRO `80c7a92f`, com `shh_fast_boot`).
- Diagnóstico detalhado:
  - Descartado fast boot (ocorre com ele desligado) e drift de clock (taxas idênticas e zero underruns).
  - Causa real: **latência de buffer e fila de áudio**. Em regime estacionário, a fila de frames mantinha
    ~8-11 quadros (cushion de 10 quadros = 53,3 ms) mais 2-3 buffers de audout de 4 quadros (21,3 ms cada =
    42,7-64 ms no hardware do Switch). O som atrasava a imagem em mais de 100 ms.
- Correção implementada:
  - `audio_switch_frames_in_queue` ajustado para 6 no Switch (~32 ms).
  - Adicionados cvars de runtime `audio_switch_buffer_frames` (ajustado para 2 no Switch = 10,7 ms por buffer)
    e `audio_switch_buffers` (3 buffers).
  - Latência total calculada cai de ~100-110 ms para ~53 ms (redução de ~50%).
  - Instrumentação de telemetria nos contadores 28 e 29 (quadros na fila amostrados na mixagem e total de amostras),
    reportando a latência real estimada diretamente na linha `audio:` do `rex_profile.log`.
- Estado: implementado, aguardando validação do usuário no console.

### SHH-05: imagem escura (opção de brilho)
- Evidência: curva de gama e formato da tela iguais no PC e no console; o Xenos de referência é ainda mais escuro.
- Decisão: é o visual do jogo; o brilho do menu de opções funciona.
- Reaberto a pedido do usuário ("ainda continua muito escuro"): `shh_output_gamma` (padrão 1.0 = o jogo), um
  expoente na saída depois da rampa do jogo; 0.8 clareia os tons médios sem tirar o preto. PC: `out/gamma_cmp.png`.
- Console (OLED, 0.8): o print do botão de captura sai claro; na tela o OLED achata os tons perto do preto. Ajuste
  pelo `shh.toml` (0.7 ou 0.6) e pelo modo de cor da tela do console (Vívido/Padrão).
- Decisão: 0.6 ficou certo no console (relato do usuário); é o padrão do Switch (`shh_app.h`), o PC fica em 1.0.

### SHH-07: custo de GPU por passe (P2, 2026-10-29)

Perfil de GPU no console (614 MHz, portátil, 720p, abertura no hospital; `renut_gpu_profile = 10`,
`banjo_renut_diag_gpu_shaders`), média dos 11 blocos de 10 s acima de 25 ms: 38 ms por frame (pico 49.5).
O custo é espalhado; nenhum shader passa de 10 %:

| ms/frame | o quê |
| ---: | --- |
| 5.1 | draws só de profundidade (sombras 1024x1024 e pré-passe 1280x720) |
| 3.6 | PS 54D5AC3E5361D2B8 (50-90 draws) |
| 3.6 | PS F3504E9285918EA9 |
| 3.0 | PS 5A724570D60CF94D |
| 2.7 | resolves de profundidade |
| 2.4 | PS 3BA9CF536FF352A8 (9 texturas, 5 amostras de sombra; 8.8 ms no pico) |
| 1.8 | resolves de cor |

Trace de um frame no PC (`banjo_renut_diag_resolve_trace`, com `shh_diag_press_a_s = 160`, que aperta A até a
abertura): a cena principal é **MSAA 2x** (cor e profundidade 1280x720, 2 samples; é por isso que o 360 usa 2 tiles).
O resolve de profundidade da cena é multisample (não entra no hand-over) e o de cor são dois resolves multisample.

Mudança: `shh_renut_single_sample` (padrão `true` no Switch, `false` no PC) deixa os alvos 2x do jogo com 1 sample no
host. No PC (fbbb0562 é o NRO): a imagem certa, o resolve de profundidade da cena vira hand-over. Falta medir no console.

Medido no console (fbbb0562, 614 MHz): os alvos caíram de 45 para 26 MB (o 1 sample pegou), mas a GPU nos trechos
pesados ficou em 34-46 ms (média 41.1 em 6 blocos, contra 38.0 antes, trecho diferente e com mais draws). Por
categoria: só profundidade 5.1 -> 4.6, PS 54D5AC3E 3.6 -> 2.6, resolves de cor 1.8 -> 1.4, resolves de profundidade
2.7 -> 2.5. Ganho pequeno: o custo é o sombreamento dos materiais e a geometria, não o MSAA.

Os 2.5 ms de resolve de profundidade são os 3 mapas de sombra 1024x1024 copiados para um atlas 2048x2048 (mais o
da cena, 1 por frame), já sem as words: é banda de memória (R32F lido e escrito).

### SHH-08: sombras em 2/3 (opção `shh_shadow_quality`, 2026-10-29)

Os mapas de sombra do SHH são alvos de profundidade 1024x1024 no formato D24FS8 (o `IsShadowMap` do N&B só pegava
D24S8), resolvidos num atlas 2048x2048. `shh_shadow_quality = "Medium"` desenha os mapas em 2/3 (683x683 cada), e o atlas
fica do mesmo tamanho; os shaders leem com UV normalizado. No PC: atlas e resolves de 60 para 42 MB, sombras no lugar,
só um pouco mais suaves. Padrão `Medium` no Switch e `High` no PC; vai para o menu depois. NRO ced4a306, falta medir.

Desenhar as sombras direto no atlas (sem o resolve, sem perda visual) esbarra em dois pontos: o alvo é D24 e o atlas é
float, e os 3 quadrantes vêm de 3 imagens, que o hand-over (uma imagem inteira) não cobre. Fica para depois da medição.

Medido no console (ced4a306, 614 MHz, memória 1331 MHz): resolves de profundidade 2.5 -> 1.7 ms, só profundidade
4.6 -> 4.1 ms. Por draw, nos blocos pesados: ~150 us (fbbb0562 e antes) -> ~140 us. Os trechos pesados ficam em 33-44 ms
(22-29 fps). Somando single sample e sombras em 2/3: uns 7-10 % de GPU. Falta ~25 % para 30 fps estáveis em 614 MHz.

### SHH-09: registradores dos shaders de material (2026-10-29)

`tools/deko3d_gpr_report.py` compila os pixel shaders com o uam do PC (o mesmo `CompileBest` do console) e lê do DKSH
os GPRs, o tamanho do código e o scratch. Os caros do perfil usam 59-80 GPRs:

| shader | GPRs | scratch com limite 48 | com 56 |
| --- | ---: | ---: | ---: |
| 5A724570 | 62 | 2048 | 1024 |
| F3504E92 | 61 | (falha) | 1024 |
| 54D5AC3E | 59 | 2048 | 512 |
| 3BA9CF53 | 69 | 4096 | 1536 |
| 3F28C63E | 62 | 3584 | 1024 |
| 4BBD50CD | 80 | 8192 | 4096 |
| 4D48F590 | 63 | 3072 | 1024 |

Na Maxwell (64 K registradores por SM, blocos de 8 por thread): 64 GPRs = 32 warps (metade), 56 = 36, 48 = 42,
40 = 51, 32 = 64. Com 20+ texturas por pixel, a ocupação é o que esconde a latência. Mudança: o passe gpr32 (do banjo)
passa a aceitar o limite pelo valor da constante 910 (`shh_deko3d_gpr_cap`, 32-64, padrão 48); o uam tenta sem limite
quando a compilação com ele falha. A lista `tools/gpr32_shaders.txt` sai do relatório (os shaders acima do limite).
Medida no console com o A/B da partida (`banjo_renut_ab_target = 16`).

Medido no console (afcaec15, A/B na partida, limite 48 em 432 shaders): **pior**. Nos trechos pesados, ligado 37-66 ms
de GPU contra 29-50 desligado; num trecho estável a 30 fps, 19.2 contra 16.9 ms. O spill (2-8 KB de scratch por warp)
custa mais que a ocupação ganha: a GPU está limitada por banda de memória, não por latência. Revertido: o pacote volta
a 36643b4e (o cache de DKSH no cartão é desse texto), a lista foi para `out/diag/gpr/`; a opção `shh_deko3d_gpr_cap` e o
fallback do uam ficam (desligados sem a lista). NRO sem o limite: ver abaixo.

Memória a 1600 MHz não muda o fps (relato do usuário, log 13): não é banda de DRAM. O custo é de núcleo (ALU e
texturas). Divisão pela medição de 480p contra 720p (14 ms com 0.44x os pixels): ~25 ms por pixel e ~14 ms de
geometria e custo fixo por frame nos trechos pesados.

Geometria: os vertex shaders dos materiais com pele/esqueleto (020396639544EC70, 7B81CD62FB28B9A0, os de sombra
08AE7B6C8FF8C202 e D4BB4AA79623C293) têm ~90 KB de código e 9400 linhas: cada um dos 12 elementos do vértice é lido da
memória (vfetchBound, `g_DkAddressWindow`) com troca de endian e um `switch` pelo formato, ambos uniformes no draw mas
lidos em tempo de execução (`g_UboShared.v[76].w`, a palavra de endian da constante de fetch). Barreiras: ~40 por frame
com ~300 draws (2-4 ms no máximo).

Candidatos sem perda: (1) especializar formato e endian dos fetches por draw (constantes de especialização: tira o
switch e as seleções); (2) limpeza de ALU nos pixel shaders traduzidos. Com perda, para o menu: resolução entre 480p e
720p (480p tirou ~35 % da GPU).

### SHH-10: menu do jogo (2026-10-29, pedido do usuário)

F5 no PC, **L+R+Plus** no Switch (o Plus não chega ao jogo com L+R apertados). Um diálogo do SDK
(`sdk/src/ui/overlay/quick_settings.cpp`: o ImGui do SDK é o que o drawer desenha; no PC o executável tem a cópia do
Renut) com as opções que o app lista (`shh_app.h` MenuSettings): resolução `shh_resolution` 480p / 600p / 720p /
1080p (1080p só no modo dock; no portátil desenha 720p), sombras, gama da tela e MSAA (ao reiniciar). Aplica na hora;
"Salvar" grava o `shh.toml`. 600p é a escala 5/6 (1067x600; o código 6 da RenderScale era 6x, que só o AO cheio em 4K
usava). O app não criava o drawer do overlay (`OnCreateImmediateDrawer`): nenhum menu do SDK existia antes, nem o
L+R+Direita. PC: menu e 600p conferidos em captura. NRO 39c2b39e.

## SHH-11: listras nos tons escuros (2026-10-29)

Causa: o jogo desenha a cena num alvo EDRAM de formato 12 (2_10_10_10_FLOAT_AS_16_16_16_16, 7e3/HDR). O
`ColorTargetFormat` não conhecia os formatos 10 e 12 e caía no padrão R8G8B8A8_UNORM: a cena HDR em 8 bits inteiros,
banding nos escuros (gama e sombras não mudavam nada). Agora 12 → R16G16B16A16_FLOAT (como o 3) e 10 →
R16G16B16A16_UNORM (como o 2). Custo: o dobro de bytes por pixel no alvo principal; medir no console.
- Correção 2 (a de verdade): o Renut nunca aplicava o polygon offset do jogo (D3DRS_DEPTHBIAS/SLOPESCALEDEPTHBIAS,
  device +0x2A50..+0x2A5C, PA_SU_SC_MODE_CNTL bits 11-12). Os mapas de sombra eram desenhados sem bias e a superfície
  se sombreava (acne em anéis; Alta só ameniza). Agora vai na chave do pipeline (`depth_bias_constant` em 2^-24,
  `depth_bias_slope`); cvar `shh_renut_depth_bias` (padrão ligado). No PC a mesa da sala de cirurgia ficou limpa.

## SHH-12: vídeo da abertura mudo no Switch (2026-10-29)
- `New_GURNEY_RIDE_FINAL.bik` tem 6 faixas de áudio no próprio Bink. No PC sai som (dump do SDL: picos 0,27-0,58);
  no Switch o perfil dá 0/472 buffers com som no trecho do vídeo.
- Suspeita: o decodificador de áudio do Bink em ARM gera NaN, que a saída do Switch tocava como silêncio sem avisar.
  Contador 30 ("NaN/infinite samples") na linha `audio:` do rex_profile para confirmar.

## SHH-13 — música dos logos só no título (boot de ~38 s no Switch)

No PC a thread principal abre `start_loop.bik` e a `track02.ogg` 1,6 s depois do áudio iniciar, e a música toca sob os
logos (17 s até o título). No Switch a mesma thread só chega lá 26 s depois do áudio, já no título: o carregamento do
boot (GLOBAL.PAK, SHADERS.PAK, SHELL.PAK) leva ~38 s contra ~2 s no PC. Não é o SD (as leituras somam menos de 1,2 s)
nem CPU (thread principal a 24-37 %): é espera. O vídeo da maca tem som certo (`[audio] 2 s` e `media open` no log).
Diagnóstico: `kernel_wait_profile_s` (xboxkrnl_threading.cpp) registra o tempo de cada espera do kernel por thread e
endereço de chamada (`[waits]` no log, a cada 5 s). No NRO 2d201329 fica ligado nos primeiros 60 s.
Resultado do NRO 2d201329 (shh_003): entre 9 e 34 s a thread principal (4) não fez nenhuma espera do kernel, enquanto
no PC ela já está no laço dos logos (Sleep de ~50 ms) antes de 5 s. Ela fica no carregamento o tempo todo usando ~1/3 de
um núcleo: o resto é espera fora do kernel ou falta de núcleo. O NRO 7aaa26f0 soma ao perfil as esperas por fence D3D
(5), pela fila de frames no Swap (6) e por seções críticas (7).

## SHH-14 — travadas ao entrar em área nova
Nenhum shader é compilado no console (`uam 0 compiles`). Candidatos vistos no shh_002: pipelines criadas na hora
(119 → 189 em 2 min; a lista salva foi zerada pela mudança da PipelineKey do depth bias e ainda está se refazendo),
blocos de 64 MB do heap de imagens criados no meio do jogo (3 em 4 s ao entrar no corredor) e picos de 100-450 ms
entre input e present. Falta medir o que cada frame lento fez.
Causa (shh_001 do NRO 7aaa26f0): a thread principal também não espera fence, fila de frames nem seção crítica no
carregamento. Ela está pronta para rodar e só recebe ~1/3 de um núcleo: toda thread nasce com núcleo preferido -1 (o
núcleo padrão do processo, o mesmo para todas), e a principal, o StdAudio Engine e os ajudantes do carregamento enchem
um núcleo enquanto o console usa 60-84 % de 400 %. Correção: `switch_guest_thread_spread` (xthread.cpp), cada thread do
jogo prefere o próximo núcleo em rodízio (a principal o 1), com os três na máscara. NRO com ela: ver o log `[threads]`.
