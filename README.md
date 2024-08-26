# Voice Assistant Service

This is a basic Voice Assistant example that accepts voice commands from the microphone.

```bash
# Run with default arguments and small model
./main -m ./models/ggml-small.en.bin -t 8

# On Raspberry Pi, use tiny or base models + "-ac 768" for better performance
./main -m ./models/ggml-tiny.en.bin -ac 768 -t 3 -c 0
```

## Building

This tool depends on SDL2 library to capture audio from the microphone. You can build it like this:

```bash
# Install SDL2
# On Debian based linux distributions:
sudo apt-get install libsdl2-dev

# On Fedora Linux:
sudo dnf install SDL2 SDL2-devel

# Install SDL2 on Mac OS
brew install sdl2

make
```

## Building Apps

This tool is able to execute apps from the main program. Here's how you can build the example app:

```bash
make example
```

## License
--
Voice Assistant Service is released under [MIT License](https://opensource.org/licenses/MIT).
