#!/usr/bin/env bash

if [[ "$(uname -s)" == "Darwin" ]]; then
  _preload=DYLD_INSERT_LIBRARIES
  _lib_ext="dylib"
  _os="macos"
  _dsymutil() {
    _echo dsymutil "${@}"
  }
else
  _preload=LD_PRELOAD
  _lib_ext="so"
  _os="linux"
  _dsymutil() {
    return 0
  }
fi

# Print out the name of a command before running it
_echo() {
    echo "${@}"
    "${@}"
}

# Get the basename of a path
_basename() {
    # Usage: basename "path" ["suffix"]
    local tmp

    tmp="${1%"${1##*[!/]}"}"
    tmp="${tmp##*/}"
    tmp="${tmp%"${2/"$tmp"}"}"

    printf '%s' "${tmp:-/}"
}

mp_runtime="build/libmp_runtime.${_lib_ext}"

_dsymutil "${mp_runtime}"

for file in examples/build/*; do
    if [[ -f $file && -x $file ]]; then
        example_name="$(_basename "${file}")"
        _dsymutil "$file"
        _echo env "${_preload}=${mp_runtime}" \
            MEM_PROFILE_OUT=etc/test_files/${_os}/${example_name}.json \
            "${file}"
    fi
done
