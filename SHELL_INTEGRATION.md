# Shell Integration for VSDF

Make shader development **instant** with shell functions that create a new shader, open it in your editor, and launch vsdf - all in one command.

The `--no-focus` flag prevents vsdf from stealing window focus, so your editor stays in focus while vsdf runs silently in the background.

## Quick Setup

### Bash

Add to `~/.bashrc` or `~/.bash_profile`:

```bash
# VSDF instant shader function
vsdf-toy() {
    local shader=$(vsdf --new-toy "$@")
    if [ -z "$shader" ]; then
        echo "Failed to create shader"
        return 1
    fi
    
    # Launch vsdf in background, redirect output
    vsdf --no-focus --log-level error "$shader" > /dev/null 2>&1 & disown
    
    # Open in editor (choose one):
    code "$shader" &          # VS Code (GUI, backgrounds)
    # nvim "$shader"           # Neovim (terminal, foreground)
    # vim "$shader"            # Vim (terminal, foreground)
    # subl "$shader" &         # Sublime Text (GUI, backgrounds)
    
    echo "vsdf running in background for $shader"
}
```

**One-liner to add it:**
```bash
curl -fsSL https://raw.githubusercontent.com/jamylak/vsdf/main/shell/bash/vsdf-toy.sh >> ~/.bashrc && source ~/.bashrc
```

Or manually:
```bash
cat >> ~/.bashrc << 'EOF'
vsdf-toy() {
    local shader=$(vsdf --new-toy "$@")
    [ -z "$shader" ] && return 1
    vsdf --no-focus --log-level error "$shader" > /dev/null 2>&1 & disown
    code "$shader" &
    echo "vsdf running in background for $shader"
}
EOF
source ~/.bashrc
```

### Fish

Add to `~/.config/fish/functions/vsdf-toy.fish`:

```fish
function vsdf-toy
    set shader (vsdf --new-toy $argv)
    if test -z "$shader"
        echo "Failed to create shader"
        return 1
    end
    
    # Open in your favorite editor (choose one):
    code $shader &          # VS Code
    # nvim $shader &         # Neovim
    # vim $shader &          # Vim
    # subl $shader &         # Sublime Text
    
    # Launch vsdf in background
    vsdf $shader &
    
    echo "Editing $shader with vsdf running"
end
```

**One-liner to add it:**
```fish
mkdir -p ~/.config/fish/functions && curl -fsSL https://raw.githubusercontent.com/jamylak/vsdf/main/shell/fish/vsdf-toy.fish > ~/.config/fish/functions/vsdf-toy.fish
```

Or manually:
```fish
mkdir -p ~/.config/fish/functions
cat > ~/.config/fish/functions/vsdf-toy.fish << 'EOF'
function vsdf-toy
    set shader (vsdf --new-toy $argv)
    test -z "$shader" && return 1
    vsdf --no-focus --log-level error $shader > /dev/null 2>&1 & disown
    code $shader &
    echo "vsdf running in background for $shader"
end
EOF
```

### Zsh

Add to `~/.zshrc`:

```zsh
# VSDF instant shader function
vsdf-toy() {
    local shader=$(vsdf --new-toy "$@")
    if [ -z "$shader" ]; then
        echo "Failed to create shader"
        return 1
    fi
    
    # Launch vsdf in background, redirect output
    vsdf --no-focus --log-level error "$shader" > /dev/null 2>&1 & disown
    
    # Open in editor (choose one):
    code "$shader" &          # VS Code (GUI, backgrounds)
    # nvim "$shader"           # Neovim (terminal, foreground)
    # vim "$shader"            # Vim (terminal, foreground)
    # subl "$shader" &         # Sublime Text (GUI, backgrounds)
    
    echo "vsdf running in background for $shader"
}
```

**One-liner to add it:**
```zsh
curl -fsSL https://raw.githubusercontent.com/jamylak/vsdf/main/shell/zsh/vsdf-toy.sh >> ~/.zshrc && source ~/.zshrc
```

## Usage

### Basic usage (random name)
```bash
vsdf-toy
# Creates my_new_toy_12345.frag
# Opens in editor
# Launches vsdf
```

### Custom name
```bash
vsdf-toy cool_effect
# Creates cool_effect.frag
```

### With template
```bash
vsdf-toy plot_demo --template plot
# Creates plot_demo.frag with 2D plotting template
```

## Editor-Specific Examples

### Neovim (Terminal Editor)
```bash
vsdf-toy() {
    local shader=$(vsdf --new-toy "$@")
    [ -z "$shader" ] && return 1
    
    # Launch vsdf in background (no output)
    vsdf --no-focus --log-level error "$shader" > /dev/null 2>&1 & disown
    
    # Edit in foreground
    nvim "$shader"
}
```

### VS Code (GUI Editor)
```bash
vsdf-toy() {
    local shader=$(vsdf --new-toy "$@")
    [ -z "$shader" ] && return 1
    
    # Launch vsdf first (redirected)
    vsdf --no-focus --log-level error "$shader" > /dev/null 2>&1 & disown
    
    # Open VS Code (can add -n for new window)
    code "$shader" &
}
```

## Available Templates

- `default` - Colorful animated starter (default)
- `plot` - 2D function plotter with grid and axes

Use with `--template`:
```bash
vsdf-toy my_function --template plot
```

## NOTES

- This is a convenient way of launching but it will hide error messages
