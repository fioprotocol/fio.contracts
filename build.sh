#!/usr/bin/env bash
#set -x

function usage() {
   printf "Usage: $0 OPTION...
  -f DIR      FIO Install Directory (FIO binary, dependencies). Default: $HOME/fio
   \\n" "$0" 1>&2
   exit 1
}

TIME_BEGIN=$(date -u +%s)
if [ $# -ne 0 ]; then
   while getopts "f:hv" opt; do
      case "${opt}" in
      f)
         FIO_INSTALL_DIR=$OPTARG
         ;;
      h)
         usage
         ;;
      v)
         VERBOSE=true
         ;;
      ?)
         echo "Invalid Option!" 1>&2
         usage
         ;;
      :)
         echo "Invalid Option: -${OPTARG} requires an argument." 1>&2
         usage
         ;;
      *)
         usage
         ;;
      esac
   done
fi

SCRIPT_VERSION=2.10
export CURRENT_WORKING_DIR=$(pwd) # relative path support

# Obtain dependency versions; Must come first in the script
. ./.environment

# Load general helpers
. ./utils.sh

echo
echo "FIO Contracts Build Script Version: ${SCRIPT_VERSION}"
echo "FIO Contracts Version: ${FIO_CNTRX_VERSION_FULL}"
echo "$(date -u)"
echo "User: ${CURRENT_USER}"
# echo "git head id: %s" "$( cat .git/refs/heads/master )"
echo "Current branch: $(execute git rev-parse --abbrev-ref HEAD 2>/dev/null)"

# Checks for Arch and OS + Support for tests setting them manually
## Necessary for linux exclusion while running bats tests/bash-bats/*.sh
[[ -z "${ARCH}" ]] && export ARCH=$(uname)
if [[ -z "${NAME}" ]]; then
    if [[ $ARCH == "Linux" ]]; then
        [[ ! -e /etc/os-release ]] && echo "${COLOR_RED} - /etc/os-release not found! It seems you're attempting to use an unsupported Linux distribution.${COLOR_NC}" && exit 1
        # Obtain OS NAME, and VERSION
        . /etc/os-release
    elif [[ $ARCH == "Darwin" ]]; then
        export NAME=$(sw_vers -productName)
    else
        echo " ${COLOR_RED}- FIO is not supported for your Architecture!${COLOR_NC}" && exit 1
    fi
    set-system-vars
fi

echo
echo "Performing OS/System Validation..."
([[ $NAME == "Ubuntu" ]] && ([[ "$(echo ${VERSION_ID})" == "18.04" ]] || [[ "$(echo ${VERSION_ID})" == "20.04" ]] || [[ "$(echo ${VERSION_ID})" == "22.04" ]])) || (echo " - You must be running 18.04.x or 20.04.x to install EOSIO." && exit 1)

# Set up the working directories for build, etc
setup

# CMAKE Installation
export CMAKE=
([[ -z "${CMAKE}" ]] && [[ -d $FIO_INSTALL_DIR ]] && [[ -x $FIO_INSTALL_DIR/bin/cmake ]]) && export CMAKE=$FIO_INSTALL_DIR/bin/cmake
([[ -z "${CMAKE}" ]] && [[ -d $FIO_CNTRX_APTS_DIR ]] && [[ -x $FIO_CNTRX_APTS_DIR/bin/cmake ]]) && export CMAKE=$FIO_CNTRX_APTS_DIR/bin/cmake
if [[ $ARCH == "Darwin" ]]; then
   ([[ -z "${CMAKE}" ]] && [[ ! -z $(command -v cmake 2>/dev/null) ]]) && export CMAKE=$(command -v cmake 2>/dev/null) && export CMAKE_CURRENT_VERSION=$($CMAKE --version | grep -E "cmake version[[:blank:]]*" | sed 's/.*cmake version //g')

   # If it exists, check that it's > required version +
   if [[ ! -z $CMAKE_CURRENT_VERSION ]] && [[ $((10#$(echo $CMAKE_CURRENT_VERSION | awk -F. '{ printf("%03d%03d%03d\n", $1,$2,$3); }'))) -lt $((10#$(echo $CMAKE_REQUIRED_VERSION | awk -F. '{ printf("%03d%03d%03d\n", $1,$2,$3); }'))) ]]; then
      echo "${COLOR_RED}The currently installed cmake version ($CMAKE_CURRENT_VERSION) is less than the required version ($CMAKE_REQUIRED_VERSION). Cannot proceed."
      exit 1
   fi
fi
ensure-cmake

echo
printf "\t=========== Building FIO Contracts ===========\n\n"

RED='\033[0;31m'
NC='\033[0m'

mkdir -p build
pushd build

${CMAKE} ../
make -j${JOBS}
popd

printf "\t=========== Copying FIO Contracts ABIs ===========\n\n"

execute cp contracts/fio.address/fio.address.abi build/contracts/fio.address/
execute cp contracts/fio.fee/fio.fee.abi build/contracts/fio.fee/
execute cp contracts/fio.request.obt/fio.request.obt.abi build/contracts/fio.request.obt/
execute cp contracts/fio.tpid/fio.tpid.abi build/contracts/fio.tpid/
execute cp contracts/fio.treasury/fio.treasury.abi build/contracts/fio.treasury/
execute cp contracts/fio.escrow/fio.escrow.abi build/contracts/fio.escrow/
execute cp contracts/fio.staking/fio.staking.abi build/contracts/fio.staking/
execute cp contracts/fio.oracle/fio.oracle.abi build/contracts/fio.oracle/
execute cp contracts/fio.perms/fio.perms.abi build/contracts/fio.perms/
echo
printf "\t=========== FIO Contracts Build Complete ===========\n\n"
echo
printf "${bldred}\n"
printf "      ___                       ___               \n"
printf "     /\\__\\                     /\\  \\          \n"
printf "    /:/ _/_      ___          /::\\  \\           \n"
printf "   /:/ /\\__\\    /\\__\\        /:/\\:\\  \\     \n"
printf "  /:/ /:/  /   /:/__/       /:/  \\:\\  \\        \n"
printf " /:/_/:/  /   /::\\  \\      /:/__/ \\:\\__\\     \n"
printf " \\:\\/:/  /    \\/\\:\\  \\__   \\:\\  \\ /:/  / \n"
printf "  \\::/__/        \\:\\/\\__\\   \\:\\  /:/  /    \n"
printf "   \\:\\  \\         \\::/  /    \\:\\/:/  /      \n"
printf "    \\:\\__\\        /:/  /      \\::/  /         \n"
printf "     \\/__/        \\/__/        \\/__/           \n\n${txtrst}"

printf "\\tFor more information:\\n"
printf "\\tFIO website: https://fio.net\\n"

