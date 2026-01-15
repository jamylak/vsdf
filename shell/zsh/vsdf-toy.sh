# VSDF instant shader function for Zsh
# Add this to your ~/.zshrc

vsdf-toy() {
    local shader=$(vsdf --new-toy "$@")
    if [ -z "$shader" ]; then
        echo "Failed to create shader"
        return 1
    fi
    
    # Launch vsdf in background, suppress output, minimal logging, no focus steal
    vsdf --no-focus --log-level error "$shader" > /dev/null 2>&1 & disown
    
    # Open in your favorite editor (uncomment your choice):
    # For GUI editors, use & to background:
    # code "$shader" &          # VS Code
    # subl "$shader" &          # Sublime Text
    # atom "$shader" &          # Atom
    
    # For terminal editors, DON'T background - edit in foreground:
    # nvim "$shader"            # Neovim (foreground)
    # vim "$shader"             # Vim (foreground)
    
    echo "vsdf is running in background for $shader"
    echo "Edit the file to see live updates, close vsdf window when done"
}
