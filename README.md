# SekiroInternal

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Game](https://img.shields.io/badge/Sekiro-1.06-8B1E3F)](https://github.com/Luckisugar/SekiroInternal)
[![Languages](https://img.shields.io/badge/menu-EN%20%7C%20PT--BR-2ea44f)](https://github.com/Luckisugar/SekiroInternal)

In-game menu for **Sekiro: Shadows Die Twice**, Steam build **1.06**. **Insert** opens and closes it. The menu is English and Portuguese.

Built for a normal Steam install. It reads Wolf only after you are in game. The title screen is expected to show stats as unreadable.

## Contents

- [What it does](#what-it-does)
- [Build](#build)
- [Run](#run)
- [Notes](#notes)
- [License](#license)
- [Português](#português)

## What it does

- Infinite vitality, undeath (never below 1), infinite spirit emblems
- Lifesteal for a landed hit, a held block, a parry, and deathblows (regular, mini-boss, boss)
- Gourd restore from parries, with a charge cap
- Damage, vitality, and posture scale sliders (0.1x to 100x). Vitality size reduces damage taken. It does not freeze the bar
- Catch lightning in the air, with its own key
- Live vitality, posture, emblems, and gourd while you are in game

Easier parry and 100% parry are not in this build. The public no-damage flag makes enemies immune too, so it is left out on purpose.

The FPS unlock and FSR upscale ship in the same zip. That half is [SekiroPerf](https://github.com/Luckisugar/SekiroPerf), a fork of LordDav0s/SekiroPerf. The menu DLL does not patch the game's swap chain. SekiroPerf draws the menu after the upscale.

## Drop in

[Latest release](https://github.com/Luckisugar/SekiroInternal/releases/latest). Unzip it into the folder that contains `sekiro.exe` (patch **1.06**). Start the game. **Insert** opens the menu.

`dinput8.dll` in that zip is the loader. Leave it when the folder has no Mod Engine. If `modengine.ini` is already there, do not replace `dinput8.dll`. Point `chainDInput8DLLPath` at `\sekiro_perf.dll` instead. If `WeaponWheel.dll` is in the folder, set `ChainDLL="\WeaponWheel.dll"` in `sekiro_perf.ini`.

Set the in-game resolution below your monitor (1280x720 on a 1080p screen). That is what the upscale speeds up. `sekiro_perf.log` should mention the wrap and `Also load SekiroInternal.dll: ok`.

Resurrection, Weapon Wheel, and Precise Deflect Flash are other mods. They are not in this zip.

`settings.ini`, `ui-language.txt`, and `activity.log` are created beside the DLL.

## Build

Windows, with **Visual Studio 2022 Build Tools** (the C++ x64 toolset).

```powershell
.\build.ps1
```

That writes `build\SekiroInternal.dll` and `build\Inject.exe`, then copies both next to the script.

## Run from a build

Use the release zip above when you just want to play. `Inject.exe` is only for a build that is not loaded by SekiroPerf. Do not run it while `sekiro_perf.dll` is already loading this DLL.

1. Start Sekiro and load into the game (1.06).
2. Run `Inject.exe` from the folder that contains `SekiroInternal.dll`.
3. Press **Insert**.

If Sekiro is running as administrator, run the injector the same way. It does not elevate itself.

## Notes

- Only `sekiro.exe` **1.06** is recognized. Other builds are left alone.
- The injector stops if `BloodOfTheDragon.dll` or a `winmm.dll` is sitting in the game folder.
- Saved menu values stay in `settings.ini` next to the DLL.

## License

Project code is [MIT](LICENSE). Dear ImGui (MIT) and Hacker Disassembler Engine 64 (Vyacheslav Patkov, all rights reserved) live under `third_party/` and keep their own notices.

## Star history

[![Star History Chart](https://api.star-history.com/svg?repos=Luckisugar/SekiroInternal&type=Date)](https://star-history.com/#Luckisugar/SekiroInternal&Date)

## Português

Menu dentro do jogo para **Sekiro: Shadows Die Twice**, build Steam **1.06**. **Insert** abre e fecha. O menu sai em inglês e português.

Feito para a instalação normal da Steam. O Wolf só é lido depois que você entra no jogo. Na tela de título os status ficam ilegíveis, e isso é esperado.

### O que faz

- Vitalidade infinita, imortal (nunca abaixo de 1), emblemas espirituais infinitos
- Roubo de vida por golpe acertado, bloqueio segurado, aparo e deathblow (comum, mini-chefe, chefe)
- Restaura o gourd com aparos, com teto de cargas
- Sliders de dano, vitalidade e postura (0.1x a 100x). O tamanho da vitalidade reduz o dano recebido. Não trava a barra
- Pegar raio no ar, com a própria tecla
- Vitalidade, postura, emblemas e gourd ao vivo quando você está no jogo

Aparo fácil e aparo 100% não estão nesta versão. A flag pública de sem dano deixa os inimigos imunes também, então ficou de fora de propósito.

O desbloqueio de FPS e o upscale FSR vão no mesmo zip. Essa metade é o [SekiroPerf](https://github.com/Luckisugar/SekiroPerf), fork do LordDav0s/SekiroPerf. A DLL do menu não mexe na swap chain. O SekiroPerf desenha o menu depois do upscale.

### Soltar na pasta

[Release mais nova](https://github.com/Luckisugar/SekiroInternal/releases/latest). Descompacta na pasta do `sekiro.exe` (patch **1.06**). Abre o jogo. **Insert** abre o menu.

O `dinput8.dll` desse zip é o carregador. Deixa ele se a pasta não tem Mod Engine. Se já existe `modengine.ini`, não troca o `dinput8.dll`. Aponta `chainDInput8DLLPath` para `\sekiro_perf.dll`. Se `WeaponWheel.dll` está na pasta, põe `ChainDLL="\WeaponWheel.dll"` no `sekiro_perf.ini`.

A resolução dentro do jogo tem que ficar abaixo do monitor (1280x720 numa tela 1080p). É isso que o upscale acelera. O `sekiro_perf.log` deve citar o wrap e `Also load SekiroInternal.dll: ok`.

Resurrection, Weapon Wheel e Precise Deflect Flash são outros mods. Não estão neste zip.

### Compilar

Windows, com **Visual Studio 2022 Build Tools** (toolset C++ x64).

```powershell
.\build.ps1
```

Gera `build\SekiroInternal.dll` e `build\Inject.exe`, e copia os dois para a pasta do script.

### Rodar a partir de um build

Usa o zip da release quando você só quer jogar. `Inject.exe` é só para um build que o SekiroPerf não está carregando. Não rode ele com `sekiro_perf.dll` já carregando esta DLL.

1. Abra Sekiro e entre no jogo (1.06).
2. Rode `Inject.exe` na mesma pasta da `SekiroInternal.dll`.
3. Aperte **Insert**.

Se Sekiro estiver como administrador, rode o injetor do mesmo jeito. Ele não pede elevação sozinho.

`settings.ini`, `ui-language.txt` e `activity.log` nascem ao lado da DLL.

### Avisos

- Só reconhece `sekiro.exe` **1.06**. Outras builds ficam intocadas.
- O injetor para se achar `BloodOfTheDragon.dll` ou `winmm.dll` na pasta do jogo.
- O menu salvo fica em `settings.ini` ao lado da DLL.
