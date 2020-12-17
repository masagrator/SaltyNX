# SaltyNX
Background process for the Nintendo Switch for file/code modification

Created by: https://github.com/shinyquagsire23

This fork includes many QoL improvements and beside plugins support also supports patches.

---

Patches pattern:
- filename is symbol of function with filetype `.asm64`,
- inside file write with hex editor instructions that you want to overwrite for this function,
- put this file either to `SaltySD/patches` to make it work for every game, or to `SaltySD/patches/*titleid*` to make it work for specific game.

For additional functions you need SaltyNX-Tool

https://github.com/masagrator/SaltyNX-Tool

Tests were done on FW 7.0.1-10.2.0, Atmosphere 0.9.1-0.9.4, 0.10.1-0.10.4, 0.11.1-0.13.0, 0.14.4-M.15.0

SX OS older than 2.9 are not working. Tested only on sysNAND 9.0.0, 2.9.2

No technical support for:
- Atmosphere forks
- SX OS
- Kosmos
- ReinX

Known issues:
- Instability with some homebrews and sysmodules,
- You need to have at least Hekate 5.0.2 if you don't want issues related to Hekate,
- 32 bit games are unsupported,
- Cheats using directly heap addresses may not work properly while using plugins.

# How to download release:

For Atmosphere >=0.10.1 just put folders from archive to root of your sdcard.

For Atmosphere <=0.9.4 and any other CFW rename `contents` folder to `titles`

For SX OS remember to rename `atmosphere` folder to `sxos`

Remember to restart Switch

---

List of titles not compatible with plugins/patches:

| Title | plugins/all | Why? |
| ------------- | ------------- | ------------- |
| Alien: Isolation | plugins | Heap related |
| Azure Striker Gunvolt: Striker Pack | all | 32-bit game, not supported |
| DEADLY PREMONITION Origins | all | 32-bit game, not supported |
| Goat Simulator | all | 32-bit game, not supported |
| Grandia Collection | all | Only launcher is 64-bit, games are 32-bit |
| Grid: Autosport | plugins | Heap related |
| Immortals Fenyx Rising | plugins | Heap related |
| LIMBO | all | 32-bit game, not supported |
| Little Busters! Converted Edition | plugins | Heap related |
| Luigi's Mansion 3 | plugins | Heap related |
| Mario Kart 8 | all | 32-bit game, not supported |
| Megadimension Neptunia VII | all | 32-bit game, not supported |
| Moero Chronicle Hyper | all | 32-bit game, not supported |
| Moero Crystal H | all | 32-bit game, not supported |
| Monster Hunter Generations Ultimate | all | 32-bit game, not supported |
| New Super Mario Bros. U Deluxe | all | 32-bit game, not supported |
| Ni no Kuni: Wrath of the White Witch | all | 32-bit game, not supported |
| Planescape: Torment and Icewind Dale | all | 32-bit game, not supported |
| Tokyo Mirage Session #FE Encore | all | 32-bit game, not supported |
| Valkyria Chronicles | all | 32-bit game, not supported |
| YouTube | plugins | Unknown |

Titles other than 32-bit are added to exceptions.txt which is treated as Black list, you can find it in root of repo. SaltyNX reads it from SaltySD folder. `X` at the beginning of titleid means that this game will not load any patches and plugins.

32-bit games are ignored by default for patches and plugins.
