# VSDF instant shader function for Fish
# Save this to ~/.config/fish/functions/vsdf-toy.fish

function vsdf-toy
    set shader (vsdf --new-toy $argv)
    if test -z "$shader"
        echo "Failed to create shader"
        return 1
    end

    # Launch vsdf in background, suppress output, minimal logging, no focus steal
    vsdf --no-focus --log-level error $shader >/dev/null 2>&1 & disown

    # Open in your favorite editor (uncomment your choice):
    # For GUI editors, use & to background:
    # code $shader &          # VS Code

    # For terminal editors, DON'T background - edit in foreground:
    # nvim $shader             # Neovim (foreground)
    # vim $shader            # Vim (foreground)

    # For GUI editors that can background:
    # subl $shader &         # Sublime Text

    echo "vsdf is running in background for $shader"
end
