/** FioEscrow header file
 *  Description: FioEscrow is the smart contract that allows the sell and purchasing of domains
 *  @author Thomas Le (BlockSmith)
 *  @modifedby
 *  @file fio.escrow.hpp
 *  @license
 */

#ifndef FIO_CONTRACTS_FIO_ESCROW_H
#define FIO_CONTRACTS_FIO_ESCROW_H

#include <fio.common/fio.common.hpp>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

using std::string;

namespace fioio {

    // table will store all the domains that are for sale
    // it needs the name of the domain, the seller and the amount the seller wants in FIO
    struct [[eosio::action]] domainsale {

        uint64_t id = 0;
        uint64_t owner;
        uint128_t ownerhash = 0;
        string domain = nullptr;
        uint128_t domainhash = 0;
        asset sale_price;

        // primary_key is required to store structure in multi_index table
        uint64_t primary_key() const { return id; }

        EOSLIB_SERIALIZE(domainsale, (id)(owner)(ownerhash)(domain)(domainhash)(sale_price))
    };

    typedef multi_index<"domainsales"_n, domainsale>
            domainsales_table;
}

#endif //FIO_CONTRACTS_FIO_ESCROW_H
