# succade

Feed your [Lemonbar](https://github.com/LemonBoy/bar) with succade! It repeatedly runs [blocks](https://github.com/domsson/candies), piping their output to Lemonbar. Every block is a script or program that provides text to be displayed on your bar. Configuration is done with a simple [ini file](https://en.wikipedia.org/wiki/INI_file).

![succade lemonbar example](https://i.imgur.com/vUtF1o5.png)

# How it works

- Starts `lemonbar` for you
- Reads the config file (default is `~/.config/succade/succaderc`)
- Loads blocks (programs or scripts defined in the config file)
- Updates Lemonbar based on the block's output 

The config file needs to have one section for lemonbar and one per block. The bar's section lists the blocks that should be displayed on the bar (and where), as well as some styling for all blocks (like prefixes and suffixes). Block sections define the styling of individual blocks, as well as how often the block should be reloaded. Alternatively, trigger commands can be defined. Once a trigger produces output, the associated block will be run (optionally with the trigger's output as command line argument). 

# Notable features

- Define **labels** for your blocks. E.g., for a volume block, have it return `35 %` and define the label `Vol.` in the config.
- Define a **prefix and suffix** for every block. Want to wrap all blocks in square brackets? That's 2 lines in the main config.
- Define **padding** (fixed width) for your blocks to achieve a uniform look when using fixed-width fonts.
- Prefix, suffix, label and actual block content can have different foreground and background colors.
- Most settings can be set once for all blocks, then overwritten for individual blocks, if need be.

# Dependencies

- [`inih`](https://github.com/benhoyt/inih) (`libinih-dev` in Debian)
- [`libkita`](https://github.com/domsson/libkita/) (by me, written for succade, public domain)

# Installation 

Make sure you have `lemonbar` (obviously), `gcc` (for compiling the source code) and all dependencies, as listed above, installed.

1. Make the build script executable, then run it:  
   `chmod +x ./build`  
   `./build`
2. Create the config directory (assuming `.config` as your config dir):  
   `mkdir ~/.config/succade`  
3. Copy the example config, then edit it to your needs:  
   `cp succaderc ~/.config/succade`  
   `vim ~/.config/succade/succaderc`
4. Make `succade` executable and put it somewhere that's included in your path:  
   `chmod +x bin/succade`  
   `cp bin/succade ~/.local/bin/`

# Configuration

Take a look at the example configuration in this repository and refer to the following documentation.

Possible property values, based on their types as listed in the tables below, are:

- `string`: Text within quotes, for example `"Hello World"`
- `number`: A number, for example `34`
- `boolean`: Either `true` or `false`
- `color`: RGB hex string, for example `#F3BD70`

## lemonbar

The special section `bar` configures Lemonbar itself and can define common formatting for all blocks. It is required for succade to run, but the only mandatory property is `format`.

| Parameter          | Type    | Description |
|--------------------|---------|-------------|
| `format`           | string  | Specifies the blocks to display on the bar. Example: <code>desktop  &#124; title  &#124; volume time</code> |
| `bin`              | string  | The command to start the bar; defaults to `lemonbar` | 
| `width`            | number  | Width of the bar in pixel - omit this value for a full-width bar. |
| `height`           | number  | Height of the bar in pixel. |  
| `x`                | number  | x-position of the bar - omit to have it sit at the edge of your screen. |
| `y`                | number  | y-position of the bar - omit to have it sit at the edge of your screen. |
| `dock`             | string  | Position of the bar; possible values are `bottom` and `top` (default). |
| `force`            | boolean | Set to `true` if you want to force docking of Lemonbar; default is `false`. |
| `foreground`       | color   | Foreground (font) color for all blocks. |
| `background`       | color   | Background color for the entire bar. |
| `block-background` | color   | Background color for all blocks. |
| `label-foreground` | color   | Font color for all block's labels. |
| `label-background` | color   | Background color for all block's labels. |
| `affix-foreground` | color   | Font color for all block's prefixes / suffixes. |
| `affix-background` | color   | Background color for all block's prefixes / suffixes. |
| `block-prefix`     | string  | A string that will be prepended to every block, for example a space: `" "`. |
| `block-suffix`     | string  | Same as the prefix, but will be added to the end of every block. |
| `block-font`       | string  | Font to use for all blocks. |
| `label-font`       | string  | Font to use for all block's labels, if any. |
| `affix-font`       | string  | Font to use for all block's prefixes / suffixes, if any. |
| `line-color`       | color   | Color for all underlines / overlines, if any. |
| `line-width`       | number  | Thickness of all underlines / overlines, if any, in pixels. |
| `overline`         | boolean | Whether or not to draw an overline for all blocks. |
| `underline`        | boolean | Whether or not to draw an underline for all blocks. |
| `block-offset`     | number  | Distance between any two blocks in pixel; default is `0`. |

## blocks

Every block that has been named in `format` needs its own config section. Some of the values that can be set here are the same as in the `bar` section - if so, they will overwrite the values specified there. This way, you can set a default font color for the entire bar, but decide to give some blocks a different one.

| Parameter          | Type    | Description |
|--------------------|---------|-------------|
| `bin`              | string  | The command to run the block; defaults to the section name. |
| `reload`           | number  | Run the block every `interval` seconds; `0` (default) means the block will only be run once. |
| `trigger`          | string  | Run the block whenever the command given here prints something to `stdout`. |
| `consume`          | boolean | Use the trigger's output as command line argument when running the block. |
| `live`             | boolean | The block is supposed to keep running; succade will monitor it for new output on `stdout`. |
| `label`            | string  | Shown before the block's main text; useful to display icons when using fonts like Siji. |
| `padding`          | number  | Minimum width of the block's main text, which will be left-padded with spaces if neccessary. |
| `foreground`       | color   | Font color for the whole block (including label and affixes). |
| `background`       | color   | Background color for the whole block (including label and affixes). |
| `label-foreground` | color   | Font color for the block's label, if any. |
| `label-background` | color   | Background color for the block's label, if any. |
| `affix-foreground` | color   | Font color for the block's prefix and suffix, if any. |
| `affix-background` | color   | Background color for the block's prefix and suffix, if any. |
| `line-color`       | color   | Overline / underline color for the block. |
| `overline`         | boolean | Whether or not to draw an overline for the block. |
| `underline`        | boolean | Whether or not to draw an underline for the block. |
| `offset`           | number  | Distance to the next block, in pixels. |
| `mouse-left`       | string  | Command to run when you left-click the block. |
| `mouse-middle`     | string  | Command to run when you middle-click the block. |
| `mouse-right`      | string  | Command to run when you right-click the block. |
| `scroll-up`        | string  | Command to run when you scroll your mouse wheel up while hovering over the block. |
| `scroll-down`      | string  | Command to run when you scroll your mouse whell down while hovering over the block. |

# Usage and command line arguments

Usage:

    succade [OPTIONS...]

Options:

- `c`: config file to use
- `e`: run bar even if it is empty (no blocks defined or loaded)
- `h`: print a help text and exit
- `s SECTION`: config section name for the bar (default is "bar")

# Why succade?

With projects like [polybar](https://github.com/polybar/polybar), the question for the relevance of succade is justified. Personally, I prefer succade - and similar solutions, like [Captain](https://github.com/muse/Captain) - because they enforce the separation of concerns as described by the [UNIX philosophy](https://en.wikipedia.org/wiki/Unix_philosophy).

For example, imagine someone created a fork of Lemonbar that works with Wayland. As long as they would keep the same interface (same format specifiers supported as with Lemonbar), you can immediately switch to that new bar, without changing anything else. You can still use the same blocks, because they are not tied to the bar or succade.

# License

succade is free software, dedicated to the public domain. Do with it whatever you want, but don't hold me responsible for anything either. However, be aware that the libaries this project is using (see section _Dependencies_) might use a different license. If so, distribution of the executable binary of this project would be affected by such licenses. For that reason, I do not provide a binary, only the source code.
