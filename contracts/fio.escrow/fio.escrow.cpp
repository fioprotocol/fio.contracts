/** FioEscrow implementation file
 *  Description: FioEscrow is the smart contract that allows the sell and purchasing of domains
 *  @author Thomas Le (BlockSmith)
 *  @modifedby
 *  @file fio.escrow.cpp
 *  @license
 */

#include "fio.escrow.hpp"
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

        /***********
        * This action will list a fio domain for sale
        * @param fio_domain this is the fio domain to be sold.
        * @param sale_price this is the amount of FIO the seller wants for the domain
        * @param tpid  this is the fio address of the owner of the domain.
        * @param actor this is the fio account that has sent this transaction.
        */
        [[eosio::action]]
        void listdomain(const name &actor, const string &fio_domain, const int64_t &sale_price,
                        const string &tpid){

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

            require_auth(actor);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);

            fio_400_assert(sale_price > 0 && sale_price > 0, "sale_price", std::to_string(sale_price),
                           "Invalid sale_price value", ErrorInvalidAmount);

            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter != domainsbyname.end(), "fio_domain", fio_domain,
                           "FIO domain not found", ErrorDomainNotRegistered);

            const string response_string = string("\"status\":\"OK\"}");

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void cxlistdomain(const name &actor, const uint64_t &domainlist_id){
            // Steps to take in this action:
            // -- Verify the actor is on the listing
            // -- transfer domain to actor
            // -- remove listing from table

            require_auth(actor);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void buydomain(const name &actor, const uint64_t &domainlist_id){
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
    }; // class FioEscrow

    EOSIO_DISPATCH(FioEscrow, (listdomain)(cxlistdomain)(buydomain))
}