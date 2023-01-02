#!/bin/zsh

# builds a variant of lit that can be used with ltrace.
# REQUIRES https://gitlab.com/cespedes/ltrace because version 0.7.3
# does not work. for some reason.
# it's not limited to lit, it just generally doesn't work. don't ask me, though.
#
# TODO: maybe stop being retarded and switch to cmake instead of cranking out
# such terrible monstrosities.

# unlike env.*.sh, don't source this
[[ $ZSH_EVAL_CONTEXT =~ :file ]] && return

srclib=(
  api.c
  ccast.c
  ccemit.c
  ccopt.c
  ccparser.c
  ccprepr.c
  ccscan.c
  chunk.c
  debug.c
  error.c
  gcmem.c
  libarray.c
  libclass.c
  libcore.c
  libfiber.c
  libfs.c
  libfunc.c
  libmap.c
  libmath.c
  libmodule.c
  libobject.c
  librange.c
  libstring.c
  state.c
  util.c
  value.c
  vm.c
  writer.c
)

srcexe=(
  main.c
)

libname="liblit.so"
exename="run.traced"

linkflags=(
  -lm
  -lreadline
)

ldmagicshit=(
  -Wl,-z,relro -Wl,-z,now
)

function verbose {
  echo "$@" >&2
  eval "$@"
}

verbose mkdir -p _tmp
for file in "${srclib[@]}"; do
  ofile="_tmp/${file%.*}.o"
  rm -fv "$ofile"
  if ! verbose gcc -no-pie -fPIC -c "$file" -o "$ofile" "${ldmagicshit[@]}"; then
    exit
  fi
done
verbose gcc -no-pie -shared _tmp/*.o -o "${libname}" "${ldmagicshit[@]}"
verbose gcc -no-pie "${srcexe[@]}" -Wl,-rpath=. liblit.so -o "${exename}" "${linkflags[@]}" "${ldmagicshit[@]}"
verbose rm -rf _tmp
echo "**le fin**"
