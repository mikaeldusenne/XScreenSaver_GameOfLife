_game_of_life_complete() {
    local current=${COMP_WORDS[COMP_CWORD]} previous=${COMP_WORDS[COMP_CWORD-1]}
    if [[ ${COMP_WORDS[1]} == game || ${COMP_WORDS[1]} == serve ]]; then
        if [[ $previous == --replay || $previous == --replay-dir || $previous == --web-root ]]; then
            COMPREPLY=( $(compgen -f -- "$current") )
        else
            COMPREPLY=( $(compgen -W '--bind --port --web-root --replay-dir --no-replays --replay --planning-seconds --help' -- "$current") )
        fi
        return
    fi
    if [[ $previous == figure || $previous == --figure ]]; then
        COMPREPLY=( $(compgen -W "$(game-of-life figures | awk '{print $1}')" -- "$current") )
    elif [[ $previous == --lang ]]; then
        COMPREPLY=( $(compgen -W 'en fr' -- "$current") )
    elif [[ $previous == --history ]]; then
        COMPREPLY=( $(compgen -f -- "$current") )
    elif (( COMP_CWORD == 1 )); then
        COMPREPLY=( $(compgen -W 'preview fancy figure mix random wallpaper screensaver replay figures doctor version help game serve' -- "$current") )
    else
        COMPREPLY=( $(compgen -W '--gpu --cpu --fancy --classic --square-cells --fps --size --random-size --trail --seconds --seed --fullscreen --idle --lang --history --no-history --replay --help' -- "$current") )
    fi
}
complete -F _game_of_life_complete game-of-life
