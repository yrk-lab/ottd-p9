# shared settings (sourced)
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd); W=${W:-$HERE/work}
export PATH=/usr/lib/llvm-17/bin:$PATH
CBE=${CBE:-$W/llvm-cbe/build/tools/llvm-cbe/llvm-cbe}
TRIPLE=${TRIPLE:-x86_64-unknown-linux-gnu}      # Phase 2+: swap for the Plan 9-ish triple
OUT=${OUT:-$HERE/out}
# Language mode that the real OpenTTD build will use. -fno-exceptions makes any try/catch a hard error,
# because cbe's LowerInvokePass would otherwise silently drop exception handling.
CXXFLAGS="--target=$TRIPLE -std=c++20 -stdlib=libc++ -fno-exceptions -fno-rtti -O1 -g0 -D_LIBCPP_DISABLE_EXTERN_TEMPLATE"
LIBCXX_SRCS="string hash charconv algorithm memory ryu/d2fixed ryu/d2s ryu/f2s"
