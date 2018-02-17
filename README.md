# succade

Feed your [Lemonbar](https://github.com/LemonBoy/bar) with succade! It works almost exactly like [Captain](https://github.com/muse/Captain): succade starts Lemonbar for you, then repeatedly runs blocks and pipes their output to Lemonbar. Every block is a script or program that provides text to be displayed on your bar. Configuration is done with simple [ini files](https://en.wikipedia.org/wiki/INI_file).

![Example bar](https://i.imgur.com/IQ26ypO.png)

# Status

This is currently a work in progress and I'm sure there are nasty bugs still to be found - this is my first time writing C. However, succade seems to work quite well for me already. You might want to give it a try!

# How does it work?

- Starts `lemonbar` for you
- Reads the config file `~/.config/succade/succaderc`
- Loads blocks (scripts) and their config files from `~/.config/succade/blocks`
- Updates Lemonbar based on the block's reloads or triggers

The general config file defines styling and position for the whole bar, lists the blocks that should be displayed on the bar (and where), as well as some styling for all blocks (prefixes and suffixes).

Every block can have a config file that defines its styling, as well as how often the block should be reloaded. Alternatively, a trigger command can be defined.

Triggers are commands that succade will run and monitor for output. When there is output, succade will run the associated block with the trigger's ouput as command line argument.

# What's missing?

- Support for clickable areas
- Support for fonts
- Support for underlines
- Support for multiple bars
- Support for multiple monitors
- Lots of testing to find and fix bugs
- Refactoring / cleaning up

# Dependencies

- `lemonbar`, obviously
- `inih`, but that's included in this repository

# How to build

Rund the `build` script.

# How to install

- Clone this git
- Create the dir `~/.config/succade` and `~/.config/succade/blocks`
- Copy or create `succaderc` to `~/.config/succade/succaderc`
- Create or copy blocks in `~/.config/succade/blocks` (make sure blocks are executable)
- Make sure `succade` is somewhere in your path, for example `~/.local/bin`

# How to configure

Take a look at the example configuration in this repository. The bar and some general configuration happens in `succaderc`, plus every block can have its own `<blockname>.ini` file. As succade is still in active development, the configuration parameters available and the names of those already in place are subject to change. However, I'm trying to keep a high compatibility to Captain. Check our the example files here, as well as the Captain wiki and play around. Eventually, as things are finally decided upon, there will be extensive documentation here.

