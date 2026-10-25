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
- Lifesteal rules for a landed hit, a held block, a parry, and deathblows (regular, mini-boss, boss)
- Gourd restore from held blocks, with a charge cap
- Live vitality, posture, emblems, and gourd while the watch is attached

Easier parry and 100% parry are not in this build. The public no-damage flag makes enemies immune too, so it is left out on purpose.

## Build

Windows, with **Visual Studio 2022 Build Tools** (the C++ x64 toolset).

```powershell
.\build.ps1
```

That writes `build\SekiroInternal.dll` and `build\Inject.exe`, then copies both next to the script.

## Run

1. Start Sekiro and load into the game (1.06).
2. Run `Inject.exe` from the folder that contains `SekiroInternal.dll`.
3. Press **Insert**.

If Sekiro is running as administrator, run the injector the same way. It does not elevate itself.

`settings.ini`, `ui-language.txt`, and `activity.log` are created beside the DLL. Delete the folder to remove them.

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
- Restaura o gourd com bloqueio segurado, com teto de cargas
- Vitalidade, postura, emblemas e gourd ao vivo quando a vigia está presa

Aparo fácil e aparo 100% não estão nesta versão. A flag pública de sem dano deixa os inimigos imunes também, então ficou de fora de propósito.

### Compilar

Windows, com **Visual Studio 2022 Build Tools** (toolset C++ x64).

```powershell
.\build.ps1
```

Gera `build\SekiroInternal.dll` e `build\Inject.exe`, e copia os dois para a pasta do script.

### Rodar

1. Abra Sekiro e entre no jogo (1.06).
2. Rode `Inject.exe` na mesma pasta da `SekiroInternal.dll`.
3. Aperte **Insert**.

Se Sekiro estiver como administrador, rode o injetor do mesmo jeito. Ele não pede elevação sozinho.

`settings.ini`, `ui-language.txt` e `activity.log` nascem ao lado da DLL. Apagar a pasta remove tudo.

### Avisos

- Só reconhece `sekiro.exe` **1.06**. Outras builds ficam intocadas.
- O injetor para se achar `BloodOfTheDragon.dll` ou `winmm.dll` na pasta do jogo.
- O menu salvo fica em `settings.ini` ao lado da DLL.
