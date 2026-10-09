# Owner: Time And Time Studio
# Date: 2026-10-09 08:50 +0700
# License: GPL-3.0-or-later
#
# Shared reader for <repo>/tma.conf. Sourced by build.sh and scripts/*.sh so
# that the application name is resolved in exactly one place instead of being
# re-derived in each script.
#
# Syntax the parser understands is deliberately tiny: flat `key = value`, one
# per line, `#` at the start of a line is a comment, and the value runs verbatim
# to the end of the line (so a value must not contain `#`).

_tma_conf_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMA_CONF="${TMA_CONF:-$_tma_conf_dir/tma.conf}"

# tma_conf_get <key>
#
# Prints the last assignment of <key>, with surrounding whitespace removed.
# Exits 1 if the key is absent, so callers that want a default should use
# `$(tma_conf_get key) || true` or supply their own fallback.
tma_conf_get() {
  local key="$1" raw
  [[ -f "$TMA_CONF" ]] || return 1
  raw="$(grep -E "^[[:space:]]*${key}[[:space:]]*=" "$TMA_CONF" | tail -n1)"
  # Absent key: grep produced no line. A present key with an empty value does
  # reach here and is returned as an empty string on success.
  [[ -n "$raw" ]] || return 1
  raw="${raw#*=}"
  raw="${raw#"${raw%%[![:space:]]*}"}"
  raw="${raw%"${raw##*[![:space:]]}"}"
  printf '%s\n' "$raw"
}

# tma_conf_name
#
# app_name, or "tma" when the key is missing or empty. Returns the value as
# written rather than checking it, because the caller that cares about a well
# formed name is setup_chromium.sh, which dies loudly; silently substituting a
# different name here would make run/package look for a binary that was never
# built.
tma_conf_name() {
  local name
  name="$(tma_conf_get app_name 2>/dev/null)" || name=""
  if [[ -z "$name" ]]; then
    name="tma"
  fi
  printf '%s\n' "$name"
}

# tma_conf_valid_name <value>
#
# True when <value> is safe to use as a file name, as a GN string, and spliced
# into the percent-encoded fallback page: ASCII, starts with a letter or digit,
# then only letters, digits, '.', '_' and '-'. Also a legal reverse-DNS id, so
# app_id is checked with the same pattern.
tma_conf_valid_name() {
  [[ "$1" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]]
}

# tma_conf_gn_escape <value>
#
# Escapes a value for the inside of a GN string literal, which is what
# setup_chromium.sh needs when it turns the file into GN arguments.
tma_conf_gn_escape() {
  local v="$1"
  v="${v//\\/\\\\}"
  v="${v//\"/\\\"}"
  printf '%s' "$v"
}
