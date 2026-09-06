# Shell-owned staging cleanup before native handoff; no child/job PID is ever used.

# Delete only retained run links created by this invocation, preserving unexpected directory contents.
style_run_remove() {
    [[ -n ${style_run-} ]] || return 0
    style_private "$style_run" || return 125
    [[ $(style_identity "$style_run") == "$style_run_identity" ]] || return 125
    /bin/rm -f -- "$style_run/program" "$style_run/supervisor" "$style_run/owner" || return 125
    /bin/rmdir -- "$style_run" || return 125
    style_run=
}

# Verify original control identity and marker before recursively deleting this invocation's payload.
style_control_remove() {
    [[ -n ${style_control-} ]] || return 0
    style_private "$style_control" || return 125
    [[ $(style_identity "$style_control") == "$style_control_identity" ]] || return 125
    style_small_file "$style_control/owner" 64 || return 125
    [[ $(/bin/cat "$style_control/owner") == FERN_STYLE_WORK_V1 ]] || return 125
    /bin/rm -rf -- "$style_control" || return 125
    style_control=
}

# Preserve the initial exit status even if stderr or secondary owned cleanup fails.
style_entry_exit() {
    local status=$?
    trap - EXIT
    local cleanup=0
    style_run_remove || cleanup=125
    style_control_remove || cleanup=125
    if [[ $cleanup != 0 ]]; then
        printf 'fern style: owned bootstrap cleanup failed\n' >&2 || :
        [[ $status != 0 ]] || status=125
    fi
    exit "$status"
}

# Worker cleanup owns only the payload directory; the native controller owns logs and handoff.
style_worker_exit() {
    local status=$?
    trap - EXIT
    if [[ $style_work != "$style_control/payload" || -L $style_work || \
          $(style_identity "$style_work") != "$style_payload_identity" ]]; then
        printf 'fern style: worker cleanup identity changed\n' >&2 || :
        [[ $status != 0 ]] || status=125
    else
        if ! cd -P -- "$style_root"; then [[ $status != 0 ]] || status=125; fi
        if ! /bin/rm -rf -- "$style_work"; then [[ $status != 0 ]] || status=125; fi
    fi
    if [[ $status != 0 ]]; then style_run_remove || :; fi
    exit "$status"
}
