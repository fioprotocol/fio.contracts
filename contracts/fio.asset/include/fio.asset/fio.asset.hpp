/** FioToken implementation file
 *  Description: FioToken is the smart contract that help manage the FIO Token.
 *  @author Adam Androulidakis, Casey Gardiner, Ed Rotthoff
 *  @modifedby
 *  @file fio.token.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE ) Dapix
 */

#pragma once

#include <fio.common/fio.common.hpp>
#include <fio.address/fio.address.hpp>
#include <fio.system/include/fio.system/fio.system.hpp>
#include <fio.fee/fio.fee.hpp>
#include <fio.tpid/fio.tpid.hpp>
#include <fio.staking/fio.staking.hpp>

namespace eosiosystem {
    class system_contract;
}

namespace eosio {
    using namespace fioio;

    using std::string;

    class [[eosio::contract("fio.token")]] token : public contract {
    private:
        fioio::eosio_names_table eosionames;
        fioio::fiofee_table fiofees;
        fioio::config appConfig;
        fioio::tpids_table tpids;
        fioio::fionames_table fionames;

    public:
        token(name s, name code, datastream<const char *> ds) : contract(s, code, ds),
                                                                eosionames(fioio::AddressContract,
                                                                           fioio::AddressContract.value),
                                                                fionames(fioio::AddressContract,
                                                                         fioio::AddressContract.value),
                                                                fiofees(fioio::FeeContract, fioio::FeeContract.value),
                                                                tpids(TPIDContract, TPIDContract.value){
            fioio::configs_singleton configsSingleton(fioio::FeeContract, fioio::FeeContract.value);
            appConfig = configsSingleton.get_or_default(fioio::config());
        }

        [[eosio::action]]
        void create(asset maximum_supply);

        [[eosio::action]]
        void issue(name to, asset quantity, string memo);

        [[eosio::action]]
        void retire(const asset &quantity, const string &memo, const name &actor);

        [[eosio::action]]
        void burn(const asset &quantity, const string &memo, const name &actor);

        /*[[eosio::action]]
        void transfer(name from,
                      name to,
                      asset quantity,
                      string memo);
        */
        [[eosio::action]]
        void trnspubky(const string &payee_public_key,
                          const asset &amount,
                          const int64_t &max_fee,
                          const name &actor,
                          const string &tpid);

        static asset get_supply(name token_contract_account, symbol_code sym_code) {
            stats statstable(token_contract_account, sym_code.raw());
            const auto &st = statstable.get(sym_code.raw());
            return st.supply;
        }

        static asset get_balance(name token_contract_account, name owner, symbol_code sym_code) {
            accounts accountstable(token_contract_account, owner.value);
            const auto &ac = accountstable.get(sym_code.raw());
            return ac.balance;
        }

        using create_action = eosio::action_wrapper<"create"_n, &token::create>;
        using issue_action = eosio::action_wrapper<"issue"_n, &token::issue>;
        using retire_action = eosio::action_wrapper<"retire"_n, &token::retire>;
        //using transfer_action = eosio::action_wrapper<"transfer"_n, &token::transfer>;
        using burn_action = eosio::action_wrapper<"burn"_n, &token::burn>;

    private:
        struct [[eosio::table]] account {
            asset balance;

            uint64_t primary_key() const { return balance.symbol.code().raw(); }
        };

        struct [[eosio::table]] currency_stats {
            asset supply;
            asset max_supply;
            name issuer;

            uint64_t primary_key() const { return supply.symbol.code().raw(); }
        };

        struct [[eosio::table]] nft_stats {
            asset supply;
            asset max_supply;
            name issuer;

            uint64_t primary_key() const { return supply.symbol.code().raw(); }
        };



        typedef eosio::multi_index<"accounts"_n, account> accounts;
        typedef eosio::multi_index<"stat"_n, currency_stats> stats;
        typedef eosio::multi_index<"assets"_n, nft_stats> assets;

        void sub_balance(name owner, asset value);

        void add_balance(name owner, asset value, name ram_payer);

        name transfer_public_key(const string &payee_public_key,
                                        const asset &amount,
                                        const int64_t &max_fee,
                                        const name &actor,
                                        const string &tpid,
                                        const int64_t &feeamount,
                                        const bool &errorifaccountexists);

  };
} /// namespace eosio
