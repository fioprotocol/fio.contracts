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

    // @abi table templete i64
    struct [[eosio::action]] oraclelegder {

        uint64_t id;
        uint64_t actor;
        string chaincode;
        string pubaddress;
        uint64_t amount = 0;
        string content = "";

        uint64_t primary_key() const { return id; }
        uint64_t by_actor() const { return actor; }

        EOSLIB_SERIALIZE(oraclelegder, (id)(actor)(chaincode)(pubaddress)(amount)(content)
        )
    };

    typedef multi_index<"oraclelegder"_n, oraclelegder,
            indexed_by<"byactor"_n, const_mem_fun < oraclelegder, uint64_t, &oraclelegder::by_actor>>>
    oraclelegder_table;
}
