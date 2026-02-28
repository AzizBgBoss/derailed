![Derailed Logo](media/banner.png)<br>
[![Downloads](https://img.shields.io/github/downloads/AzizBgBoss/derailed/total.svg?label=Download)](https://github.com/AzizBgBoss/derailed/releases)
# Derailed!
A game inspired by Unrailed! for the DS by AzizBgBoss, about keeping a train running.<br>

I bought the Unrailed! game for my switch some years ago, and got obsessed with the game. And after I learned a lot about programming on the DS when making [TerrariaDS](https://github.com/AzizBgBoss/TerrariaDS), I decided to make a DS version of it, because I love the DS too lol.<br>
<br>
I'm very open to contributions and suggestions, so if you want to help, feel free to open a pull request or an issue.<br>
<br>
You can join my [Discord server](https://discord.gg/zfMwPhvDc4) for more updates and discussions.<br>

### Disclaimer
This project is not affiliated with Unrailed! or Indoor Astronaut. It is a fan-made project and is not intended for commercial use. All of the assets used in this project are made by me apart from the music and title font.

### Notes:
- The local multiplayer mode is unpolished, but I'm working to fix it.
- I'm thinking about porting the game to many other consoles and add an Online Multiplayer mode with cross-play (apart from the GBA, yeah there will be no online multiplayer).
- I finally added a tutorial for the game. It's not very good yet, but I'm working to make it better. I did it since many people had problems learning how to play [TerrariaDS](https://github.com/AzizBgBoss/TerrariaDS).

## Installation:
You can either compile the game yourself (for nightly releases) or check for stable releases in the Releases page. Please check the Tested devices part before starting.

### Tested devices:
| System                           | Functionality             | Cons                  | Notes                                                                                                       |
|----------------------------------|---------------------------|-----------------------|-------------------------------------------------------------------------------------------------------------|
| Nintendo DSi (XL) (TM++)         | Good                      | Laggy multiplayer     | None                                                                 |
| Nintendo 3DS (New) (XL) (TM++)   | Good                      | Laggy multiplayer     | Should work fine as long as you're using TW++ since it basically becomes a DSi at that point.|
| Flashcarts (DS/DS Lite/DSi)      | Not tested yet            | Not tested yet        | There is no saving in the game for now so it should work even without a DLDI patch. |
| melonDS (Windows Emulator)       | Good                      | Laggy multiplayer     | None          |

## Download the latest version (QR code scannable with DSi Downloader)

![QR](media/frame.png)

## Gameplay:
Check tutorial on main menu.
- D-Pad: Move
- A: Pick up item / Put item
- L: Command Bobot to bring the axe.
- R: Command Bobot to bring the pickaxe.
- Y: Command Bobot to bring the nearest wood.
- X: Command Bobot to bring the nearest iron.
- Start: Main menu
- Select: Debug mode

## Changelog and Features
I don't really expect really big changes as the core mechanics of the game are already done, but I'll still try to add some features in the future.

### Version 1.0 ([![Download](https://img.shields.io/github/downloads/AzizBgBoss/derailed/1.0/total.svg?label=Download)](https://github.com/AzizBgBoss/derailed/releases/tag/1.0)):
Initial commit with basic project structure.

- Local multiplayer. ***(done but buggy)***
- Added Bobot, your (optional) assistant robot. ***(done)***

### Version 1.1 (Working on it):
- Reimagine the art.
- Added a pause menu.
- Better looking UI.
- Fix the multiplayer bugs.

## Planned Features for the future:
- Online multiplayer with hostable servers.
- Better graphics.
- More music.

## Compile it yourself
To compile this project, you need to have BlocksDS installed. You can find instructions on how to install it on the [BlocksDS website](https://blocksds.skylyrac.net/).

Once you have BlocksDS installed, you can clone this repository by running:
```bash
git clone https://github.com/AzizBgBoss/derailed.git
```

Then, navigate to the project directory:
```bash
cd derailed
```

For good measure, clean the place first:
```bash
make clean
```

Now, you can compile the project by running:
```bash
make
```
This will create a `derailed.nds` file in the root directory of the project.

## Credits:
- AzizBgBoss for making the game.
- AntonioND for making the BlocksDS toolchain.
- Anadune & Floppy for the music.
- Many kind people that made free 3D assets I used for the graphics.

## Donate:
This game is totally free and if you enjoy this project and would like to support its development, any donation, no matter how small, would be greatly appreciated. I'm a teenager and I really don't have much time with school and other stuff, so your generosity helps motivate me and dedicate more time to continue working this and other games and bring new features to life. Thank you for considering a contribution!
- USDT (TRX20): TVq9gPQP2xTybbA2QtWrXBuQgvxZSVdiXm
- USDT (ETH): 0x63FCB723F9EaA3100b466f229067227EfA461Bf7

## Gameplay video:
*in progress*