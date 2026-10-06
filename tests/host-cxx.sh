# Shared compiler settings for host-side tests (sourced by tests/*.sh).
# One compiler, one flag set: warnings are errors everywhere.
HOST_CXX="${HOST_CXX:-clang++}"
HOST_CC="${HOST_CC:-clang}"
host_cxx(){ "$HOST_CXX" -std=c++23 -O1 -Wall -Wextra -Werror -I"$ROOT" -I"$ROOT/include" -I"$ROOT/kernel" "$@"; }
host_cc(){ "$HOST_CC" -std=c11 -O1 -Wall -Wextra -Werror -I"$ROOT/include" -I"$ROOT/bootctl/common" "$@"; }
