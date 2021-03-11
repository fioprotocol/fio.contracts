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
        mrkplconfigs_table mrkplconfigs;
        holderaccts_table holderaccts;

        domains_table domains;
        eosio_names_table accountmap;
    public:
        using contract::contract;

        FioEscrow(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                domainsales(_self, _self.value),
                mrkplconfigs(_self, _self.value),
                holderaccts(_self, _self.value),
                domains(AddressContract, AddressContract.value),
                accountmap(AddressContract, AddressContract.value)
                { }

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
            return id;
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

            // transfer the domain to FIOESCROWACCOUNT
//            domainsbyname.modify(domains_iter, AddressContract, [&](struct domain &a) {
//                a.account = FIOESCROWACCOUNT.value;
//            });

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
        void buydomain(const name &actor, const string &fio_domain){
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
        void rnlistdomain(const name &actor, const string &fio_domain){

        }

        // this action is to set the config for a marketplace. this will be used to calculate the fee that is taken out
        // when a listing has sold.
        [[eosio::action]]
        void setmrkplcfg(const string &marketplace, const name &owner,
                          const string &owner_public_key, const uint64_t &marketplacefee){
            // only fio.escrow can call this action
            require_auth(owner);

            // TODO: check that all parameters are not null

            fio_400_assert(isPubKeyValid(owner_public_key),"owner_public_key", owner_public_key,
                           "Invalid FIO Public Key", ErrorPubKeyValid);

            eosio_assert(owner.length() == 12, "Length of account name should be 12");
            eosio_assert(marketplace.length() >= 1, "Length of marketplace name should be 1 or more characters");

            const bool accountExists = is_account(owner);

            auto acctmap_itr = accountmap.find(owner.value);

            fio_400_assert(acctmap_itr != accountmap.end(), "owner", owner.to_string(),
                           "Account is not bound on the fio chain",
                           ErrorPubAddressExist);
            fio_400_assert(accountExists, "owner", owner.to_string(),
                           "Account does not yet exist on the fio chain",
                           ErrorPubAddressExist);

            const string response_string = string("{\"status\": \"OK\"}");

            auto marketplaceByMarketplace = mrkplconfig.get_index<"bymarketplace"_n>();
            auto marketplace_iter = marketplaceByMarketplace.find(marketplace);

            uint64_t id = domainsales.available_primary_key();
            uint128_t ownerHash = string_to_uint128_hash(owner.value.to_string());

            if(marketplace_iter == mrkplconfig.end()){
                // not found, emplace
                mrkplconfig.emplace(FIOESCROWACCOUNT, [&](auto& row){
                    row.id = id;
                    row.marketplace = marketplace;
                    row.owner = owner.value;
                    row.ownerhash = ownerHash;
                    row.owner_public_key = owner_public_key;
                    row.marketplace_fee = marketplacefee;
                });
            } else {
                // found, modify
                // TODO: calling this action when there's already a match will only update the
                //  marketplacefee. this might need to be changed? perhaps a rmmrkplcfg and setmkplfee
                //  action one to remove a marketplace config and another to only change the fee

                mrkplconfig.modify(marketplace_iter, same_payer, [&](auto& row){
//                    row.marketplace = marketplace;
//                    row.owner = owner.value;
//                    row.ownerhash = ownerHash;
//                    row.owner_public_key = owner_public_key;
                    row.marketplace_fee = marketplacefee;
                });
            }

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void sethldacct(const string &public_key){
            check((has_auth(EscrowContract) || has_auth(SYSTEMACCOUNT)),
                  "missing required authority of fio.escrow, eosio");

            fio_400_assert(isPubKeyValid(owner_public_key),"owner_public_key", owner_public_key,
                           "Invalid FIO Public Key", ErrorPubKeyValid);

            holderaccts_table table(_self, _self.value);
            auto hold_account_itr = table.find(0); // only one entry so use 0th index

            if(hold_account_itr == table.end()){
                // not found, emplace
                table.emplace(EscrowContract, [&](auto& row){
                    row.id = 0;
                    row.holder_public_key = public_key.c_str();
                });
            } else {
                // found, modify
                // TODO: this will likely rarely be done but if it is there should be a transferring of all
                //  domains/addresses the old pubkey owns to this new pubkey
                table.modify(hold_account_itr, same_payer, [&](auto& row){
                    row.holder_public_key = public_key.c_str();
                });
            }

            const string response_string = string("{\"status\": \"OK\"}");

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }
    }; // class FioEscrow

    EOSIO_DISPATCH(FioEscrow, (listdomain)(cxlistdomain)(buydomain)(rnlistdomain)(setmrkplccfg)(sethldacct))
}