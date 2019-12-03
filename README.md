# succade

Feed your [Lemonbar](https://github.com/LemonBoy/bar) with succade! It works almost exactly like [Captain](https://github.com/muse/Captain): succade starts Lemonbar for you, then repeatedly runs blocks, piping their output to Lemonbar. Every block is a script or program that provides text to be displayed on your bar. Configuration is done with simple [ini files](https://en.wikipedia.org/wiki/INI_file).

![Example bar](https://i.imgur.com/IQ26ypO.png)
![Example bar](https://i.imgur.com/6iCKW3w.png)

# How it works

- Starts `lemonbar` for you
- Reads the config file `~/.config/succade/succaderc`
- Loads blocks (config files that point to binaries/scripts) from `~/.config/succade/blocks`
- Updates Lemonbar based on the block's reloads or triggers

The general config file defines styling and position for the whole bar, lists the blocks that should be displayed on the bar (and where), as well as some styling for all blocks (like prefixes and suffixes).

Every block can have a config file that defines its styling, as well as how often the block should be reloaded. Alternatively, a trigger command can be defined.

Triggers are commands that succade will run and monitor for output. When there is output, succade will run the associated block with the trigger's ouput as command line argument.

# Notable features

- Define **labels** for your blocks. E.g., for a volume block, have it return `35 %` and define the label `Vol.` in the config.
- Define a **prefix and suffix** for every block. Want to wrap all blocks in square brackets? That's 2 lines in the main config.
- Define **padding** (fixed width) for your blocks to achieve a uniform look when using fixed-width fonts.
- Prefix, suffix, label and actual block content can have different foreground and background colors.
- Most settings can be set once for all blocks, then overwritten for individual blocks in their own config, if need be.

# What it can't do

- There is no conditional formatting, blocks always have the colors as defined in the config.

# To-Do List

- [x] Support for _live_ blocks
- [ ] Support for multiple bars
- [ ] Support for multiple monitors
- [ ] Lots of testing to find and fix bugs (your help is appreciated!)
- [ ] Some Refactoring

# Dependencies

- [`inih`](https://github.com/benhoyt/inih) (`libinih-dev` in Debian)

# How to install

Make sure you have `lemonbar` (obviously), `gcc` (for compiling the source code) and all dependencies, as listed above, installed.

1. Clone this repository:  
   `git clone https://github.com/domsson/succade.git`
2. Change into the succade directory:  
   `cd succade`
3. Make the build script executable, then run it:  
   `chmod +x ./build`  
   `./build`
4. Create the config directories (assuming `.config` as your config dir):  
   `mkdir ~/.config/succade`  
   `mkdir ~/.config/succade/blocks`
5. Create a directory for the example block scripts:
   `mkdir ~/.local/bin/candies`
6. Copy the example config:
   `cp succaderc ~/.config/succade`  
7. Copy the example block config files:
   `cp blocks/cfg/* ~/.config/succade/blocks`
8. Copy the example block scripts:
   `cp blocks/bin/* ~/.local/bin/candies`
7. Make sure the blocks are executable:  
   `chmod +x ~/.local/bin/candies/*`
8. Make `succade` executable and put it somwhere that's included in your path:  
   `chmod +x bin/succade`  
   `cp bin/succade ~/.local/bin/`

# How to configure

Take a look at the example configuration in this repository. The general configuration of the bar happens in `succaderc`. Additionally, every block can have its own `<blockname>.ini` file. As succade is still in active development, the configuration parameters available are subject to change. However, I'm trying to keep a high compatibility to Captain.

## succaderc

`succaderc` is the config file for the bar itself. You need this file, otherwise succade won't start. At least the `format` property needs to be defined, everything else is optional.

| Parameter             | Alias      | Type    | Description |
|-----------------------|------------|---------|-------------|
| `format`              |            | string  | Specifies what blocks to display on the bar: the names of your blocks, separated by spaces. By adding two pipes you can align the blocks left, center or right, depending on whether you note down the block names on the left of both pipes, the right of both pipes or in the middle of them. Example: <code>desktop  &#124; title  &#124; time</code> |
| `width`               | `w`        | number  | Width of the bar in pixel - omit this value for a full-width bar. |
| `height`              | `h`        | number  | Height of the bar in pixel. |  
| `x`                   |            | number  | x-position of the bar - omit to have it sit at the edge of your screen. |
| `y`                   |            | number  | y-position of the bar - omit to have it sit at the edge of your screen. |
| `dock`                |            | string  | Set to `bottom` or `top` (default), depending on where you want the bar to show up on your screen. |
| `force`               |            | boolean | Set to `true` if you want to force docking of Lemonbar. Default is `false`. |
| `foreground`          | `fg`       | color   | Default foreground color (font color) for all blocks. Hex format (example: `#7E22C3`). |
| `background`          | `bg`       | color   | Default background color for the entire bar. |
| `block-background`    | `block-bg` | color   | Background color for all blocks (while `bg` tints the whole bar). |
| `label-foreground`    | `label-fg` | color   | Font color for all block's labels. |
| `label-background`    | `label-bg` | color   | Background color for all block's labels. |
| `affix-foreground`    | `affix-fg` | color   | Font color for all block's prefixes / suffixes. |
| `affix-background`    | `affix-bg` | color   | Background color for all block's prefixes / suffixes. |
| `block-prefix`        | `prefix`   | string  | A string that will be prepended to every block, for example a space: `" "`. |
| `block-suffix`        | `suffix`   | string  | Same as the prefix, but will be added to the end of every block. |
| `block-font`          | `font`     | string  | Default font to use for all blocks. |
| `label-font`          |            | string  | Font to use for all block's labels (affixes and actual block content won't be affected). |
| `affix-font`          |            | string  | Font to use for all block's prefixes / suffixes (does not affect label and block content). |
| `line`                | `lc`       | color   | Default underline / overline color for all blocks. |
| `line-width`          | `lw`       | number  | Thickness of all underlines / overlines, if any, in pixel. |
| `overline`            | `ol`       | boolean | Whether or not to draw an overline for all blocks. Set to `true` or `false`. |
| `underline`           | `ul`       | boolean | Whether or not to draw an underline for all blocks. |
| `block-offset`        | `offset`   | number  | Distance between any two blocks in pixel. Default is `0` |

## name-of-block.ini

In the block directory, you can create one config file for each block. The file name should end in `.ini`. The `bin` option points to the script/binary that should be run to create the desired output. Some of the values that can be set in these files are the same as in the succaderc file - if so, they will overwrite the behaviour specified there. This way, you can set a default font color in `succaderc`, but decide to give some blocks a different one via their own config.

| Parameter          | Alias      | Type    | Description |
|--------------------|------------|---------|-------------|
| `bin`              |            | string  | The binary/script that will be run to generate this block's output. This is (currently) required. |
| `reload`           |            | number  | Defines in what interval (in seconds) this block should be run. Setting this to `5` will run this block every 5 seconds. Lower values will lead to more CPU usage. A third option is to set `reload` to `0`. In this case, succade will run your block once, and only once, effectively creating a static block. |
| `trigger`          |            | string  | If your block should be run depending on the output of another command, then set this command here. If you do this, succade will run that trigger command and monitor its output. Whenever the command produces new output, succade will run the block - and pipe the trigger's output as input to the block. This will set `reload` to `0`. |
| `live`             |            | boolean | Puts the block into live mode, meaning that it will act as its own trigger (whenever a new line is being printed to `stdout`, `succade` will update `lemonbar` with the new output |
| `label`            |            | string  | A string to be displayed before the block's main text. Can be used to display an icon by using an appropriate font, like Siji. |
| `padding`          | `pad`      | number  | Minimum width of the block's main text. If required, succade will left-pad the string returned by the block with spaces. For example, if you set padding to `6` and your block returns `nice`, then succade will display that as <code>&nbsp;&nbsp;nice</code> (note the spaces). Useful for when you use fixed-width fonts. |
| `foreground`       | `fg`       | color   | Sets the font color for the whole block. Overwrites the default font color (see above). |
| `background`       | `bg`       | color   | Sets the background color for the whole block. Overwrites the default font color (see above). |
| `label-foreground` | `label-fg` | color   | Sets the font color this the label of this block. Overwrites the default label color. |
| `label-background` | `label-bg` | color   | Sets the background color this the label of this block. Overwrites the default. |
| `affix-foreground` | `affix-fg` | color   | Sets the font color for the prefix and suffix of this block. Overwrites the default. |
| `affix-background` | `affix-bg` | color   | Sets the background color for the prefix and suffix of this block. Overwrites the default. |
| `line`             | `lc`       | color   | Sets the overline / underline color. Overwrites the default (see above). |
| `overline`         | `ol`       | boolean | Whether or not to draw an overline for this block. Overwrites the default. |
| `underline`        | `ul`       | boolean | Whether or not to draw an underline for this block. Overwrites the default. |
| `offset`           |            | number  | Distance to the next block, in pixel. Overwrites the default. |
| `mouse-left`       |            | string  | Set this to a command that you want succade to run when you left-click this block. |
| `mouse-middle`     |            | string  | See `mouse-left`. |
| `mouse-right`      |            | string  | See `mouse-left`. |
| `scroll-up`        |            | string  | Set this to a command that you want succade to run when you scroll your mouse wheel up while hovering over this block. |
| `scroll-down`      |            | string  | See `scroll-up`. |

# License

succade is free software, dedicated to the public domain. Do with it whatever you want, but don't hold me responsible for anything either. However, be aware that the libaries this project is using (see section _Dependencies_) might use a different license. If so, distribution of the executable binary of this project would be affected by such licenses. For that reason, I do not provide a binary, only the source code.
