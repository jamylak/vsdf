# VSDF Quickstart

Go from zero to your first shader in minutes.

## 1) Install

### macOS
Install vsdf and its dependencies with Homebrew:
```sh
brew install jamylak/vsdf/vsdf
```

### Linux

**Easiest way to install vsdf:**
Pre-built binaries for Linux are available in the [GitHub Releases](https://github.com/jamylak/vsdf/releases) page.
The only dependency is Vulkan.

To get the **latest release**:

```sh
LATEST_RELEASE_TAG=$(curl -sL https://api.github.com/repos/jamylak/vsdf/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
DOWNLOAD_URL="https://github.com/jamylak/vsdf/releases/download/${LATEST_RELEASE_TAG}/vsdf-linux-x86_64.tar.gz"
echo "Downloading from: ${DOWNLOAD_URL}"
curl -LO "${DOWNLOAD_URL}"
tar -xzf vsdf-linux-x86_64.tar.gz
chmod +x linux/vsdf
sudo mv linux/vsdf /usr/local/bin/vsdf
rm -rf vsdf-linux-x86_64.tar.gz linux # Clean up downloaded files
```

### Windows (binary)

The only dependency is Vulkan.
Download the pre-built Windows release:

```powershell
$tag = (Invoke-RestMethod https://api.github.com/repos/jamylak/vsdf/releases/latest).tag_name
$zip = "vsdf-windows-x86_64.zip"
$url = "https://github.com/jamylak/vsdf/releases/download/$tag/$zip"
Invoke-WebRequest -Uri $url -OutFile $zip
Expand-Archive $zip -DestinationPath vsdf
.\vsdf\vsdf.exe --version
```

## 2) Quick Start - INSTANT shader development!

The fastest way to get started is by using the `vsdf` command line tool directly.

1. Create a new shader file:

```sh
vsdf --new-toy example.frag               # Creates 'example.frag' with a default template
vsdf --new-toy plot.frag --template plot  # Creates 'plot.frag' with the 2D plot template
```

2. Open the newly created shader file in your favorite editor (e.g., VS Code, Neovim, Sublime Text) and start editing!

```sh
code example.frag          # VS Code
# or: nvim example.frag    # Neovim
# or: subl example.frag    # Sublime Text
```

3. While you're editing, run `vsdf` in another terminal to see your changes hot-reload:
```sh
vsdf --toy example.frag
```

For a more integrated and faster *experimental* workflow with a shell function that creates the shader, opens the editor, and launches `vsdf` in one command, see the [Shell Integration Guide](SHELL_INTEGRATION.md).

## 3) Record video with `ffmpeg` (offline MP4 encoding)
```sh
vsdf --toy example.frag --frames 100 --ffmpeg-output out.mp4
```

## Notes
- `--toy` sets up push constants and other things in the format
ShaderToy uses, e.g., `iTime` as well as `main()` etc.
- `--new-toy` creates an example shader file starting point
- **Hot reload**: Saving your shader file automatically reloads it in vsdf!
- Available templates: `default` (colorful animation), `plot` (2D function plotter)

## Example: save a ShaderToy and run it locally
1. Open the ShaderToy and copy the fragment shader code:
   https://www.shadertoy.com/view/Xds3zN
2. Save it as `shaders/raymarching_primitives.frag`.
3. Run it with the template prepended:
```sh
vsdf --toy shaders/raymarching_primitives.frag
```

## Recommendations
I would recommend visiting https://www.shadertoy.com to see their amazing shaders.
Also check out https://iquilezles.org/
