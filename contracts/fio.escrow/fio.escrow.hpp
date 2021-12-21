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
        uint64_t sale_price = 0;
        double commission_fee = 0;
        uint64_t date_listed;
        uint64_t status; // status = 1: on sale, status = 2: Sold, status = 3; Cancelled
        uint64_t date_updated;

        uint64_t primary_key() const { return id; }
        uint128_t by_domain() const { return domainhash; }
        uint128_t by_owner() const { return ownerhash; }
        uint64_t by_status() const { return status; }
        uint64_t by_updated() const { return date_updated; }

        EOSLIB_SERIALIZE(domainsale,
                         (id)(owner)(ownerhash)
                         (domain)(domainhash)(sale_price)
                         (commission_fee)(date_listed)
                         (status)(date_updated)
        )
    };

    typedef multi_index<"domainsales"_n, domainsale
            ,indexed_by<"bydomain"_n, const_mem_fun<domainsale, uint128_t, &domainsale::by_domain>>
            ,indexed_by<"byowner"_n, const_mem_fun<domainsale, uint128_t, &domainsale::by_owner>>
            ,indexed_by<"bystatus"_n, const_mem_fun<domainsale, uint64_t, &domainsale::by_status>>
            ,indexed_by<"byupdated"_n, const_mem_fun<domainsale, uint64_t, &domainsale::by_updated>>
    >
    domainsales_table;

    struct [[eosio::action]] mrkplconfig {
        uint64_t id = 0;
        uint64_t owner = 0;
        uint128_t ownerhash = 0;
        double commission_fee = 0;
        uint64_t listing_fee = 0;
        uint64_t e_break = 0;

        uint64_t primary_key() const { return id; }
        uint128_t by_owner() const { return ownerhash; }

        EOSLIB_SERIALIZE(mrkplconfig,
                         (id)(owner)(ownerhash)(commission_fee)
                         (listing_fee)(e_break)
        )
    };

    typedef multi_index<"mrkplconfigs"_n, mrkplconfig> mrkplconfigs_table;
}

#endif //FIO_CONTRACTS_FIO_ESCROW_H
