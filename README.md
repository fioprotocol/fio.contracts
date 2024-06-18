# FIO.Contracts
The Foundation for Interwallet Operability (FIO) or, in short, the FIO Protocol, is an open-source project based on EOSIO 1.8+. The smart contracts provide the basic functions of the FIO blockchain automating the execution of "agreements" by the blockchain producers for such things as account creation and use, fees, and voting.

* For information on FIO Protocol, visit [FIO](https://fio.net).
* For information on the FIO Chain, API, and SDKs, including detailed clone, build and deploy instructions, visit [FIO Protocol Developer Hub](https://dev.fio.net).
* To get updates on the development roadmap, visit [FIO Improvement Proposals](https://github.com/fioprotocol/fips). Anyone is welcome and encouraged to contribute.
* To contribute, please review [Contributing to FIO](CONTRIBUTING.md)
* To join the community, visit [Discord](https://discord.com/invite/pHBmJCc)

## Contract Summary
This repository contains examples of these privileged contracts that are useful when deploying, managing, and/or using an FIO blockchain.  They are provided for reference purposes:

   * [eosio.bios](./contracts/eosio.bios)
   * [eosio.msig](./contracts/eosio.msig)
   * [eosio.wrap](./contracts/eosio.wrap)
   * [fio.system](./contracts/fio.system)

The following unprivileged contract(s) are also part of the system.
   * [fio.address](./contracts/fio.address)
   * [fio.escrow](./contracts/fio.escrow)
   * [fio.fee](./contracts/fio.fee)
   * [fio.oracle](./contracts/fio.oracle)
   * [fio.perms](./contracts/fio.perms)
   * [fio.request.obt](./contracts/fio.request.obt)
   * [fio.staking](./contracts/fio.staking)
   * [fio.token](./contracts/fio.token)
   * [fio.tpid](./contracts/fio.tpid)
   * [fio.treasury](./contracts/fio.treasury)

## License
[FIO Contracts License](./LICENSE)

The included icons are provided under the same terms as the software and accompanying documentation, the MIT License.  We welcome contributions from the artistically-inclined members of the community, and if you do send us alternative icons, then you are providing them under those same terms.

## Release Information
* [TestNet Releases](docs/releases-testnet.md)
* [MainNet Releases](docs/releases.md)

## Build Information
The build script is located in main directory. Note that smart contracts are NOT installed per say; they are uploaded to the chain via clio, if a development chain, or via a multi-sig if a production chain.

### Build
The build script is straight-forward; it checks for any dependencies, then builds the contracts, putting the artifacts into the build directory.

To build, first change to the `~/fioprotocol/fio.contracts` folder, then execute the script as follows:

```shell
./build.sh
```

The build process writes all content to the `build` folder.

### Dependencies:
[fio.cdt](https://github.com/fioprotocol/fio.cdt/tree/release/1.5.x)
fio.cdt, version 1.5.x, must be installed onto the build machine in order for the contracts to successfully build. As such, the fio.contracts build script will automatically install the cdt (contract development toolkit), however, a manual build may be done by cloning the fio.cdt repo, then running build.sh and install.sh. Note that the fio.cdt artifacts are installed into `/usr/local` under the folder eosio.cdt and links are created to these artifacts in `/usr/local/bin`. Note: fio.cdt is a customized version of eosio.cdt but as such any installed artifacts retain the prefix of eosio.

[fio](https://github.com/fioprotocol/fio/)
While the FIO core blockchain is not a direct dependency of fio.contracts, in order to successfully execute contract functionality, getters may be called that are part of the blockchain. FIO contracts are backward compatible* to multiple versions of the blockchain. The latest release of fio.contracts aligns to the latest release of the fio.blockchain. The latest release may be found [here](https://github.com/fioprotocol/fio/releases).

* A forking change, one that includes a change to protocol, or the basic set of rules of that blockchain, mandates a minimum version of the blockchain that a version of that chain's contracts may operate successfully.