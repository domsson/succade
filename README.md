# succade

Feed your [Lemonbar](https://github.com/LemonBoy/bar) with succade! It works almost exactly like [Captain](https://github.com/muse/Captain): succade starts Lemonbar for you, then repeatedly runs blocks, piping their output to Lemonbar. Every block is a script or program that provides text to be displayed on your bar. Configuration is done with simple [ini files](https://en.wikipedia.org/wiki/INI_file).

![Example bar](https://i.imgur.com/IQ26ypO.png)
![Example bar](https://i.imgur.com/6iCKW3w.png)

# How does it work?

- Starts `lemonbar` for you
- Reads the config file `~/.config/succade/succaderc`
- Loads blocks (scripts) and their config files from `~/.config/succade/blocks`
- Updates Lemonbar based on the block's reloads or triggers

The general config file defines styling and position for the whole bar, lists the blocks that should be displayed on the bar (and where), as well as some styling for all blocks (like prefixes and suffixes).

Every block can have a config file that defines its styling, as well as how often the block should be reloaded. Alternatively, a trigger command can be defined.

Triggers are commands that succade will run and monitor for output. When there is output, succade will run the associated block with the trigger's ouput as command line argument.

# Notable features

- You can define **labels** for your blocks. Example: for a volume block, have it return "35 %" and define the label "Vol." in the config.
- You can define a **prefix and suffix** for every block. Want to wrap all blocks in square brackets? That's two lines in the main config.
- Prefix, suffix, label and actual block content can have different foreground and background colors.
- Most settings can be set for all blocks (in the main config) but overwritten for individual blocks in their own config, if need be.

# What doesn't work

- There is no conditional formatting, so you can't have blocks change their color depending on the status of your battery, for example.

# What's still on the to-do list?

- Support for _live_ blocks
- Support for multiple bars
- Support for multiple monitors
- Lots of testing to find and fix bugs (your help is appreciated!)
- Some Refactoring

# Dependencies

- `lemonbar`, obviously
- `inih`, but that's included in this repository
- `gcc`, to compile succade

# How to install

- Clone this repository:  
  `git clone https://github.com/domsson/succade.git`
- Change into the succade directory:  
  `cd succade`
- Make the build script executable, then run it:  
  `chmod +x ./build`  
  `./build`
- Create the config directories:  
  `mkdir ~/.config/succade`  
  `mkdir ~/.config/succade/blocks`
- Copy the example config and example blocks:  
  `cp succaderc ~/.config/succade`  
  `cp blocks/* ~/.config/succade/blocks`
- Make sure the blocks are executable (`chmod +x ~/.config/succade/blocks/name-of-block`)
- Make `succade` executable and put it somwhere that's included in your path:  
  `chmod +x succade`  
  `cp succade ~/.local/bin`

# How to configure

Take a look at the example configuration in this repository. The general configuration of the bar happens in `succaderc`. Additionally, every block can have its own `<blockname>.ini` file. As succade is still in active development, the configuration parameters available are subject to change. However, I'm trying to keep a high compatibility to Captain.

## succaderc

`succaderc` is the config file for the bar itself. You need this file, otherwise succade won't start. At least the `format` property needs to be defined, everything else is optional.

- `format` (string, required)  
   Specifies what blocks to display on the bar. Write down the file names of your blocks, separated by spaces. By adding two pipes you can align the blocks left, center or right, depending on whether you note down the block names on the left of both pipes, the right of both pipes or in the middle of them. Example: `desktop | title | time`
- `w` or `width` (int)  
   Width of the bar in pixel - omit this value for a full-width bar.
- `h` or `height` (int)  
   Height of the bar in pixel.
- `x` (int)  
   x-position of the bar - omit to have it sit at the edge of your screen.
- `y` (int)  
   y-position of the bar - omit to have it sit at the edge of your screen.
- `fg` or `foreground` (string)  
   Foreground color (font color) in hex format, for example `#FF0000` for red.
- `bg` or `background` (string)  
   Background color in hex format.
- `lc` or `line` (string)  
   Color for underlines and overlines, if used.
- `lw` or `line-width` (int)  
   Thickness of the underlines/overlines in pixel.
- `prefix` (string)  
   A string that will be prepended to every block, for example a space: `" "`.
- `suffix`  (string)  
   Same as the prefix, but will be added to the end of every block.
- `dock` (string)  
   Set to `bottom` if you want the bar to sit at the bottom of your screen.
- `force` (bool)  
   Set to `true` if you want to force docking of Lemonbar.
- `font` or `block-font` (string)  
   Font to use for the body of the blocks (will be used for everything by default).
- `label-font` (string)  
   Font to use only for the block's labels.
- `affix-font` (string)  
   Font to use for the block's prefixes / suffixes.

## name-of-block.ini

In the block directory, you can create one config file for each block. The file name should be the same as the block, but end in `.ini`. For example, if you have a `time` block (script), name the config file `time.ini`. Some of the values that can be set in these files are the same as in the succaderc file - if so, they will overwrite the behaviour specified there. This way, you can specify a default font color in `succaderc`, but decide to give some blocks a different one via their own config.

- `fg` or `foreground` (string)  
   See above.
- `bg` or `background` (string)  
   See above.
- `lc` or `line` (string)  
   See above.
- `ol` or `overline` (bool)  
   Set to `true` in order to draw an overline for this block.
- `ul` or `underline` (bool)  
   Set to `true` in order to draw an underline for this block.
- `pad` or `padding` (int)  
   Defines the minimum width for the block's main text, that is, the text between prefix, label and suffix. If you set this to `3`, succade will make sure that the values returned from your block will display with 3 characters by appending spaces to the left, if required. This can help achieve a uniform look when using monospace fonts.
- `offset` (int)  
   Defines an offset, in pixel, that this block should have to the next block (if any).
- `label` (string)  
   A string that will be displayed before the block's main text, but after the prefix. Can be used to display an icon by using an appropriate font, like Siji.
- `reload` (float)  
  Defines in what interval (in seconds) this block should be run. Setting this to `5` will run this block every 5 seconds. Lower values will lead to more CPU usage. A third option is to set `reload` to `0`. In this case, succade will run your block once, and only once, effectively creating a static block.
- `trigger` (string)  
  If your block should be run depending on the output of another command, then set this command here. If you do this, succade will run that trigger command and monitor its output. Whenever the command produces new output, succade will run the block - and pipe the trigger's output as input to the block. This will set `reload` to `0`.
- `mouse-left` (string)  
  Set this to a command that you want succade to run when you left-click this block.
- `mouse-middle` (string)
  See above.
- `mouse-right` (string)  
  See above.
- `scroll-up` (string)
  Set this to a command that you want succade to run when you scroll your mouse wheel up while hovering over this block.
- `scroll-down` (string)  
  See above.

# Licence

- succade is free software, dedicated to the public domain. Do with it whatever you want, but don't hold me responsible for anything either.
- [inih](https://github.com/benhoyt/inih) is licensed under the new BSD license, see file headers.
