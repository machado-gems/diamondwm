# diamondwm
Diamond X Window Manager

A simple, lightweight X11 window manager with a diamond-inspired design. Minimalist and efficient window management for X11 systems.

## Requirements

### Dependencies
- X11 development libraries
- feh (for wallpaper support)
- xterm (default terminal)
- x11-apps (X11 utilities)

### Install Dependencies
```bash
sudo apt update
sudo apt install libx11-dev
sudo apt install libxext-dev
sudo apt install feh
sudo apt install xterm
sudo apt install x11-apps
```

## Installation
### Compile from Source
```bash
gcc -o diamondwm diamondwm.c -lX11
```

### Install System-wide
```bash
sudo cp diamondwm /usr/local/bin/
sudo chmod +x /usr/local/bin/diamondwm
```

### Create Session File
```bash
sudo nano /usr/share/xsessions/diamondwm.desktop
```

### Add the following content to the file:
```ini
[Desktop Entry]
Name=DiamondWM
Comment=Simple diamond-inspired window manager
Exec=diamondwm
Type=XSession
DesktopNames=DiamondWM
```

## Usage
Select "DiamondWM" from your display manager's session menu to start using the window manager.

## Features
- Minimalist window management
- Diamond-inspired layout algorithms
- Lightweight and fast
- Basic window operations (move, resize, close)
- Customizable through source code

## License
This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing
Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Building from Source
1. Ensure you have the required dependencies installed
2. Clone or download the source code  
3. Compile using:
   ```bash
   gcc -o diamondwm diamondwm.c -lX11
   ```
## Troubleshooting
If you encounter any issues:

- Ensure all dependencies are properly installed
- Check that the diamondwm binary is in your PATH
- Verify the session file is correctly placed in /usr/share/xsessions/
- Check system logs for any error messages
