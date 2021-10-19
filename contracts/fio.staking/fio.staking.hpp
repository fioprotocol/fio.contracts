/** Fio Staking implementation file
 *  Description:
 *  @author Ed Rotthoff
 *  @modifedby
 *  @file fio.staking.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE )
 */

#pragma once

#include <fio.common/fio.common.hpp>
#include <fio.address/fio.address.hpp>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

namespace fioio {
    using namespace eosio;

    //staking info is a global state table used to track information relating to staking within the FIO protocol.
    struct [[eosio::table("staking"), eosio::contract("fio.staking")]] global_staking_state {
        global_staking_state(){}
        uint64_t staked_token_pool = 0;   //total FIO tokens staked for all accounts, units sufs.
        uint64_t combined_token_pool = 0;  //total fio tokens staked for all accounts plus fio rewards all accounts, units SUFs,
        // incremented by the staked amount when user stakes, when tokens are earmarked as staking rewards,
        // decremented by unstaked amount + reward amount when users unstake
        uint64_t last_combined_token_pool = 1000000000000000;
        uint64_t rewards_token_pool = 0; //total counter how much has come in from fees units SUFs
        uint64_t global_srp_count = 0;  // units SUS, total SRP for all FIO users, increment when users stake, decrement when users unstake.
        uint64_t last_global_srp_count = 2000000000000000;
        uint64_t daily_staking_rewards = 0; //this is used to track the daily staking rewards collected from fees,
        // its used only to determine if the protocol should mint FIO whe rewards are under the DAILYSTAKINGMINTTHRESHOLD
        uint64_t staking_rewards_reserves_minted = 0; //the total amount of FIO used in minting rewards tokens, will not exceed STAKINGREWARDSRESERVEMAXIMUM

        EOSLIB_SERIALIZE(global_staking_state,(staked_token_pool)
                (combined_token_pool)(last_combined_token_pool)(rewards_token_pool)(global_srp_count)(last_global_srp_count)
                (daily_staking_rewards)(staking_rewards_reserves_minted)
        )
    };

    //stake account table holds staking info used to compute staking rewards by FIO account
    struct [[eosio::table, eosio::contract("fio.staking")]] account_staking_info {
        uint64_t id = 0;   //unique id for ease of maintenance. primary key
        name account;  //the account name associated with this staking info, secondary key,
        uint64_t total_srp = 0; //the staking rewards points awarded to this account, units SUS, incremented on stake, decremented on unstake.
        uint64_t total_staked_fio = 0;  //total fio staked by this account, units SUFs, incremented on stake, decremented on unstake.

        uint64_t primary_key() const{return id;}
        uint64_t by_account() const { return account.value; }

        EOSLIB_SERIALIZE( account_staking_info,(id)
                (account)(total_srp)(total_staked_fio)
        )
    };
    typedef eosio::multi_index<"accountstake"_n, account_staking_info,
            indexed_by<"byaccount"_n, const_mem_fun<account_staking_info, uint64_t, &account_staking_info::by_account>>
    > account_staking_table;


    typedef eosio::singleton<"staking"_n, global_staking_state> global_staking_singleton;


    static const uint128_t STAKING_MULT = 1000000000000000000;

    //this method will perform integer division with rounding.
    //returns
    // the rounded result of numerator / denominator
    static uint128_t fiointdivwithrounding(const uint128_t numerator, const uint128_t denominator) {

        uint128_t res = numerator / denominator;
        uint128_t rem_res = numerator %  denominator;
        if(rem_res >= (denominator / (uint128_t)2)){
            res++;
        }
        return res;
    }

    //this method will perform integer division with rounding.
    //returns
    // the rounded result of numerator / denominator
    static uint128_t fiointdivwithrounding(const uint64_t numerator, const uint64_t denominator) {

        uint64_t res = numerator / denominator;
        uint64_t rem_res = numerator %  denominator;
        if(rem_res >= (denominator / (uint64_t)2)){
            res++;
        }
        return res;
    }
}
