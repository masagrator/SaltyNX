# SaltyNX

Background process for the Nintendo Switch for file/code modification

Created by: https://github.com/shinyquagsire23

This fork includes many QoL improvements and beside plugins support also supports patches. 

Since 0.7.0 version NX-FPS and ReverseNX-RT are an intergral part of SaltyNX Core.<br>
Since 1.0.0 32-bit games are supported (external plugins are not supported for them).<br>
To use ReverseNX-RT download newest overlay from [HERE](https://github.com/masagrator/ReverseNX-RT/releases)

![GitHub all releases](https://img.shields.io/github/downloads/masagrator/SaltyNX/total?style=for-the-badge)
---

Patches pattern:
- filename is symbol of function with filetype `.asm64` for 64-bit games, `.asm32` for 32-bit games,
- inside file write with hex editor instructions that you want to put into this function,
- put this file either to `SaltySD/patches` to make it work for every game, or to `SaltySD/patches/*titleid*` to make it work for specific game.

For additional functions you need SaltyNX-Tool

https://github.com/masagrator/SaltyNX-Tool

> [!CAUTION]
> It is required to have FW installed at least 10.0.0 version
> 
> No technical support for anything else than stock Atmosphere.

> [!IMPORTANT]
> Known issues:
> - Instability with some homebrews and sysmodules (like emuiibo),
> - You need to have at least Hekate 5.0.2 if you don't want issues related to Hekate,
> - Cheats using HEAP+offset address instead of MAIN+offset address chain may not work properly.

# How to download release:

For Atmosphere >=0.10.1 just put folders from archive to root of your sdcard.

For Atmosphere <=0.9.4 and any other CFW rename `contents` folder to `titles`

Remember to restart Switch

---

# Thanks to
- `Cooler3D` for sharing code with me how he was changing handheld display refresh rate in his tools that were first publicly available tools allowing this on HOS. I have used that as basis to make my own function.

# List of titles not compatible with plugins/patches

| Title | plugins/all | Why? |
| ------------- | ------------- | ------------- |
| Alien: Isolation | plugins | Heap related |
| Azure Striker Gunvolt: Striker Pack | plugins | 32-bit game, not supported |
| Baldur's Gate and Baldur's Gate II: Enhanced Editions | plugins | 32-bit game, not supported |
| CelDamage HD | plugins | 32-bit game, not supported |
| Company of Heroes Collection | plugins | heap related |
| DEADLY PREMONITION Origins | plugins | 32-bit game, not supported |
| Death Road to Canada | plugins | 32-bit game, not supported |
| Dies irae Amantes amentes For Nintendo Switch | plugins | 32-bit game, not supported |
| EA SPORTS FC 24 | plugins | heap related |
| Goat Simulator | plugins | 32-bit game, not supported |
| Gothic | plugins | 32-bit game, not supported |
| Grandia Collection | plugins | Only launcher is 64-bit, actual games are 32-bit, so not supported |
| Grid: Autosport | plugins | Heap related |
| Immortals Fenyx Rising | plugins | Heap related |
| LIMBO | plugins | 32-bit game, not supported |
| Luigi's Mansions 2 HD | plugins | 32-bit game, not supported |
| Luigi's Mansion 3 | plugins | Heap related |
| Mario Kart 8 Deluxe (1.0.0-3.0.3)| plugins | 32-bit game, not supported |
| Mario Strikers: Battle League | plugins | Heap related |
| Megadimension Neptunia VII | plugins | 32-bit game, not supported |
| Moero Chronicle Hyper | plugins | 32-bit game, not supported |
| Moero Crystal H | plugins | 32-bit game, not supported |
| Monster Hunter Generations Ultimate | plugins | 32-bit game, not supported |
| Monster Hunter XX Nintendo Switch Ver. | plugins | 32-bit game, not supported |
| New Super Mario Bros. U Deluxe | plugins | 32-bit game, not supported |
| Ni no Kuni: Wrath of the White Witch | plugins | 32-bit game, not supported |
| Olympic Games Tokyo 2020 – The Official Video Game™ | plugins | heap related, exefs patch included to get it to work at all |
| Pikmin 3 Deluxe | plugins | 32-bit game, not supported |
| Planescape: Torment and Icewind Dale | plugins | 32-bit game, not supported |
| Plants vs. Zombies: Battle for Neighborville | plugins | Heap related |
| Radiant Silvergun | plugins | 32-bit game, not supported |
| Sherlock Holmes and The Hound of The Baskervilles | plugins | 32-bit game, not supported |
| Splatoon 3 | all | Crashes after online match |
| Stubbs the Zombie in Rebel Without a Pulse | plugins | heap related, exefs patch included to get it to work at all |
| The Lara Croft Collection | plugins | heap related |
| Tokyo Mirage Session #FE Encore | plugins | 32-bit game, not supported |
| Valkyria Chronicles | plugins | 32-bit game, not supported |
| Witcher 3 GOTY (version 3.2) | all | heap related |
| World of Goo | plugins | 32-bit game, not supported |
| YouTube | plugins | Unknown |
| 超次次元ゲイム ネプテューヌRe;Birth1 | plugins | 32-bit game, not supported |
| 超次次元ゲイム ネプテューヌRe;Birth2 SISTERS GENERATION | plugins | 32-bit game, not supported |
| 神次次元ゲイム ネプテューヌRe;Birth3 V CENTURY | plugins | 32-bit game, not supported |

Titles in exceptions.txt are treated as part of Blacklist, you can find it in root of repo. SaltyNX reads it from SaltySD folder. `X` at the beginning of titleid means that this game will not load any patches and plugins. `R` at the beginning of titleid means that this game will not load any patches and plugins if romfs mod for this game is installed.

32-bit games are ignored by default for plugins.
