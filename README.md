# SaltyNX

Sysmodule for injecting custom codes into retail games, by default it injects "Core" which incldues many useful features known as NX-FPS (tracking game performance statistics, unlocking FPS) and ReverseNX-RT (switching between docked and handheld graphics profiles). Sysmodule also manages custom refresh rates in both handheld and docked modes.

Original sysmodule was created by: https://github.com/shinyquagsire23

This fork includes many new features (aforementioned NX-FPS and ReverseNX-RT), compatibility fixes, and beside plugins support also supports patches for easy way to edit common functions.

![GitHub all releases](https://img.shields.io/github/downloads/masagrator/SaltyNX/total?style=for-the-badge)
---

> [!CAUTION]
> It is required to have FW installed at least 10.0.0 version
> 
> No technical support for anything else than stock Atmosphere.

> [!IMPORTANT]
> Known issues:
> - Instability with some sysmodules (like emuiibo),
> - Cheats using HEAP+offset address instead of MAIN+offset address chain may not work properly.

Tools utilizing SaltyNX:
- [FPSLocker](https://github.com/masagrator/FPSLocker)
- [ReverseNX-RT](https://github.com/masagrator/ReverseNX-RT)
- [Status Monitor Overlay](https://github.com/masagrator/Status-Monitor-Overlay)
- And more

For additional functions you need [SaltyNX-Tool](https://github.com/masagrator/SaltyNX-Tool)

Patches pattern:
- filename is symbol of function with filetype `.asm64` for 64-bit games, `.asm32` for 32-bit games,
- inside file write with hex editor instructions that you want to put into this function,
- put this file either to `SaltySD/patches` to make it work for every game, or to `SaltySD/patches/*titleid*` to make it work for specific game.

# How to install release:

Download newest version from [RELEASES](https://github.com/masagrator/SaltyNX/releases), look for .zip file with name starting "SaltyNX", unpack it.

Put `atmosphere` and `SaltySD` folders unpacked from zip archive to root of your sdcard.

Remember to restart Switch

---

# Exceptions

<details> 

  <summary>List of titles not compatible only with external plugins, they support all other SaltyNX features</summary>

| Title | Why? |
| ------------- | ------------- |
| Alien: Isolation | Heap related |
| Azure Striker Gunvolt: Striker Pack | 32-bit games don't support plugins |
| Baldur's Gate and Baldur's Gate II: Enhanced Editions | 32-bit games don't support plugins |
| CelDamage HD | 32-bit games don't support plugins |
| Company of Heroes Collection | heap related |
| DEADLY PREMONITION Origins | 32-bit games don't support plugins |
| Death Road to Canada | 32-bit games don't support plugins |
| Dies irae Amantes amentes For Nintendo Switch | 32-bit games don't support plugins |
| EA SPORTS FC 24 | heap related |
| Goat Simulator | 32-bit games don't support plugins |
| Gothic | 32-bit games don't support plugins |
| Grandia Collection | Only launcher is 64-bit, actual games are 32-bit, so plugins are not supported |
| Grid: Autosport | Heap related |
| Immortals Fenyx Rising | Heap related |
| LIMBO | 32-bit games don't support plugins |
| Luigi's Mansions 2 HD | 32-bit games don't support plugins |
| Luigi's Mansion 3 | Heap related |
| Mario Kart 8 Deluxe (1.0.0-3.0.3) | 32-bit games don't support plugins |
| Mario Strikers: Battle League | Heap related |
| Megadimension Neptunia VII | 32-bit games don't support plugins |
| Moero Chronicle Hyper | 32-bit games don't support plugins |
| Moero Crystal H | 32-bit games don't support plugins |
| Monster Hunter Generations Ultimate | 32-bit games don't support plugins |
| Monster Hunter XX Nintendo Switch Ver. | 32-bit games don't support plugins |
| New Super Mario Bros. U Deluxe | 32-bit games don't support plugins |
| Ni no Kuni: Wrath of the White Witch | 32-bit games don't support plugins |
| Olympic Games Tokyo 2020 – The Official Video Game™ | heap related |
| Pikmin 3 Deluxe | 32-bit games don't support plugins |
| Planescape: Torment and Icewind Dale | 32-bit games don't support plugins |
| Plants vs. Zombies: Battle for Neighborville | Heap related |
| Radiant Silvergun | 32-bit games don't support plugins |
| Sherlock Holmes and The Hound of The Baskervilles | 32-bit games don't support plugins |
| Stubbs the Zombie in Rebel Without a Pulse | heap related |
| The Lara Croft Collection | heap related |
| Tokyo Mirage Session #FE Encore | 32-bit games don't support plugins |
| Valkyria Chronicles | 32-bit games don't support plugins |
| World of Goo | 32-bit games don't support plugins |
| YouTube | Unknown |
| 超次次元ゲイム ネプテューヌRe;Birth1 | 32-bit games don't support plugins |
| 超次次元ゲイム ネプテューヌRe;Birth2 SISTERS GENERATION | 32-bit games don't support plugins |
| 神次次元ゲイム ネプテューヌRe;Birth3 V CENTURY | 32-bit games don't support plugins |

</details>

<details>
  <summary>List of games not compatible with SaltyNX at all</summary>

| Title | Why? |
| ------------- | ------------- |
| Witcher 3 GOTY (version 3.2) | heap related |
</details>
Titles in exceptions.txt are treated as part of Blacklist, you can find it in root of repo. SaltyNX reads it from SaltySD folder. `X` at the beginning of titleid means that this game will not load any patches and plugins. `R` at the beginning of titleid means that this game will not load any patches and plugins if romfs mod for this game is installed. 32-bit games are ignored by default for plugins.

---

# Thanks to
- `Cooler3D` for sharing code with me how he was changing handheld display refresh rate in his tools that were first publicly available tools allowing this on HOS. I have used that as basis to make my own function.
