/** Fio Oracle implementation file
 *  Description:
 *  @author Casey Gardiner
 *  @modifedby
 *  @file fio.oracle.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE )
 */

#pragma once

#include <fio.common/fio.common.hpp>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <fio.token/include/fio.token/fio.token.hpp>

using std::string;

namespace fioio {
    using namespace eosio;

    struct oraclefees {
        string fee_name = "";
        uint64_t fee_amount = 0;
        EOSLIB_SERIALIZE(oraclefees, (fee_name)(fee_amount))
    };

    // @abi table templete i64
    struct [[eosio::action]] oracleledger {

        uint64_t id;
        // type? Allows for index searching on NFT types ( ie. domain nft, address nft or token swaps )
        uint64_t actor;
        string chaincode;
        string pubaddress;
        uint64_t amount = 0;
        string nftname = "";
        string content = "";
        uint64_t timestamp;

        uint64_t primary_key() const { return id; }
        uint64_t by_actor() const { return actor; }

        EOSLIB_SERIALIZE(oracleledger, (id)(actor)(chaincode)(pubaddress)(amount)(nftname)(content)(timestamp)
        )
    };

    typedef multi_index<"oracleldgrs"_n, oracleledger,
            indexed_by<"byactor"_n, const_mem_fun < oracleledger, uint64_t, &oracleledger::by_actor>>>
    oracleledger_table;

    // @abi table templete i64
    struct [[eosio::action]] oracles {

        uint64_t actor;
        std::vector <oraclefees> fees;

        uint64_t primary_key() const { return actor; }

        EOSLIB_SERIALIZE(oracles, (actor)(fees)
        )
    };

    typedef multi_index<"oracless"_n, oracles> oracles_table;

    // @abi table templete i64
    struct [[eosio::action]] oracle_votes {

        uint64_t id;
        uint128_t idhash;
        string obt_id;
        string fio_address;
        uint64_t amount;
        uint64_t timestamp;
        vector<name> voters;
        bool isComplete = false;

        uint64_t primary_key() const { return id; }
        uint128_t by_idhash() const { return idhash; }
        uint64_t by_finished() const { return isComplete; }

        EOSLIB_SERIALIZE(oracle_votes, (id)(idhash)(obt_id)(fio_address)(amount)(voters)(isComplete)
        )
    };

    typedef multi_index<"oravotes"_n, oracle_votes,
            indexed_by<"byidhash"_n, const_mem_fun < oracle_votes, uint128_t, &oracle_votes::by_idhash>>,
    indexed_by<"byfinished"_n, const_mem_fun<oracle_votes, uint64_t, &oracle_votes::by_finished>>
    >
    oraclevoters_table;
}
