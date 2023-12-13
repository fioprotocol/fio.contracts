/** delegate bandwidth file
 *  Description: this is part of the system contract. it supports voting power.
 *  @author Adam Androulidakis, Casey Gardiner, Ed Rotthoff
 *  @file delegate_bandwidth.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE ) Dapix
 *
 *  Changes:
 */
#include <fio.system/fio.system.hpp>

#include <eosiolib/eosio.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/privileged.h>
#include <eosiolib/transaction.hpp>

#include <fio.token/fio.token.hpp>


#include <cmath>
#include <map>

namespace eosiosystem {
    using eosio::asset;
    using eosio::indexed_by;
    using eosio::const_mem_fun;
    using eosio::permission_level;
    using eosio::time_point_sec;
    using std::map;
    using std::pair;

    static constexpr uint32_t refund_delay_sec = 3 * 24 * 3600;
    static constexpr int64_t ram_gift_bytes = 1400;

    struct [[eosio::table, eosio::contract("fio.system")]] user_resources {
        name owner;
        asset net_weight;
        asset cpu_weight;
        int64_t ram_bytes = 0;

        bool is_empty() const { return net_weight.amount == 0 && cpu_weight.amount == 0 && ram_bytes == 0; }

        uint64_t primary_key() const { return owner.value; }

        // explicit serialization macro is not necessary, used here only to improve compilation time
        EOSLIB_SERIALIZE( user_resources, (owner)(net_weight)(cpu_weight)(ram_bytes)
        )
    };


    typedef eosio::multi_index<"userres"_n, user_resources> user_resources_table;

    // void system_contract::update_voting_power(const name &voter, const asset &total_update) {
   void system_contract::updatepower(const name &voter,bool updateonly) {

        check((has_auth(SYSTEMACCOUNT) ||
               has_auth(MSIGACCOUNT) ||
               has_auth(WRAPACCOUNT) ||
               has_auth(ASSERTACCOUNT) ||
               has_auth(REQOBTACCOUNT) ||
               has_auth(FeeContract) ||
               has_auth(AddressContract) ||
               has_auth(TPIDContract) ||
               has_auth(TokenContract) ||
               has_auth(TREASURYACCOUNT) ||
               has_auth(StakingContract) ||
               has_auth(FIOSYSTEMACCOUNT) ||
               has_auth(EscrowContract) ||
               has_auth(FIOORACLEContract) ||
               has_auth(PERMSACCOUNT) ||
               has_auth(FIOACCOUNT)
              ),
              "missing required fio system account authority ");

        auto votersbyowner = _voters.get_index<"byowner"_n>();
        auto voter_itr = votersbyowner.find(voter.value);

        if ((voter_itr == votersbyowner.end())&& updateonly) {
            //its not there so return.
            return;
        }
        if ((voter_itr == votersbyowner.end())&& !updateonly) {
             _voters.emplace(voter, [&](auto &v) {
                v.owner = voter;
            });
             return;
        }

        if (voter_itr->producers.size() || voter_itr->proxy) {
            update_votes(voter, voter_itr->proxy, voter_itr->producers, false);
        }else {
            update_last_vote_weight(voter);
        }
    }


} //namespace eosiosystem
