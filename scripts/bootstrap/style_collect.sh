# Convert compiler-produced dependency records into bounded, literal cache manifests.

# Map selected snapshot inputs back to the checkout; derived build artifacts need no external hash.
style_external() {
    local path=$1
    [[ $path == /* ]] || path=$style_snapshot/$path
    style_path "$path" || return 125
    if [[ $path == "$style_snapshot"/* ]]; then
        [[ -f $path ]] || return 125
        return 0
    fi
    [[ -f $path ]] || return 125
    path=$(style_absolute "$path") || return 125
    printf '%s\n' "$path" >> "$style_work/external.paths" || return 125
}

# Include traces retain literal POSIX backslashes that Clang's Make output can normalize away.
style_header_dependencies() {
    local file path normalized count=0
    : > "$style_work/include.normalized" || return 125
    for file in "$style_snapshot"/build/*.include.paths; do
        style_small_file "$file" 1048576 || return 125
        while IFS= read -r -d '' path; do
            count=$((count + 1)); ((count <= 65536)) || return 125
            [[ $path == /* ]] || path=$style_snapshot/$path
            normalized=${path//\\//}
            printf '%s\n' "$normalized" >> "$style_work/include.normalized" || return 125
            style_external "$path" || return 125
        done < "$file"
    done
}

# Make dependencies remain an independent inventory, including forced includes and source inputs.
style_make_dependencies() {
    local file path count=0
    for file in "$style_snapshot"/build/*.dep.paths; do
        style_small_file "$file" 1048576 || return 125
        while IFS= read -r -d '' path; do
            count=$((count + 1)); ((count <= 65536)) || return 125
            [[ $path == /* ]] || path=$style_snapshot/$path
            if [[ ! -f $path ]]; then
                /usr/bin/grep -Fqx -- "$path" "$style_work/include.normalized" || return 125
                continue
            fi
            style_external "$path" || return 125
        done < "$file"
    done
}

# Record complete linker inputs; archive member suffixes are stripped only when the full path is absent.
style_link_dependencies() {
    local file path parent count=0
    for file in "$style_snapshot"/bin/*.link.paths; do
        style_small_file "$file" 1048576 || return 125
        while IFS= read -r path; do
            count=$((count + 1)); ((count <= 16384)) || return 125
            [[ $path == /* ]] || path=$style_snapshot/$path
            if [[ ! -f $path && $path == *')' ]]; then path=${path%\(*}; fi
            style_external "$path" || return 125
            if [[ $path != "$style_snapshot"/* ]]; then
                parent=${path%/*}
                style_search_root "$parent" || return 125
            fi
        done < "$file"
    done
}

# Store a logical missing root or a canonical existing directory, avoiding invocation-cwd drift.
style_search_root() {
    local path=$1
    [[ $path == /* ]] || path=$style_snapshot/$path
    style_path "$path" || return 125
    if [[ $path == "$style_snapshot"/* ]]; then
        path=$style_root/${path#"$style_snapshot"/}
    fi
    if [[ -d $path ]]; then path=$(cd -P -- "$path" && pwd) || return 125; fi
    printf '%s\n' "$path" >> "$style_work/search-roots" || return 125
}

# Decode one owned compiler flags line into literal words, never executable shell syntax.
style_collect_words() {
    local text=$1 word
    printf '%s\n' "$text" > "$style_work/flags.input" || return 125
    "$style_metadata" flags < "$style_work/flags.input" > "$style_work/flags.words" || return 125
    style_words=()
    while IFS= read -r -d '' word; do style_words+=("$word"); done < "$style_work/flags.words"
}

# Parse Clang's bounded verbose include-search section, including currently nonexistent roots.
style_compiler_search() {
    local line active=0 found=0 path
    style_small_file "$style_snapshot/build/search.stderr" 1048576 || return 125
    while IFS= read -r line; do
        if [[ $line == '#include '* ]]; then active=1; continue; fi
        if [[ $line == 'End of search list.' ]]; then active=0; found=1; continue; fi
        if [[ $active == 1 ]]; then
            [[ $line == ' '* ]] || return 125
            path=${line# }; path=${path%' (framework directory)'}
            style_search_root "$path" || return 125
        elif [[ $line == 'ignoring nonexistent directory '* ]]; then
            style_collect_words "${line#ignoring nonexistent directory }" || return 125
            [[ ${#style_words[@]} == 1 ]] || return 125
            style_search_root "${style_words[0]}" || return 125
        elif [[ $line == ' "'* ]]; then
            style_collect_words "$line" || return 125
            (( ${#style_words[@]} > 0 )) || return 125
            style_external "${style_words[0]}" || return 125
            style_compiler_image=${style_words[0]}
        fi
    done < "$style_snapshot/build/search.stderr"
    [[ $found == 1 ]]
}

# Include explicit -I/-L/-isystem roots from all effective build and pkg-config flag records.
style_flag_search() {
    local file word wanted=0
    for file in "$style_snapshot"/build/config.* "$style_snapshot"/build/pkg.*; do
        style_small_file "$file" 1048576 || return 125
        "$style_metadata" flags < "$file" > "$style_work/flag.paths" || return 125
        wanted=0
        while IFS= read -r -d '' word; do
            if [[ $wanted == 1 ]]; then style_search_root "$word" || return 125; wanted=0
            elif [[ $word == -I || $word == -L || $word == -isystem || $word == -iquote ]]; then wanted=1
            elif [[ $word == -I?* || $word == -L?* ]]; then style_search_root "${word:2}" || return 125
            fi
        done < "$style_work/flag.paths"
        [[ $wanted == 0 ]] || return 125
    done
    local rest part
    while IFS= read -r word; do
        [[ $word == 'libraries: ='* ]] || continue
        rest=${word#libraries: =}
        while [[ -n $rest ]]; do
            part=${rest%%:*}
            if [[ $rest == *:* ]]; then rest=${rest#*:}; else rest=; fi
            [[ -n $part ]] && style_search_root "$part" || return 125
        done
    done < "$style_snapshot/build/compiler.search-dirs"
}

# Record ELF loader-selected dependencies for the actual tools and new binaries on Linux.
# ldd is an installed trusted tool; its output is bounded literal metadata, never shell source.
style_elf_dependencies() {
    [[ $style_platform == Linux ]] || return 0
    local file line path magic count=0
    local binaries=("${style_tools[@]}" "${style_compiler_image-$style_cc}"
        "$style_snapshot/bin/check_style" "$style_snapshot/bin/supervisor" "$style_snapshot/bin/metadata")
    for file in "${binaries[@]}"; do
        magic=$(/usr/bin/head -c 4 "$file") || return 125
        [[ $magic == $'\177ELF' ]] || continue
        "$style_ldd" "$file" > "$style_work/loader.paths" 2> "$style_work/loader.stderr" || return 125
        style_small_file "$style_work/loader.paths" 1048576 || return 125
        while IFS= read -r line; do
            count=$((count + 1)); ((count <= 16384)) || return 125
            line=${line#$'\t'}
            if [[ $line == linux-vdso.so.* || $line == 'statically linked' ]]; then continue; fi
            if [[ $line == *' => '* ]]; then line=${line#* => }; fi
            path=${line%' ('*}
            [[ $path == /* && -f $path ]] || return 125
            style_external "$path" || return 125
            style_search_root "${path%/*}" || return 125
        done < "$style_work/loader.paths"
    done
    for file in /etc/ld.so.cache /etc/ld.so.conf; do
        [[ ! -f $file ]] || style_external "$file" || return 125
    done
    if [[ -d /etc/ld.so.conf.d ]]; then
        style_search_root /etc/ld.so.conf.d || return 125
        for file in /etc/ld.so.conf.d/*; do
            [[ ! -f $file ]] || style_external "$file" || return 125
        done
    fi
}

# Collect a sorted bounded inventory before hashing; source snapshots are already covered by base.
style_collect() {
    local source=$1 path count=0
    style_snapshot=$(cd -P -- "$source" && pwd) || return 125
    : > "$style_work/external.paths" || return 125
    : > "$style_work/search-roots" || return 125
    style_header_dependencies || return 125
    style_make_dependencies || return 125
    style_link_dependencies || return 125
    style_compiler_search || return 125
    style_flag_search || return 125
    style_elf_dependencies || return 125
    "$style_sort" -u -o "$style_work/external.paths" "$style_work/external.paths" || return 125
    "$style_sort" -u -o "$style_work/search-roots" "$style_work/search-roots" || return 125
    style_paths=()
    while IFS= read -r path; do
        count=$((count + 1)); ((count <= 16384)) || return 125
        style_paths+=("$path")
    done < "$style_work/external.paths"
    style_hash_files "$style_work/dependencies" "${style_paths[@]}" || return 125
    style_search_inventory "$style_work/search-roots" "$style_work/search" || return 125
}
