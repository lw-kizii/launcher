# Kiwi Launcher
The Kiwi Launcher is a mod loader for [Swordigo](http://touchfoo.com/swordigo). It is inspired by [SwMini](https://github.com/ItsJustSomeDude/SwMini) framework.

This launcher does not include *any* assets from Swordigo.

> [!NOTE]
> This launcher requires vanilla Swordigo to be installed on your device.

# Information
For installation, you can download the latest versions of the launcher from the [releases](https://github.com/lw-kizii/launcher/blob/releases) tab.

- [Discord Server](https://discord.gg/S2BGUgqGvh)
- [Launcher Data](https://github.com/lw-kizii/_)

# Working
The launcher queries the Swordigo application and extracts the library files: `libswordigo.so` and `libopenal-soft.so` into its internal directory. A virtual context is created to load the package `com.touchfoo.swordigo`'s assets and resources so the game can be launched normally!

`libswordigo.so` is also hooked using GlossHook and several functions are patched/rewritten to provide awesome APIs that extend modding capabilities.

# License and Information
The launcher is under the GPL-3 license, read about it more [here](https://github.com/lw-kizii/launcher/blob/main/LICENSE).

**The launcher embeds code from the following projects:**

## Lua 5.1
> Lua 5.1 - Copyright (C) 1994-2012 Lua.org, PUC-Rio.
>
> MIT License
>
> https://www.lua.org/versions.html#5.1

## GlossHook
> GlossHook - Copyright (C) 2022 XMDS.
>
> MIT License
>
> https://github.com/XMDS/GlossHook