# SILENT HILL HOMECOMING NX

🇧🇷 **Português** · 🇺🇸 [English](README.en.md) · 🇪🇸 [Español](README.es.md)

**Silent Hill: Homecoming** (Xbox 360) no **Nintendo Switch**, como homebrew (`.nro`), por **recompilação estática**.
O código PowerPC do `default.xex` é traduzido para C++ com [ReXGlue](https://github.com/rexglue/rexglue-sdk) e
compilado para ARM64. Não é emulação: o jogo roda nativo, com o Direct3D 9 do Xbox 360 reescrito sobre o deko3d, a
API gráfica nativa do Switch.

| | |
|---|---|
| **Versão** | V1.0 |
| **Estado** | Jogável: intro, menus, gameplay e áudio; 30 fps nos menus e na maior parte do jogo |
| **Download** | `shh.nro` e `SHH-NX-V1.0.zip` em [Releases](https://github.com/gerisson1996/SHH-NX-PORT/releases/latest) |
| **Próximo** | 30 fps também nas áreas mais pesadas em 720p; Android e iOS |

### ☕ Apoie o projeto

**Precisamos da sua ajuda!** As ferramentas que uso para fazer estes ports são **pagas**, e cada doação ajuda a
mantê-las e a continuar o projeto. Quer ver outro jogo portado ou uma melhoria? **Faça seu pedido junto com a sua
doação** (escreva na mensagem do Ko-fi, do PayPal ou do Pix).

Se o port foi útil para você, considere apoiar:

- **Ko-fi:** [ko-fi.com/gerissoncassiano](https://ko-fi.com/gerissoncassiano)
- **PayPal:** [Doar pelo PayPal](https://www.paypal.com/donate/?hosted_button_id=B3YTKT29F5ZLG)
- **Pix:** `pocketneogames@gmail.com`

> [!IMPORTANT]
> Este repositório **não contém os arquivos do jogo** (o `.xex`, os `.PAK`, vídeos e sons) nem o código C++ gerado
> a partir dele. Você precisa da sua própria cópia original do Silent Hill: Homecoming de Xbox 360.

---

## ⚡ 30 fps estável

Em **720p** com o clock original do console, a maior parte do jogo já roda a 30 fps, mas as áreas mais pesadas caem
para 23–27 fps. Para ficar **estável**:

- **Resolução 600p:** no menu do port (**L + R + Plus**), mude a resolução para **600p**. Com ela o jogo fica estável,
  com uma imagem só um pouco mais suave.
- **Ou suba o clock da GPU:** quem quiser manter os 720p com 30 fps estáveis pode subir o clock da GPU (por exemplo
  com o sys-clk). Com a GPU acima do clock original do portátil (460,8 MHz) o jogo fica em 30 fps estáveis.

---

## O que já funciona

- O jogo roda nativo no Switch: vídeos de abertura, menus, gameplay e áudio.
- Renderizador nativo em deko3d, **720p** no portátil (480p, 600p e 720p no menu do port; até 1080p no dock), com os
  shaders do jogo compilados no console e guardados em `deko3d_shaders.bin` (a release já traz esse cache pronto para
  as primeiras áreas).
- **30 fps** nos menus e na maior parte das áreas com o clock original do console (CPU 1020 MHz, GPU 460,8 MHz no
  portátil); estável em 600p ou com a GPU acima do clock original.
- Otimizações de CPU no gerador de código e de GPU nos shaders do jogo (luzes, sombras, vértices e cópias de tela),
  sem mudar a imagem.
- Menu do port (**L + R + Plus**).

## O que falta

- **30 fps estáveis em 720p no clock original** nas áreas mais pesadas.
- Bugs conhecidos:
  - no menu, o cursor às vezes pula duas opções ao segurar para baixo;
  - os ícones de botão aparecem gigantes por um instante ao abrir o menu;
  - o reflexo do espelho desenha a cena de novo;
  - manchas escuras arredondadas em algumas luzes.
- Testar o jogo inteiro do começo ao fim.
- Android e iOS.

---

## Instalar no Switch

```
sdmc:/switch/shh-nx/shh.nro
sdmc:/switch/shh-nx/deko3d_shaders.bin   <- cache de shaders (vem no zip)
sdmc:/switch/shh-nx/game_root/           <- os arquivos do seu disco: default.xex, os .PAK, audio/, movies/...
```

- Precisa de Atmosphère e hbmenu.
- Abra em **modo título**: segure **R** ao abrir um jogo instalado e escolha o Silent Hill Homecoming no hbmenu.
  Pelo álbum não há memória suficiente.
- Áreas que não estão no cache compilam os shaders na primeira visita (alguns segundos); depois ficam salvos.
- Os logs ficam em `sdmc:/switch/shh-nx/logs/`.

---

## Como funciona

| Pasta | O que é |
|---|---|
| `app/` | O port: configuração do gerador de código (`overrides.toml`, `huecos.toml`), o aplicativo (`src/shh_app.h`) e o renderizador Renut (`src/renut_engine`), que troca o Direct3D 9 do Xbox 360 por deko3d. |
| `sdk/` | O SDK de recompilação [ReXGlue](https://github.com/rexglue/rexglue-sdk), com a camada do Switch vinda do [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) (memória, arquivos no SD, threads, áudio). |
| `app/thirdparty/` | plume (abstração gráfica), XenosRecomp (shaders do Xenos para GLSL), uam (compilador de shaders do deko3d) e Dear ImGui. |
| `tools/` | Scripts de build, de análise do `.xex` e do pacote de shaders do deko3d (otimizações e cache). |
| `docs/` | Notas técnicas: [tradução do `.xex`](docs/notas-traducao.md) e [perfil de desempenho](docs/perfil-shh.md). |

---

## Compilar

### Requisitos

- **Windows** com **Visual Studio 2022 Build Tools** ("Desenvolvimento para desktop com C++" e "Ferramentas do C++
  Clang") e **LLVM clang 18+**.
- **devkitPro** com `switch-dev`, libnx e deko3d.
- **Python 3.10+** e **Git**.
- O repositório num caminho **sem acento e sem espaço** (ex.: `C:\dev\shh-nx`).

### Passos

1. Baixar as bibliotecas do SDK e compilar o gerador de código:

   ```bat
   python tools\fetch_thirdparty.py
   tools\build_host.bat
   ```

2. Colocar os arquivos do seu disco em `assets\game_root` (o `default.xex` e os `.PAK`) e traduzir o jogo, o que
   gera `app/generated/default`:

   ```sh
   REXGLUE=sdk/out/win-amd64/rexglue.exe tools/codegen.sh app
   ```

3. Compilar o `.nro` (deko3d):

   ```bat
   tools\build_switch.bat C:\caminho\para\nvk_sdk nonvk
   ```

   O resultado fica em `out\switch-deko\shh.nro`.

---

## Créditos

- **ReXGlue SDK**: Tom Clay, com partes do [Xenia](https://xenia.jp) (BSD-3-Clause)
- **nfsmw-nx**: StevensND, a camada do Switch e a base do projeto
- **Renderizador Renut**: dos ports sa2-nx e banjo-nx
- **XenosRecomp**: hedge-dev (MIT)
- **deko3d e uam**: fincs (devkitPro)
- **Port para Switch**: RECOMP-GUTO (gerisson1996)
- **Bibliotecas**: libnx/devkitPro, plume, glslang, SPIRV-Tools, FFmpeg, Dear ImGui, toml++, fmt, spdlog; veja
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

Licença GPL-3.0 para o código principal; o SDK e as bibliotecas mantêm as suas próprias licenças.

Silent Hill é marca da Konami Digital Entertainment. Este projeto não tem relação com ela.
