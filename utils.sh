# Execution helpers; necessary for BATS testing and log output in buildkite
function execute() {
  $VERBOSE && echo "--- Executing: $@"
  $DRYRUN || "$@"
}

function execute-quiet() {
  $VERBOSE && echo "--- Executing: $@ &>/dev/null"
  $DRYRUN || "$@" &>/dev/null
}

function execute-always() {
  ORIGINAL_DRYRUN=$DRYRUN
  DRYRUN=false
  execute "$@"
  DRYRUN=$ORIGINAL_DRYRUN
}

function execute-without-verbose() {
  ORIGINAL_VERBOSE=$VERBOSE
  VERBOSE=false
  execute "$@"
  VERBOSE=$ORIGINAL_VERBOSE
}

function pushd () {
    command pushd "$@" &> /dev/null
}

function popd () {
    command popd "$@" &> /dev/null
}

function setup() {
    if $VERBOSE; then
        echo "VERBOSE: ${VERBOSE}"
        echo "TEMP_DIR: ${TEMP_DIR}"
        echo "FIO_CNTRX_TMP_DIR: ${FIO_CNTRX_TMP_DIR}"
        echo "FIO_CNTRX_APTS_DIR: ${FIO_CNTRX_APTS_DIR}"
    fi
    ([[ -d ${BUILD_DIR} ]]) && execute rm -rf ${BUILD_DIR} # cleanup old build directory
    execute mkdir -p ${BUILD_DIR}
    execute-always mkdir -p ${TEMP_DIR}
    execute-always mkdir -p ${FIO_CNTRX_TMP_DIR}
    execute mkdir -p ${FIO_CNTRX_APTS_DIR}
    execute-always mkdir -p ${FIO_CDT_TMP_DIR}
}

function set-system-vars() {
    if [[ $ARCH == "Darwin" ]]; then
        export OS_VER=$(sw_vers -productVersion)
        export OS_MAJ=$(echo "${OS_VER}" | cut -d'.' -f1)
        export OS_MIN=$(echo "${OS_VER}" | cut -d'.' -f2)
        export OS_PATCH=$(echo "${OS_VER}" | cut -d'.' -f3)
        export MEM_GIG=$(bc <<< "($(sysctl -in hw.memsize) / 1024000000)")
        export DISK_INSTALL=$(df -h . | tail -1 | tr -s ' ' | cut -d\  -f1 || cut -d' ' -f1)
        export blksize=$(df . | head -1 | awk '{print $2}' | cut -d- -f1)
        export gbfactor=$(( 1073741824 / blksize ))
        export total_blks=$(df . | tail -1 | awk '{print $2}')
        export avail_blks=$(df . | tail -1 | awk '{print $4}')
        export DISK_TOTAL=$((total_blks / gbfactor ))
        export DISK_AVAIL=$((avail_blks / gbfactor ))
    else
        export DISK_INSTALL=$( df -h . | tail -1 | tr -s ' ' | cut -d\  -f1 )
        export DISK_TOTAL_KB=$( df . | tail -1 | awk '{print $2}' )
        export DISK_AVAIL_KB=$( df . | tail -1 | awk '{print $4}' )
        export MEM_GIG=$(( ( ( $(cat /proc/meminfo | grep MemTotal | awk '{print $2}') / 1000 ) / 1000 ) ))
        export DISK_TOTAL=$(( DISK_TOTAL_KB / 1048576 ))
        export DISK_AVAIL=$(( DISK_AVAIL_KB / 1048576 ))
    fi
    export CPU_CORES=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu)
    export JOBS=${JOBS:-$(( MEM_GIG > CPU_CORES ? CPU_CORES : MEM_GIG ))}
}

function ensure-cmake() {
    echo
    echo "${COLOR_CYAN}[Ensuring CMAKE installation]${COLOR_NC}"
    if [[ ! -e "${CMAKE}" ]]; then
        if ! is-cmake-built; then
            build-cmake
        fi
        install-cmake
        export CMAKE_LOCATION=${CMAKE_INSTALL_DIR}
        export CMAKE="${CMAKE_INSTALL_DIR}/bin/cmake"
        echo " - CMAKE successfully installed @ ${CMAKE}"
        echo ""
    else
        echo " - CMAKE found @ ${CMAKE}."
        echo ""
    fi
}

# CMake may be built but is it configured for the same install directory??? applies to other repos as well
function is-cmake-built() {
    if [[ -x ${FIO_CNTRX_TMP_DIR}/cmake-${CMAKE_VERSION}/build/bin/cmake ]]; then
        cmake_version=$(${FIO_CNTRX_TMP_DIR}/cmake-${CMAKE_VERSION}/build/bin/cmake --version | grep version | awk '{print $3}')
        if [[ $cmake_version =~ 3.2 ]]; then
            #cat ${FIO_CNTRX_TMP_DIR}/cmake-${CMAKE_VERSION}/build/CMakeCache.txt | grep CMAKE_INSTALL_PREFIX | grep ${EOSIO_INSTALL_DIR} >/dev/null
            #if [[ $? -eq 0 ]]; then
            #    return
            #fi
            return
        fi
    fi
    false
}

function build-cmake() {
    echo "Building cmake..."
    execute bash -c "cd $FIO_CNTRX_TMP_DIR \
        && rm -rf cmake-${CMAKE_VERSION} \
        && curl -LO https://cmake.org/files/v${CMAKE_VERSION_MAJOR}.${CMAKE_VERSION_MINOR}/cmake-${CMAKE_VERSION}.tar.gz \
        && tar -xzf cmake-${CMAKE_VERSION}.tar.gz \
        && rm -f cmake-${CMAKE_VERSION}.tar.gz \
        && cd cmake-${CMAKE_VERSION} \
        && mkdir build && cd build \
        && ../bootstrap --prefix=${CMAKE_INSTALL_DIR} \
        && make -j${JOBS}"
}

function install-cmake() {
    echo "Installing cmake..."
    execute bash -c "cd $FIO_CNTRX_TMP_DIR/cmake-${CMAKE_VERSION} \
        && cd build \
        && make install"
}

function ensure-cdt() {
    echo
    echo "${COLOR_CYAN}[Ensuring fio.cdt installation]${COLOR_NC}"
    #if [[ ! -d "${FIO_CDT_TMP_DIR}/fio.cdt-${CDT_VERSION}/build/bin/eosio-cpp" ]]; then
    if ! hash eosio-cpp 2>/dev/null; then
        if ! is-cdt-built; then
            build-cdt
        fi
        install-cdt
        echo " - FIO.CDT successfully installed @ ${FIO_CDT_INSTALL_DIR}"
        echo ""
    else
        [[ ! -z $(command -v eosio-cpp 2>/dev/null) ]] && cdt_bin=$(command -v eosio-cpp 2>/dev/null) && [[ "${cdt_bin}" =~ "${FIO_CDT_INSTALL_DIR}" ]]
        if [[ $? -eq 0 ]]; then
            echo " - FIO.CDT found @ ${FIO_CDT_INSTALL_DIR}."
        else
            echo " - FIO.CDT found @ ${cdt_bin}."
        fi
        echo ""
    fi
}

# CMake may be built but is it configured for the same install directory??? applies to other repos as well
function is-cdt-built() {
    if [[ -x ${FIO_CDT_TMP_DIR}/fio.cdt-${CDT_VERSION}/build/bin/eosio-cpp ]]; then
        #FIO eosio-cpp version 1.5.0
        cdt_version=$(${FIO_CDT_TMP_DIR}/fio.cdt-${CDT_VERSION}/build/bin/eosio-cpp --version | grep version | awk '{print $4}')
        if [[ $cdt_version =~ 1.5 ]]; then
            #cat ${FIO_CNTRX_TMP_DIR}/cmake-${CMAKE_VERSION}/build/CMakeCache.txt | grep CMAKE_INSTALL_PREFIX | grep ${EOSIO_INSTALL_DIR} >/dev/null
            #if [[ $? -eq 0 ]]; then
            #    return
            #fi
            return
        fi
    fi
    false
}

function build-cdt() {
    echo "Building fio.cdt..."
    execute bash -c "cd ${FIO_CDT_TMP_DIR} \
        && rm -rf fio.cdt-${CDT_VERSION} \
        && git clone https://www.github.com/fioprotocol/fio.cdt.git fio.cdt-${CDT_VERSION} \
        && cd fio.cdt-${CDT_VERSION} \
        && git submodule update --init --recursive \
        && git checkout --recurse-submodules -- . \
        && rm -rf build \
        && git checkout feature/bd-4618-ubuntu-upgrade \
        && ./build.sh -c ${CMAKE_LOCATION}"
}

function install-cdt() {
    echo "Installing fio.cdt..."
    execute bash -c "cd $FIO_CDT_TMP_DIR/fio.cdt-${CDT_VERSION} \
        && sudo ./install.sh"
}
