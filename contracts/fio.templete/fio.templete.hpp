/** Fio Templete implementation file
 *  Description:
 *  @author Casey Gardiner
 *  @modifedby
 *  @file fio.templete.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE ) Dapix
 */

#pragma once

#include <fio.common/fio.common.hpp>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

namespace fioio {
    using namespace eosio;

    // @abi table templete i64
//    struct [[eosio::action]] templete {
//
//        uint64_t id;
//        uint128_t fioaddhash;
//        string fioaddress;
//        uint64_t tempvar;
//
//        uint64_t primary_key() const { return id; }
//        uint128_t by_name() const { return fioaddhash; }
//
//        EOSLIB_SERIALIZE(templete, (id)(fioaddhash)(fioaddress)(tempvar)
//        )
//    };
//
//    typedef multi_index<"templetes"_n, templete,
//            indexed_by<"byname"_n, const_mem_fun < templete, uint128_t, &templete::by_name>>>
//    templetes_table;
}