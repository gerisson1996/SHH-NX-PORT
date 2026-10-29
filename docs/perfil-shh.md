# Perfil do port: Silent Hill: Homecoming (shh-nx)

Segue o "Mapa universal de otimização, ports Xbox 360 -> Switch" (v1.0). Uma mudança por vez; cada item tem
evidência antes e depois, e só fica se for medido e repetível. O plano técnico do renderizador está em
`docs/plano-renut.md`.

## Fase 0: a base (2026-10-29)

| Campo | Valor |
|---|---|
| Jogo | Silent Hill: Homecoming, Xbox 360 (arquivos do próprio usuário, `assets/game_root`) |
| Repositório | `C:\dev\shh-nx` (git); último commit `1722db5a`, mudanças da etapa 4 e 5 ainda sem commit |
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
- Evidência: falta perfil da cena pesada. Suspeitos: render thread (setter marks, ver SHH-04), threads do jogo,
  tempo de GPU.
- Estado: pendente.

### SHH-03: boot longo (P2)
- Evidência: título em 24,8 s; só ~6 s são do port (tela preta antes do logo, ~4 s lendo o `SHADERS.PAK`).
- Mudança: `shh_fast_boot` (aperta A nos logos até o primeiro BeginTiling). Risco baixo: para no título,
  desliga com `shh_fast_boot = false`.
- Antes/depois no PC: título em ~26 s -> 19,3 s. Console: falta testar.
- Tentado e revertido: relógio do jogo 8x mais rápido (os logos ficaram mais lentos).

### SHH-04: setter marks fora do lugar (P1)
- Evidência: `Setter marks: device chunk +12544 (0x3100) changed with no mark; the marks are off`.
  `kMarkSnapshotEnd = 13088` é o tamanho do SA2; o do SHH é 12712.
- Estado: pendente.

### SHH-05: imagem escura (fechado, sem mudança)
- Evidência: curva de gama e formato da tela iguais no PC e no console; o Xenos de referência é ainda mais escuro.
- Decisão: é o visual do jogo; o brilho do menu de opções funciona.
