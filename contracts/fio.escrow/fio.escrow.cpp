/** FioEscrow implementation file
 *  Description: FioEscrow is the smart contract that allows the sell and purchasing of domains
 *  @author Thomas Le (BlockSmith)
 *  @modifedby
 *  @file fio.escrow.cpp
 *  @license
 */

#include "fio.escrow.hpp"
#include <eosiolib/asset.hpp>
#include <fio.common/fiotime.hpp>
#include <fio.common/fio.common.hpp>
#include <fio.address/fio.address.hpp>

namespace fioio {

    class [[eosio::contract("FioEscrow")]] FioEscrow : public eosio::contract {
    private:
        domainsales_table domainsales;
        domains_table domains;
    public:
        using contract::contract;

        FioEscrow(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                domains(AddressContract, AddressContract.value),
                domainsales(_self, _self.value) {
        }

        inline uint32_t get_list_time(){
            return now() + SALELISTTIME; // 3 months
        }

        uint32_t listdomain_update(const name &actor, const string &fio_domain, const uint128_t &domainhash, const int64_t &sale_price){
//            uint128_t domainHash = string_to_uint128_hash(fio_domain);
            uint128_t ownerHash = string_to_uint128_hash(actor.to_string());
            uint32_t expiration_time;

            expiration_time = get_list_time();

            uint64_t id = domainsales.available_primary_key();

            domainsales.emplace(actor, [&](struct domainsale &d){
                d.id = id;
                d.owner = actor.to_string();
                d.ownerhash = ownerHash;
                d.domain = fio_domain;
                d.domainhash = domainhash;
                d.sale_price = sale_price;
                d.expiration = expiration_time;
            });
            return expiration_time;
        }

        /***********
        * This action will list a fio domain for sale
        * @param actor this is the fio account that has sent this transaction.
        * @param fio_domain this is the fio domain to be sold.
        * @param sale_price this is the amount of FIO the seller wants for the domain
        * @param tpid  this is the fio address of the owner of the domain.
        */
        [[eosio::action]]
        void listdomain(const name &actor, const string &fio_domain, const int64_t &sale_price){
/*
            // Steps to take in this action:
            // -- Verify the actor owns the domain
            // -- Verify domain will not expire in 90 days
            // -- Verify the sale price is valid
            // *** not sure if i should write to the table first or transfer the domain first
            // -- require the seller to pay domainxfer fee to the chain
            // * require the seller to send domainxfer fee to fio.escrow (to cover the instance of a cancel OR to cover the xfer in the event of a sale)
            // -- transfer domain to fio.escrow
            // -- write the domain listing to the table
            // * this poses a problem because the xfer fee can change between the time it is listed and it gets cancelled or sold
            // this can be remedied by not collecting that fee up front and have it paid out of the sale price or when the seller cancels it'll cost the xfer fee
            // or we can collect the xfer fee at the time of listing and save that to the table and if the fee has changed just charge or refund the difference
*/
            require_auth(actor);

            // make sure sale price is greater than 0
            fio_400_assert(sale_price > 0, "sale_price", std::to_string(sale_price),
                           "Invalid sale_price value", ErrorInvalidAmount);

            // hash domain for easy querying
            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());

            // verify domain exists in the fio_domain table
            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter != domainsbyname.end(), "fio_domain", fio_domain,
                           "FIO domain not found", ErrorDomainNotRegistered);

            // check that the `actor` owners `fio_domain`
            fio_400_assert(domains_iter->account == actor.value, "fio_domain", fio_domain,
                            "FIO domain now owned by actor", ErrorDomainOwner);

            // write to table
            auto domainsale_id = listdomain_update(actor, fio_domain, domainHash, sale_price);

            const string response_string = string("{\"status\": \"OK\",\"domainsale_id\":\"") +
                    to_string(domainsale_id) + string("}");

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void cxlistdomain(const name &actor, const string &fio_domain){
            // Steps to take in this action:
            // -- Verify the actor is on the listing
            // -- transfer domain to actor
            // -- remove listing from table

            require_auth(actor);

            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());

            auto domainsalesbydomain = domainsales.get_index<"bydomain"_n>();
            auto domainsale_iter = domainsalesbydomain.find(domainHash);
            fio_400_assert(domainsale_iter != domainsalesbydomain.end(), "domainsale", fio_domain,
                           "FIO fee not found for endpoint", ErrorDomainSaleNotFound);

            domainsalesbydomain.erase(domainsale_iter);

            const string response_string = string("{\"status\": \"OK\"}");

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void buydomain(const name &actor, const uint64_t &domainsale_id){
            // Steps to take in this action:
            // -- Verify the actor is on the listing
            // -- retrieve FIO for the sale price
            // -- transfer domain to actor
            // -- divvy up the fees between marketplace, seller and bp fees
            // -- remove listing from table

            require_auth(actor);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }

        // this action is to renew the domain listing by 90 days. it must first check to see if the domain expires within 90
        // days and if it does it rejects the action
        [[eosio::action]]
        void rnlistdomain(const name &actor, const uint64_t &domainsale_id){

        }

        // this action is to set the config for a marketplace. this will be used to calculate the fee that is taken out
        // when a listing has sold.
        [[eosio::action]]
        void setmrkplccfg(const name &actor, const string &marketplace, const string &owner, const uint64_t &marketplacefee){

        }
    }; // class FioEscrow

    EOSIO_DISPATCH(FioEscrow, (listdomain)(cxlistdomain)(buydomain)(rnlistdomain)(setmrkplccfg))
}