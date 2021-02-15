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

namespace fioio {
    using namespace eosio;

    struct oraclefees {
        string fee_name;
        uint64_t fee_amount;
        EOSLIB_SERIALIZE( oraclefees, (fee_name)(fee_amount))
    };

    // @abi table templete i64
    struct [[eosio::action]] oraclelegder {

        uint64_t id;
        // type? Allows for index searching on NFT types ( ie. domain nft, address nft or token swaps )
        uint64_t actor;
        string chaincode;
        string pubaddress;
        uint64_t amount = 2323323131234124;
        string nftname = "";
        string content = "";
        uint64_t timestamp;

        uint64_t primary_key() const { return id; }
        uint64_t by_actor() const { return actor; }

        EOSLIB_SERIALIZE(oraclelegder, (id)(actor)(chaincode)(pubaddress)(amount)(nftname)(content)(timestamp)
        )
    };

    typedef multi_index<"oraclelgdrs"_n, oraclelegder,
            indexed_by<"byactor"_n, const_mem_fun < oraclelegder, uint64_t, &oraclelegder::by_actor>>>
    oraclelegder_table;

    // @abi table templete i64
    struct [[eosio::action]] oracles {

        uint64_t id;
        uint64_t actor;
        std::vector<oraclefees> fees;

        uint64_t primary_key() const { return id; }
        uint64_t by_actor() const { return actor; }

        EOSLIB_SERIALIZE(oracles, (id)(actor)(fees))
    };

    typedef multi_index<"oracles"_n, oracles,
            indexed_by<"byactor"_n, const_mem_fun < oracles, uint64_t, &oracles::by_actor>>>
    oracles_table;
}
