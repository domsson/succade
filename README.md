# succade - a Lemonbar manager

Feed your [Lemonbar](https://github.com/LemonBoy/bar) with succade! It repeatedly runs [blocks](https://github.com/domsson/candies), piping their output to Lemonbar. Every block is a script or program that provides text to be displayed on your bar. Configuration is done with a simple [ini file](https://en.wikipedia.org/wiki/INI_file).

![succade lemonbar example 1](https://i.imgur.com/H7vFfV3.png)
![succade lemonbar example 2](https://i.imgur.com/nkoptQR.png)
![succade lemonbar example 3](https://i.imgur.com/baHPzeI.png)
![succade lemonbar example 4](https://i.imgur.com/K5tPuo0.png)

# How it works

- Starts `lemonbar` for you
- Reads the config file (default is `~/.config/succade/succaderc`)
- Loads blocks (programs or scripts defined in the config file)
- Updates Lemonbar based on the blocks' output 

The config needs to have one section for lemonbar (`bar`) and one per block, plus an optional section for common styles (`default`) that will apply to all blocks. The bar's section lists the blocks that should be displayed on the bar (and where), and can set some lemonbar properties like its size and position. Block sections define the styling of individual blocks, as well as how often the blocks should be reloaded. Alternatively, trigger commands can be defined. Once a trigger produces output, the associated block will be run (optionally with the trigger's output as command line argument).

# Notable features

- Define **labels** for your blocks. E.g., for a volume block, have it return `35 %` and define the label `Vol.` in the config.
- Define a **prefix and suffix** for every block. Want to wrap all blocks in square brackets? That's 2 lines in the main config.
- Define a **minimum width** for your blocks to achieve a uniform look when using fixed-width fonts.
- Prefix, suffix, label and actual block content can have different foreground and background colors.
- Most settings can be set once for all blocks, then overwritten for individual blocks, if need be.

# Dependencies

- [`inih`](https://github.com/benhoyt/inih) (`libinih-dev` in Debian, but also included in this repo, see below)

# Portability

succade uses [libkita](https://github.com/domsson/libkita) to manage child processes. libkita uses `epoll`, which is Linux only. I've attempted to port libkita to BSD using `kqueue`, but couldn't get it to work reliably (yet).

# Installation 

Make sure you have `lemonbar` (obviously), `gcc` (for compiling the source code) and all dependencies, as listed above, installed. If `inih` is not available in your distribution, just replace `./build` with `./build-inih` below and you should be good to go.

1. Clone succade and change into its directory:  
   `git clone https://github.com/domsson/succade.git`  
   `cd succade`
2. Make the build script executable, then run it:  
   `chmod +x ./build`  
   `./build`
3. Create the config directory (assuming `.config` as your config dir):  
   `mkdir ~/.config/succade`  
4. Copy the example config:  
   `cp ./cfg/example1.ini ~/.config/succade/succaderc`  
5. Make `succade` executable and put it somewhere that's included in your path:  
   `chmod +x ./bin/succade`  
   `cp ./bin/succade ~/.local/bin/`

# Configuration

Take a look at the example configurations in this repository and refer to the following documentation.

Possible property values, based on their types as listed in the tables below, are:

- `string`: Text within quotes, for example `"Hello World"`
- `number`: A number, for example `34`
- `boolean`: Either `true` or `false`
- `color`: RGB hex string, for example `#F3BD70`

# Commands in config options 

The config options `command`, `trigger`, `mouse-left`, `mouse-middle`, `mouse-right`, `scroll-up`, `scroll-down` expect a script or binary to execute. For performance reasons, succade does _not_ invoke a shell to run the commands. This means that shell built-in functionality, like `echo`, pipes or redirection, will not work (as expected). If you want to use those, wrap those commands in a simple shell script and give succade the path to that script in these config options. You also don't need (and should not) background commands via `&`, succade will take care of that for you already.

You can, however, use variable substituion, `.` and `~`, as succade internally uses [wordexp](https://linux.die.net/man/3/wordexp). Also see the following paragraph from the wordexp man page:

> The expansion done consists of the following stages: tilde expansion (replacing ~user by user's home directory), variable substitution (replacing $FOO by the value of the environment variable FOO), command substitution (replacing $(command) or `command` by the output of command), arithmetic expansion, field splitting, wildcard expansion, quote removal. 

## lemonbar

The special section `bar` configures Lemonbar itself and can define common formatting for all blocks. It is required for succade to run, but the only mandatory property is `blocks`.

| Parameter          | Type    | Description |
|--------------------|---------|-------------|
| `command`          | string  | The command to start the bar; defaults to `lemonbar` | 
| `blocks`           | string  | Specifies the blocks to display on the bar. Example: <code>desktop  &#124; title  &#124; volume time</code> |
| `width`            | number  | Width of the bar in pixel - omit this value for a full-width bar. |
| `height`           | number  | Height of the bar in pixel - omit to get the minimum required height. |  
| `left`             | number  | x-position of the bar - omit to have it sit at the edge of your screen. |
| `top`              | number  | y-position of the bar - omit to have it sit at the edge of your screen. |
| `bottom`           | boolean | Dock the bar at the bottom instead of the top of the screen. |
| `force`            | boolean | Set to `true` if you want to force docking of Lemonbar; default is `false`. |
| `foreground`       | color   | Default foreground (font) color for all blocks. |
| `background`       | color   | Default background color for the entire bar. |
| `font`             | string  | Font to use for all blocks. |
| `label-font`       | string  | Font to use for all block's labels, if any. |
| `affix-font`       | string  | Font to use for all block's prefixes / suffixes, if any. |
| `line-color`       | color   | Color for all underlines / overlines, if any. |
| `line-width`       | number  | Thickness of all underlines / overlines, if any, in pixels. |
| `separator`        | string  | String to place in between any two blocks of the same alignment. |

## blocks

Every block that has been named in `blocks` needs its own config section. Most of these values can also be specified in the special `default` section, which will apply to all blocks.

| Parameter          | Type    | Description |
|--------------------|---------|-------------|
| `command`          | string  | The command to run the block; defaults to the section name. |
| `interval`         | number  | Run the block every `interval` seconds; `0` (default) means the block will only be run once. |
| `trigger`          | string  | Run the block whenever the command given here prints something to `stdout`. |
| `consume`          | boolean | Use the trigger's output as command line argument when running the block. |
| `live`             | boolean | The block is supposed to keep running; succade will monitor it for new output on `stdout`. |
| `raw`              | boolean | If `true`, succade will not escape '%' characters, allowing you to use format strings directly. |
| `prefix`           | string  | Shown before the block's main text and label. |
| `suffix`           | string  | Shown after the block's main text and unit, if any. |
| `label`            | string  | Shown before the block's main text; useful to display icons when using fonts like Siji. |
| `min-width`        | number  | Minimum width of the block's main text, which will be left-padded with spaces if neccessary. |
| `foreground`       | color   | Font color for the whole block (including label and affixes). |
| `background`       | color   | Background color for the whole block (including label and affixes). |
| `label-foreground` | color   | Font color for the block's label, if any. |
| `label-background` | color   | Background color for the block's label, if any. |
| `affix-foreground` | color   | Font color for the block's prefix and suffix, if any. |
| `affix-background` | color   | Background color for the block's prefix and suffix, if any. |
| `line-color`       | color   | Overline / underline color for the block. |
| `overline`         | boolean | Whether or not to draw an overline for the block. |
| `underline`        | boolean | Whether or not to draw an underline for the block. |
| `margin`           | number  | Distance to the next block (or edge of bar) on the left and right, in pixels. |
| `margin-left`      | number  | Distance to the next block (or edge of bar) on the left, in pixels. |
| `margin-right`     | number  | Distance to the next block (or edge of bar) on the right, in pixels. |
| `padding`          | number  | Number of spaces that will be added around a block's output on the left and right. |
| `padding-left`     | number  | Number of spaces that will be added on the left side of a block's output. |
| `padding-right`    | number  | Number of spaces that will be added on the right side of a block's output. |
| `mouse-left`       | string  | Command to run when you left-click the block. |
| `mouse-middle`     | string  | Command to run when you middle-click the block. |
| `mouse-right`      | string  | Command to run when you right-click the block. |
| `scroll-up`        | string  | Command to run when you scroll your mouse wheel up while hovering over the block. |
| `scroll-down`      | string  | Command to run when you scroll your mouse whell down while hovering over the block. |

# Usage and command line arguments

Usage:

    succade [OPTIONS...]

Options:

- `c CONFIG`: config file to use
- `e`: run bar even if it is empty (no blocks defined or loaded)
- `h`: print help text and exit
- `s SECTION`: config section name for the bar (default is "bar")
- `V`: print version information and exit

# Support

[![ko-fi](https://www.ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/L3L22BUD8)

# Blocks - Fetching System Information

Looking for scripts, programs or code that can fetch information to display on your bar? Check out [fetch-all-the-things](https://github.com/domsson/fetch-all-the-things).

# License

succade is public domain software, do with it whatever you want. However, succade uses [`inih`](https://github.com/benhoyt/inih), which is under the New BSD license. 

# Motivation 

With projects like [polybar](https://github.com/polybar/polybar), the question for the relevance of succade is justified. Personally, I prefer succade - and similar solutions, like [Captain](https://github.com/muse/Captain) - because they enforce the separation of concerns as described by the [UNIX philosophy](https://en.wikipedia.org/wiki/Unix_philosophy).

For example, imagine someone created a fork of Lemonbar that works with Wayland. As long as they would keep the same interface (same format specifiers supported as with Lemonbar), you can immediately switch to that new bar, without changing anything else. You can still use the same blocks, because they are not tied to the bar or succade.

Additionally, I like minimalistic setups. I don't want most of the additional features of other bars, like true type fonts or rounded borders. Hence, I might as well save a bit of RAM by going with more minimalistic solutions.

Fun fact: polybar started out as a lemonbar wrapper just like succade, but eventually the project maintainers decided to include lemonbar's functionality right into the project. Therefore, in a way, succade is what polybar used to be.
