# Authored finite Clang profile: opaque config, plugin, response and tool-loading flags are excluded.

# Only the compiler-owned empty regular config is accepted; older Clang skips implicit search
# after explicit config selection, while newer Clang also needs its dedicated environment switch.
style_driver_run() {
    local config=$1 mode
    shift
    [[ -f $config && -O $config && ! -L $config && ! -s $config ]] || return 125
    case $OSTYPE in
        darwin*) mode=$(/usr/bin/stat -f '%Lp' "$config") || return 125 ;;
        linux*) mode=$(/usr/bin/stat -c '%a' "$config") || return 125 ;;
        *) return 125 ;;
    esac
    [[ $mode == 400 ]] || return 125
    CLANG_NO_DEFAULT_CONFIG=1 "$@" --config "$config"
}

# Reject Clang's argv-rewriting escape hatch before any cache lookup or trusted seed compilation.
style_driver_environment() {
    if [[ -n ${CCC_OVERRIDE_OPTIONS-} ]]; then
        printf 'fern style: unsupported Clang bootstrap environment: CCC_OVERRIDE_OPTIONS\n' >&2
        return 125
    fi
}

# Validate decoded project/pkg-config flag words before invoking the private compiler.
# Path and macro operands remain literal; no shell expansion or driver forwarding is accepted.
style_driver_flags() {
    local flag pending=
    for flag in "$@"; do
        if [[ -n $pending ]]; then
            [[ -n $flag && $flag != -* && $flag != @* ]] || {
                printf 'fern style: unsupported Clang bootstrap flag: %s\n' "$flag" >&2
                return 125
            }
            pending=
            continue
        fi
        case $flag in
            -I|-L|-D|-U|-isystem|-iquote|-isysroot|-framework|-target|--sysroot) pending=$flag ;;
            -I?*|-L?*|-D?*|-U?*|-l?*|-std=c89|-std=c90|-std=c99|-std=c11|-std=c17|-std=gnu99|-std=gnu11|-std=gnu17) ;;
            -O0|-O1|-O2|-O3|-Os|-Oz|-Og|-g|-g0|-g1|-g2|-g3|-gdwarf-4|-gdwarf-5) ;;
            -Wall|-Wextra|-Wpedantic|-Werror|-Werror=*|-Wno-*|-pedantic|-pedantic-errors) ;;
            -pthread|-fPIC|-fpic|-fPIE|-fpie|-fno-common|-fno-omit-frame-pointer) ;;
            -mmacosx-version-min=*|--sysroot=*|--target=*) ;;
            *) printf 'fern style: unsupported Clang bootstrap flag: %s\n' "$flag" >&2; return 125 ;;
        esac
    done
    if [[ -n $pending ]]; then
        printf 'fern style: unsupported Clang bootstrap flag: missing operand for %s\n' "$pending" >&2
        return 125
    fi
}
