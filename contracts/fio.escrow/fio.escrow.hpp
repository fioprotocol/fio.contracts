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
#include <fio.common/fio.accounts.hpp>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

namespace fioio {

    using namespace eosio;
    using namespace std;

    struct [[eosio::action]] domainsale {
        uint64_t id = 0;
        uint64_t owner = 0;
        uint128_t ownerhash = 0;
        string domain = nullptr;
        uint128_t domainhash = 0;
        int64_t sale_price;

        uint64_t primary_key() const { return id; }
        uint128_t by_domain() const { return domainhash; }
        uint128_t by_owner() const { return ownerhash; }

        EOSLIB_SERIALIZE(domainsale,
                         (id)(owner)(ownerhash)
                         (domain)(domainhash)(sale_price)
        )
    };

    typedef multi_index<"domainsales"_n, domainsale,
            indexed_by<"bydomain"_n, const_mem_fun<domainsale, uint128_t, &domainsale::by_domain>>,
            indexed_by<"byowner"_n, const_mem_fun<domainsale, uint128_t, &domainsale::by_owner>>
    >
    domainsales_table;

    struct [[eosio::action]] mrkplconfig {
        uint64_t id = 0;
        string marketplace = nullptr;
        uint128_t marketplacehash = 0;
        uint64_t owner = 0;
        uint128_t ownerhash = 0;
        string owner_public_key = nullptr;
        uint64_t commission_fee;
        uint64_t listing_fee;

        uint64_t primary_key() const { return id; }
        uint128_t by_marketplace() const { return marketplacehash; }
        uint128_t by_owner() const { return ownerhash; }

        EOSLIB_SERIALIZE(mrkplconfig,
                         (id)(marketplace)(marketplacehash)
                         (owner)(ownerhash)(owner_public_key)
                         (commission_fee)(listing_fee)
        )
    };

    typedef multi_index<"mrkplconfigs"_n, mrkplconfig,
            indexed_by<"bymarketplace"_n, const_mem_fun<mrkplconfig, uint128_t, &mrkplconfig::by_marketplace>>,
            indexed_by<"byowner"_n, const_mem_fun<mrkplconfig, uint128_t, &mrkplconfig::by_owner>>
    >
    mrkplconfigs_table;

    struct [[eosio::action]] holderacct {
        uint64_t id = 0;
        string holder_public_key;

        uint64_t primary_key() const { return id; }

        EOSLIB_SERIALIZE(holderacct, (id)(holder_public_key)
        )
    };

    typedef multi_index<"holderaccts"_n, holderacct> holderaccts_table;
}

#endif //FIO_CONTRACTS_FIO_ESCROW_H
