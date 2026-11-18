# Sekiro Mods

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Game](https://img.shields.io/badge/Sekiro-1.06-8B1E3F)](https://github.com/Luckisugar/Sekiro-Mods)
[![Languages](https://img.shields.io/badge/menu-EN%20%7C%20PT--BR-2ea44f)](https://github.com/Luckisugar/Sekiro-Mods)

Drop-in folder for **Sekiro: Shadows Die Twice**, patch **1.06**. This is not the game. Install the game first, then copy this repo on top of the folder that contains `sekiro.exe`.

**Insert** opens the in-game menu (English and Portuguese). The same folder unlocks the 60 FPS cap, upscales with FSR, gives the weapon wheel 20 slots, almost stops time while that wheel is open, and loads the deflect cue.

## Contents

- [Put it on the game](#put-it-on-the-game)
- [What you get](#what-you-get)
- [Left out](#left-out)
- [Build](#build)
- [License](#license)
- [Português](#português)

## Put it on the game

1. Install Sekiro **1.06**.
2. Download this repo as a ZIP (Code, then Download ZIP). A `git clone` is the same files.
3. Copy everything in the ZIP onto the folder that contains `sekiro.exe`. Let it replace files that have the same name. Leave the game's `Data` archives, movies, and sound where they are.
4. Start the game. In the graphics menu, set the resolution **below** your monitor. That is what the upscale speeds up. **Insert** opens the menu.

Quit the game before you copy. A running Sekiro keeps the old DLLs loaded.

The loader is `dinput8.dll`. It starts `sekiro_perf.dll`. That DLL unlocks the frame rate, upscales, loads `SekiroInternal.dll`, and chain-loads `WeaponWheel.dll`. The wheel then loads `PreciseDeflectFlash.dll`. Those paths are already set in `modengine.ini`, `sekiro_perf.ini`, and `WeaponWheelResources/chainload.txt`.

`sekiro_perf.log` should mention the wrap and `Also load SekiroInternal.dll: ok`.

## What you get

The menu can give infinite vitality, undeath (never below 1), and infinite spirit emblems. It can lifesteal on a landed hit, a held block, a parry, and deathblows (regular, mini-boss, boss). Parries can refill the gourd, with a charge cap. Damage, vitality, and posture have sliders from 0.1x to 100x. Vitality size reduces damage taken and does not freeze the bar. Catch lightning has its own key. Vitality, posture, emblems, and gourd show live once you are in game. The title screen stats stay unreadable until then. Your saved sliders are in `settings.ini`. The menu language is `ui-language.txt` (`en` right now).

Easier parry and 100% parry are not in this build. The public no-damage flag makes enemies immune too, so it is left out on purpose.

Frame rate is uncapped and then capped at **144**, with the movement-speed fix so running stays correct. FSR 1.0 sharpness is **0.8**. VSync stays on. Upscaling runs borderless at the desktop refresh. On a 1080p monitor, 1280x720 is the usual in-game resolution. `dxgi.dll`, `d3d11.dll`, and `d3d9.dll` are DXVK, with async compile turned on in `dxvk.conf`.

The weapon wheel has **20** slots. `wheelItems.txt` is the loadout. "Slow Down Time On Wheel Open" is on, and the slow is almost a stop: each frame's time step is written as **0.0001** (about 0.6% speed at 60 FPS). The old value was 0.0047. Number keys 1 through 0 still do nothing useful. The wheel's own equip keys were already dead code.

The deflect cue is on. **F7** toggles it, **F8** reloads `PreciseDeflectFlash.ini`. Insert, F7, and F8 stay reserved.

## Left out

Sekiro Resurrection, the costume pack, and the extra sound pack are not in this repo. Together they are about 2.2 GB, and some files are over GitHub's 100 MB file limit. They were deployed with Vortex. After a new install, enable those again in Vortex. This drop-in does not need Vortex.

The game itself is not here. No executable, no archives, no movies, no soundtrack.

## Build

You do not need to build to play. The DLLs at the root are the ones to copy.

The menu source is `source/SekiroInternal`. Windows, Visual Studio 2022 Build Tools, C++ x64:

```powershell
cd source\SekiroInternal
.\build.ps1
```

That writes `SekiroInternal.dll` and `Inject.exe` next to the script. Copy `SekiroInternal.dll` onto the game folder. `Inject.exe` is only for a build that SekiroPerf is not already loading. Do not run it while `sekiro_perf.dll` is loading the menu. If Sekiro is running as administrator, run the injector the same way.

The FPS and FSR source is `source/SekiroPerf` (CMake, Visual Studio 2022). Copy the built `sekiro_perf.dll` onto the game folder. Only `sekiro.exe` **1.06** is recognized. Other builds are left alone.

## License

Code under `source/SekiroInternal` and `source/SekiroPerf` is [MIT](LICENSE). Dear ImGui (MIT) and Hacker Disassembler Engine 64 keep their own notices under `source/SekiroInternal/third_party`.

DXVK, Mod Engine, the weapon wheel, and the deflect cue are third-party binaries included so a reinstall can be copied back into place. They are not covered by the MIT license.

## Star history

[![Star History Chart](https://api.star-history.com/svg?repos=Luckisugar/Sekiro-Mods&type=Date)](https://star-history.com/#Luckisugar/Sekiro-Mods&Date)

## Português

Pasta pronta para **Sekiro: Shadows Die Twice**, patch **1.06**. Isto não é o jogo. Instala o jogo primeiro, depois copia este repositório em cima da pasta do `sekiro.exe`.

**Insert** abre o menu dentro do jogo (inglês e português). A mesma pasta tira o limite de 60 FPS, faz upscale com FSR, deixa a roda de armas com 20 espaços, quase para o tempo com a roda aberta, e carrega a dica de aparo.

### Colocar no jogo

1. Instala Sekiro **1.06**.
2. Baixa o ZIP deste repositório (Code, depois Download ZIP). Um `git clone` traz os mesmos arquivos.
3. Copia o conteúdo do ZIP para a pasta do `sekiro.exe`. Pode substituir arquivo com o mesmo nome. Não apaga os `Data`, os filmes nem o som do jogo.
4. Abre o jogo. Na resolução, deixa **abaixo** do monitor. É isso que o upscale acelera. **Insert** abre o menu.

Fecha o jogo antes de copiar. O Sekiro aberto continua com as DLLs velhas.

O `dinput8.dll` é o carregador. Ele sobe o `sekiro_perf.dll`. Essa DLL solta o FPS, faz o upscale, carrega `SekiroInternal.dll` e encadeia `WeaponWheel.dll`. A roda carrega `PreciseDeflectFlash.dll`. Os caminhos já estão em `modengine.ini`, `sekiro_perf.ini` e `WeaponWheelResources/chainload.txt`.

O `sekiro_perf.log` deve citar o wrap e `Also load SekiroInternal.dll: ok`.

### O que vem

O menu pode dar vitalidade infinita, imortal (nunca abaixo de 1) e emblemas infinitos. Roubo de vida em golpe, bloqueio segurado, aparo e deathblow (comum, mini-chefe, chefe). Aparos enchem o gourd, com teto de cargas. Dano, vitalidade e postura vão de 0.1x a 100x. O tamanho da vitalidade reduz o dano e não trava a barra. Pegar raio tem a própria tecla. Vitalidade, postura, emblemas e gourd aparecem ao vivo depois que você entra no jogo. Na tela de título isso fica ilegível, e é esperado. Os sliders salvos estão em `settings.ini`. O idioma do menu está em `ui-language.txt` (`en` agora).

Aparo fácil e aparo 100% não estão nesta versão. A flag pública de sem dano deixa os inimigos imunes também, então ficou de fora de propósito.

O FPS sobe e o teto fica em **144**, com a correção de velocidade de corrida. A nitidez do FSR 1.0 está em **0.8**. O VSync continua ligado. O upscale roda em borderless na taxa do desktop. Em monitor 1080p, 1280x720 é a resolução usual dentro do jogo. `dxgi.dll`, `d3d11.dll` e `d3d9.dll` são DXVK, com compilação async ligada no `dxvk.conf`.

A roda de armas tem **20** espaços. O `wheelItems.txt` é o loadout. "Slow Down Time On Wheel Open" está ligado, e a lentidão é quase uma parada: o passo de tempo de cada frame vira **0.0001** (cerca de 0.6% da velocidade a 60 FPS). O valor antigo era 0.0047. As teclas 1 a 0 continuam sem servir. O atalho antigo da roda já era código morto.

A dica de aparo está ligada. **F7** liga e desliga, **F8** recarrega `PreciseDeflectFlash.ini`. Insert, F7 e F8 ficam reservados.

### Ficou de fora

Sekiro Resurrection, o pack de roupa e o pack de som extra não estão aqui. Juntos são uns 2.2 GB, e alguns arquivos passam do limite de 100 MB do GitHub. Eles estavam no Vortex. Numa instalação nova, liga eles de novo no Vortex. Esta pasta não precisa do Vortex.

O jogo em si não está aqui. Sem executável, sem archives, sem filmes, sem trilha.

### Compilar

Não precisa compilar para jogar. As DLLs na raiz são as que você copia.

O código do menu está em `source/SekiroInternal`. Windows, Visual Studio 2022 Build Tools, C++ x64:

```powershell
cd source\SekiroInternal
.\build.ps1
```

Gera `SekiroInternal.dll` e `Inject.exe` ao lado do script. Copia `SekiroInternal.dll` para a pasta do jogo. `Inject.exe` é só para um build que o SekiroPerf não está carregando. Não rode ele com `sekiro_perf.dll` já carregando o menu. Se o Sekiro estiver como administrador, rode o injetor do mesmo jeito.

O código do FPS e do FSR está em `source/SekiroPerf` (CMake, Visual Studio 2022). Copia a `sekiro_perf.dll` compilada para a pasta do jogo. Só `sekiro.exe` **1.06** é reconhecido. Outras builds ficam intocadas.

### Licença

O código em `source/SekiroInternal` e `source/SekiroPerf` é [MIT](LICENSE). Dear ImGui (MIT) e Hacker Disassembler Engine 64 mantêm os avisos deles em `source/SekiroInternal/third_party`.

DXVK, Mod Engine, a roda de armas e a dica de aparo são binários de terceiros, incluídos para a reinstalação voltar no lugar. Não entram na licença MIT.
