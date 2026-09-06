# Authored Bash3.2 helpers; never source generated manifests or evaluate metadata as shell code.

# Close inherited non-stdio descriptors using literal Bash descriptor movement, then exec.
# /dev/fd enumeration observes inheritable descriptors even above a lowered soft fd limit.
# A producer completion marker proves ls finished; Bash3.2 cannot reliably wait for process substitution.
# No shell job PID is used for signaling or ownership.
style_close_descriptors() {
    local descriptor path count=0 complete=0
    exec 3>&-
    exec 9< <(if CLICOLOR=0 CLICOLOR_FORCE=0 /bin/ls -1 /dev/fd; then
        printf 'FERN_STYLE_FD_END:0\n'
    else
        printf 'FERN_STYLE_FD_END:1\n'
    fi)
    while IFS= read -r descriptor <&9; do
        if [[ $descriptor == FERN_STYLE_FD_END:0 && $complete == 0 ]]; then
            complete=1
            continue
        fi
        count=$((count + 1))
        if [[ $complete != 0 ]] || ((count > 65536)) || [[ -z $descriptor || $descriptor == *[!0-9]* ]]; then
            exec 9<&-
            return 125
        fi
        [[ $descriptor == 0 || $descriptor == 1 || $descriptor == 2 || $descriptor == 9 ]] && continue
        path=/dev/fd/$descriptor
        if [[ -e $path ]]; then
            exec 3>&"$descriptor"- || return 125
            exec 3>&-
        fi
    done
    exec 9<&-
    [[ $complete == 1 ]] || return 125
}

# Run a literal executable after closing inherited non-stdio descriptors.
style_clean_exec() {
    style_close_descriptors || return 125
    exec "$@"
}
