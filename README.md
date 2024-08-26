# Voice Assistant Service

This is a basic Voice Assistant example that accepts voice commands from the microphone.

```bash
# Run with default arguments and small model
./main -m ./models/ggml-small.en.bin -t 8

# On Raspberry Pi, use tiny or base models + "-ac 768" for better performance
./main -m ./models/ggml-tiny.en.bin -ac 768 -t 3 -c 0
```

## Building

This tool depends on SDL2 library to capture audio from the microphone. Here's how to install:

```bash
# Install SDL2
# On Debian based linux distributions:
sudo apt-get install libsdl2-dev

# On Fedora Linux:
sudo dnf install SDL2 SDL2-devel

# Install SDL2 on Mac OS
brew install sdl2
```

Start cloning the repository with the required submodules:

```bash
git clone --recurse-submodules https://github.com/AndhikaFW/whisper-assistant-service.git
```
Then, download one of the Whisper models converted in [`ggml` format](#ggml-format). For example:

```bash
bash ./lib/whisper.cpp/models/download-ggml-model.sh base.en
```

Now you can build it like this:

```bash
make
```

## Building Apps

This tool is able to execute apps from the main program. Here's how you can build the example app:

```bash
make example
```

## `ggml` format

The original models are converted to a custom binary format. This allows to pack everything needed into a single file:

- model parameters
- mel filters
- vocabulary
- weights

You can download the converted models using the ``lib/whisper.cpp/models/download-ggml-model.sh`` script
or manually from here:

- https://huggingface.co/ggerganov/whisper.cpp
- https://ggml.ggerganov.com

For more details, see the conversion script ``lib/whisper.cpp/models/convert-pt-to-ggml.py`` or ``lib/whisper.cpp/models/README.md``.


## License
--
Voice Assistant Service is released under [MIT License](https://opensource.org/licenses/MIT).
