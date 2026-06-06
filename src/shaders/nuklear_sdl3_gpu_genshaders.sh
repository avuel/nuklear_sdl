# very raw prototype of shader generation for nuklear_sdl3_gpu demo
# I just wanted to have something running
# https://github.com/Immediate-Mode-UI/Nuklear/issues/726#issuecomment-3765429104

set -e
#set -x

progname="$(basename "$0")"

cd "$(dirname "$0")"  # always in same directory

if ! command -v shadercross >/dev/null 2>/dev/null; then
	echo "${progname} could not find: shadercross"
	exit 1
fi
# FIXME: widely available but not POSIX... any alternative?
if ! xxd -v >/dev/null 2>/dev/null; then
	echo "${progname} could not find: xxd"
	exit 1
fi

generating() {
	printf "%s: '%s' ...\n" "${progname}" "${1}" 2>/dev/stderr;
}

name=nuklear_sdl3_gpu
#input=./nuklear_d3d11.hlsl
#input=../../vendor/nuklear/demo/d3d11/nuklear_d3d11.hlsl

tmpdir="$(mktemp -d)"
trap 'rm -rfv "${tmpdir}"' EXIT INT QUIT TERM

generating "${tmpdir}/${name}_VERTEX.hlsl";   sed "./vertex.hlsl"  -e 's/vs_main(/VERTEX(/'   >${tmpdir}/${name}_VERTEX.hlsl
generating "${tmpdir}/${name}_FRAGMENT.hlsl"; sed "./pixel.hlsl"   -e 's/ps_main(/FRAGMENT(/' >${tmpdir}/${name}_FRAGMENT.hlsl

for dest in DXBC DXIL SPIRV MSL; do
	for stage in VERTEX FRAGMENT; do
		sym="${tmpdir}/${name}_${stage}_${dest}"

		generating "${sym}"
		shadercross       "${tmpdir}/${name}_${stage}.hlsl" \
		    --source      HLSL                      \
		    --dest        "${dest}"                 \
		    --stage       "${stage}"                \
		    --entrypoint  "${stage}"                \
		    --output      "${sym}"                  \
		    --debug                                 \
		    ;

		# FIXME: dx11/12 demos use .h files, not .c, so maybe I should use it too?
		generating "${sym}.c.in"
		xxd -i -n "__${name}_${stage}_${dest}" "${sym}" >"${sym}.c.in"
	done
done

data="${tmpdir}/${name}_data.c"
generating "${data}"
{
	echo "/*      !!! THIS FILE WAS GENERATED AUTOMATICALLY !!!      */"
	echo "/* Do not edit by hand, unless you know what you're doing! */"
	echo ""

	# every format supported by SDL3
	for dest in PRIVATE SPIRV DXBC DXIL MSL METALLIB; do
		for stage in VERTEX FRAGMENT; do
			input="${tmpdir}/${name}_${stage}_${dest}.c.in"
			if test -f "$input"; then
				cat <"$input"
			else
				# this means that the format is unsupported (__*_len == 0)
				# still use an empty stub so it's easier to deal with this from C code
				echo "unsigned char __${name}_${stage}_${dest}[]    = {0x00};"
				echo "unsigned int  __${name}_${stage}_${dest}_len  = 0;"
			fi
		done
		echo ""
	done
} >"${data}"

mv -fv "${data}" "./$(basename "${data}")"

