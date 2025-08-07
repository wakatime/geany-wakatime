# Geany WakaTime Plugin

Automatic time tracking for programmers using WakaTime in Geany IDE.

## Prerequisites

### Install Geany

**macOS:**

⚠️ **Important**: You need the Homebrew formula version (not cask) for plugin development.

If you already have Geany cask installed:
```bash
brew uninstall --cask geany
brew install geany
```

If you don't have Geany installed:
```bash
brew install geany
```

**Ubuntu/Debian:**
```bash
sudo apt-get install geany geany-dev libgtk-3-dev
```

**CentOS/RHEL/Fedora:**
```bash
sudo yum install geany geany-devel gtk3-devel
# or on newer versions:
sudo dnf install geany geany-devel gtk3-devel
```

### Install WakaTime CLI

The plugin requires wakatime-cli to be installed. Install it with:

```bash
pip install wakatime
```

Or download from: https://github.com/wakatime/wakatime-cli/releases

## Installation

1. Clone this repository:
```bash
git clone https://github.com/wakatime/geany-wakatime.git
cd geany-wakatime
```

2. Compile the plugin:
```bash
make
```

3. Install the plugin:
```bash
make install
```

4. Restart Geany

5. Enable the plugin:
   - Go to **Tools → Plugin Manager**
   - Check **WakaTime** in the plugin list
   - Click **OK**

## Configuration

1. Get your WakaTime API key from: https://wakatime.com/api-key

2. Create a `~/.wakatime.cfg` file with your API key:
```ini
[settings]
api_key = your-api-key-here
```

That's it! The plugin will now automatically track your coding time.

## Features

- **Automatic tracking**: Sends heartbeats when you open, save, or switch between files
- **Rate limiting**: Only sends one heartbeat per 2 minutes per file (except saves)
- **Cross-platform**: Works on macOS, Linux, and other Unix-like systems
- **No configuration needed**: Works out of the box once API key is set

## Troubleshooting

### Plugin not appearing in Plugin Manager
- Make sure Geany development headers are installed
- Check that the plugin compiled without errors
- Verify the plugin file exists in `~/.config/geany/plugins/wakatime.so`

### Time not being tracked
- Check that wakatime-cli is installed and in your PATH
- Verify your `~/.wakatime.cfg` file exists and contains your API key
- Look for error messages in Geany's message window
- Check WakaTime logs at `~/.wakatime/wakatime.log`

### Compilation errors
- Ensure all dependencies are installed
- Try running `make clean && make` to rebuild from scratch

## Uninstallation

```bash
make uninstall
```

## License

BSD 3-Clause License