_game_of_life_complete() {
    local current=${COMP_WORDS[COMP_CWORD]} previous=${COMP_WORDS[COMP_CWORD-1]}
    if [[ $previous == figure || $previous == --figure ]]; then
        COMPREPLY=( $(compgen -W "$(game-of-life figures | awk '{print $1}')" -- "$current") )
    elif [[ $previous == --lang ]]; then
        COMPREPLY=( $(compgen -W 'en fr' -- "$current") )
    elif (( COMP_CWORD == 1 )); then
        COMPREPLY=( $(compgen -W 'preview figure mix random wallpaper screensaver figures doctor version help' -- "$current") )
    else
        COMPREPLY=( $(compgen -W '--fps --size --trail --seconds --seed --fullscreen --idle --lang --help' -- "$current") )
    fi
}
complete -F _game_of_life_complete game-of-life
