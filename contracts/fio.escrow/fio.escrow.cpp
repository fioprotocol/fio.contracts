/** FioEscrow implementation file
 *  Description: FioEscrow is the smart contract that allows the sell and purchasing of domains
 *  @author Thomas Le (BlockSmith)
 *  @modifedby
 *  @file fio.escrow.cpp
 *  @license
 */

#include "fio.escrow.hpp"
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <fio.common/fiotime.hpp>
#include <fio.common/fio.common.hpp>
#include <fio.address/fio.address.hpp>

namespace fioio {

    class [[eosio::contract("FioEscrow")]] FioEscrow : public eosio::contract {
    private:
        domainsales_table  domainsales;
        mrkplconfigs_table mrkplconfigs;
        holderaccts_table  holderaccts;
        domains_table      domains;
        eosio_names_table  accountmap;
    public:
        using contract::contract;

        FioEscrow(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                domainsales(_self, _self.value),
                mrkplconfigs(_self, _self.value),
                holderaccts(_self, _self.value),
                domains(AddressContract, AddressContract.value),
                accountmap(AddressContract, AddressContract.value) {}

        inline uint32_t get_list_time() {
            return now() + SALELISTTIME; // 3 months
        }

        uint32_t listdomain_update(const name &actor, const string &fio_domain,
                                   const uint128_t &domainhash, const string &marketplace,
                                   const uint128_t &marketplacehash, const int64_t &sale_price) {

            uint128_t ownerHash = string_to_uint128_hash(actor.to_string());
            uint32_t  expiration_time;

            expiration_time = get_list_time();

            uint64_t id = domainsales.available_primary_key();

            domainsales.emplace(actor, [&](struct domainsale &d) {
                d.id              = id;
                d.owner           = actor.value;
                d.ownerhash       = ownerHash;
                d.domain          = fio_domain;
                d.domainhash      = domainhash;
                d.marketplace     = marketplace;
                d.marketplacehash = marketplacehash;
                d.sale_price      = sale_price;
                d.expiration      = expiration_time;
            });
            return id;
        }
        // listdomain_update

        /***********
        * This action will list a fio domain for sale. It also collects a fee for listing that is sent to the
         * marketplace that is passed in as a parameter. It transfers the domain ownership to a holderaccount
         * that is set in the holder account table
        * @param actor this is the fio account that has sent this transaction.
        * @param fio_domain this is the fio domain to be sold.
        * @param sale_price this is the amount of FIO the seller wants for the domain
        * @param marketplace the name of the marketplace used to list this domain
        */
        [[eosio::action]]
        void listdomain(const name &actor, const string &fio_domain,
                        const int64_t &sale_price, const string &marketplace,
                        const string &notif_address) {
// region auth check
            require_auth(actor);
// endregion

// region Parameter assertions
            fio_400_assert(sale_price >= 1000000000, "sale_price", std::to_string(sale_price),
                           "Sale price should be greater than 1 FIO (1,000,000,000 SUF)", ErrorInvalidAmount);
// endregion

// region Check `marketplace`
            eosio_assert(marketplace.length() >= 1 && marketplace.length() <= 25, "Length of marketplace name should be between 1 and 25 characters");
            uint128_t marketplaceHash = string_to_uint128_hash(marketplace.c_str());

            auto marketplaceByMarketplace = mrkplconfigs.get_index<"bymarketplace"_n>();
            auto marketplace_iter         = marketplaceByMarketplace.find(marketplaceHash);
            fio_400_assert(marketplace_iter != marketplaceByMarketplace.end(), "marketplace", marketplace,
                           "Marketplace not found", ErrorNoWork);
// endregion

// region marketplace found
            // found
            auto listingfee         = asset(marketplace_iter->listing_fee, FIOSYMBOL);
            auto marketplaceAccount = name(marketplace_iter->owner);

            const bool accountExists = is_account(marketplaceAccount);
            auto       acctmap_itr   = accountmap.find(marketplaceAccount.value);

            action(permission_level{EscrowContract, "active"_n},
                   TokenContract, "transfer"_n,
                   make_tuple(actor, marketplaceAccount, listingfee, string("Listing fee"))
            ).send();
// endregion

            // hash domain for easy querying
            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());

            // verify domain exists in the fio_domain table
            auto domainsbyname                 = domains.get_index<"byname"_n>();
            auto domains_iter                  = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter != domainsbyname.end(), "fio_domain", fio_domain,
                           "FIO domain not found", ErrorDomainNotRegistered);

            // check that the `actor` owners `fio_domain`
            fio_400_assert(domains_iter->account == actor.value, "fio_domain", fio_domain,
                           "FIO domain not owned by actor", ErrorDomainOwner);

            // get holder account that will hold the domains while listed for sale
            holderaccts_table table(_self, _self.value);
            auto              hold_account_itr = table.find(0); // only one entry so use 0th index

            string new_owner_fio_public_key;

            // if holderAccount found, set the new_owner_fio_public_key to that public key
            if (hold_account_itr != table.end()) {
                new_owner_fio_public_key = hold_account_itr->holder_public_key;
            }

            // transfer the domain to holder account
            action(
                    permission_level{EscrowContract, "active"_n},
                    AddressContract,
                    "xferescrow"_n,
                    std::make_tuple(fio_domain, new_owner_fio_public_key, actor)
            ).send();

            // write to table
            auto domainsale_id = listdomain_update(actor, fio_domain, domainHash,
                                                   marketplace, marketplaceHash, sale_price);

            const string response_string = string("{\"status\": \"OK\",\"domainsale_id\":\"") +
                                           to_string(domainsale_id) + string("}");

//            const string response_string = string("{\"status\": \"OK\"}");

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }
        // listdomain

        /***********
        * This action will list a fio domain for sale
        * @param actor this is the account name that listed the domain
        * @param this is the name of the fio_domain
        */
        [[eosio::action]]
        void cxlistdomain(const name &actor, const string &fio_domain) {
            // will be called by either the account listing the domain `actor`
            // or the EscrowContract from `chkexplistng` when the listing is expired
            check(has_auth(actor) || has_auth(EscrowContract), "Permission Denied");

            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());

            auto domainsalesbydomain = domainsales.get_index<"bydomain"_n>();
            auto domainsale_iter     = domainsalesbydomain.find(domainHash);
            fio_400_assert(domainsale_iter != domainsalesbydomain.end(), "domainsale", fio_domain,
                           "Domain not found", ErrorDomainSaleNotFound);

            domainsalesbydomain.erase(domainsale_iter);

            const bool accountExists = is_account(actor);

            auto owner = accountmap.find(actor.value);

            fio_400_assert(owner != accountmap.end(), "owner_account", actor.to_string(),
                           "Account is not bound on the fio chain",
                           ErrorPubAddressExist);
            fio_400_assert(accountExists, "owner_account", actor.to_string(),
                           "Account does not yet exist on the fio chain",
                           ErrorPubAddressExist);

            action(
                    permission_level{EscrowContract, "active"_n},
                    AddressContract,
                    "xferescrow"_n,
                    std::make_tuple(fio_domain, owner->clientkey, actor)
            ).send();

            // send a fio request

            const string response_string = string("{\"status\": \"OK\"}");

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }
        // cxlistdomain

        /***********
        * This action will list a fio domain for sale
        * @param buyer this is the account name that listed the domain
        * @param this is the name of the fio_domain
        */
        [[eosio::action]]
        void buydomain(const name &buyer, const string &fio_domain, const string &marketplace) {
            // Steps to take in this action:
            // -- Verify the actor is on the listing
            // -- retrieve FIO for the sale price
            // -- transfer domain to actor
            // -- divvy up the fees between marketplace, seller and bp fees
            // -- remove listing from table
            require_auth(buyer);

// region Check if marketplace exists
            eosio_assert(marketplace.length() >= 1, "Length of marketplace name should be between 1 and 25 characters");
            uint128_t marketplaceHash = string_to_uint128_hash(marketplace.c_str());

            auto marketplaceByMarketplace = mrkplconfigs.get_index<"bymarketplace"_n>();
            auto marketplace_iter         = marketplaceByMarketplace.find(marketplaceHash);
            fio_400_assert(marketplace_iter != marketplaceByMarketplace.end(), "marketplace", marketplace,
                           "Marketplace not found", ErrorNoWork);
// endregion

// region Check if domain is listed for sale
            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());

            auto domainsalesbydomain = domainsales.get_index<"bydomain"_n>();
            auto domainsale_iter     = domainsalesbydomain.find(domainHash);
            fio_400_assert(domainsale_iter != domainsalesbydomain.end(), "domainsale", fio_domain,
                           "Domain not found", ErrorDomainSaleNotFound);
// endregion

            auto saleprice           = asset(domainsale_iter->sale_price, FIOSYMBOL);
            auto marketCommissionFee = marketplace_iter->commission_fee / 100.0;
            auto marketCommission    = (marketCommissionFee > 0) ?
                                       asset(saleprice.amount * marketCommissionFee, FIOSYMBOL) : asset(0, FIOSYMBOL);
            auto toBuyer             = asset(saleprice.amount - marketCommission.amount, FIOSYMBOL);

// region get private key of buyer
            const bool accountExists = is_account(buyer);
            auto       buyerAcct     = accountmap.find(buyer.value);
            fio_400_assert(buyerAcct != accountmap.end(), "buyer", buyer.to_string(),
                           "Account is not bound on the fio chain",
                           ErrorPubAddressExist);
            fio_400_assert(accountExists, "buyer", buyer.to_string(),
                           "Account does not yet exist on the fio chain",
                           ErrorPubAddressExist);
// endregion

// region Make FIO transfer from buyer to seller - minus fee
            action(permission_level{EscrowContract, "active"_n},
                   TokenContract, "transfer"_n,
                   make_tuple(buyer, domainsale_iter->owner, toBuyer, string("Domain Purchase"))
            ).send();
// endregion

// region Make FIO transfer from buyer to marketplace
            action(permission_level{EscrowContract, "active"_n},
                   TokenContract, "transfer"_n,
                   make_tuple(buyer, marketplace_iter->owner, marketCommission, string("Marketplace Commission"))
            ).send();
// endregion

// region transfer domain ownership to buyer
            // transfer the domain to holder account
            action(
                    permission_level{EscrowContract, "active"_n},
                    AddressContract,
                    "xferescrow"_n,
                    std::make_tuple(fio_domain, buyerAcct->clientkey, buyer)
            ).send();
// endregion

// region remove listing from table
            domainsalesbydomain.erase(domainsale_iter);

            domainsale_iter = domainsalesbydomain.find(domainHash);
            fio_400_assert(domainsale_iter == domainsalesbydomain.end(), "domainsale", fio_domain,
                           "Domain listing not removed properly", ErrorDomainSaleNotFound);
// endregion

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }
        // buydomain

        /***********
        * this action is to renew the domain listing by 90 days. it must first check
        * to see if the domain expires within 90 days and if it does it rejects the action.
         * This action will fail if the listing does not expire within the `warn_days` duration.
        * @param actor this is the account name that listed the domain
        * @param this is the name of the fio_domain
        */
        [[eosio::action]]
        void rnlistdomain(const name &actor, const string &fio_domain) {
            require_auth(actor);
            // TODO: 3/15/21

            // find listing based on fio_domain
        }
        // rnlistdomain

        /***********
        * this action can only be called by the system contract or fio.escrow and it sets
        * the pubkey that will be the trusted owner of domains/addresses when they are listed on sale.
        * @param
        */
        [[eosio::action]]
        void sethldacct(const string &public_key) {
            check((has_auth(EscrowContract) || has_auth(SYSTEMACCOUNT)),
                  "missing required authority of fio.escrow, eosio");

            fio_400_assert(isPubKeyValid(public_key), "owner_public_key", public_key,
                           "Invalid FIO Public Key", ErrorPubKeyValid);

            holderaccts_table table(_self, _self.value);
            auto              hold_account_itr = table.find(0); // only one entry so use 0th index

            if (hold_account_itr == table.end()) {
                // not found, emplace
                table.emplace(EscrowContract, [&](auto &row) {
                    row.id                = 0;
                    row.holder_public_key = public_key.c_str();
                });
            } else {
                // found, modify
                // TODO: this will likely rarely be done but if it is there should be a transferring of all
                //  domains/addresses the old pubkey owns to this new pubkey.

                table.modify(hold_account_itr, same_payer, [&](auto &row) {
                    row.holder_public_key = public_key.c_str();
                });
            }

            const string response_string = string("{\"status\": \"OK\"}");

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }
        // sethldacct


        /***********
        * this action is to set the config for a marketplace. this will be used to calculate the fee that is taken out
        * when a listing has sold.
        * @param
        */
        [[eosio::action]]
        void setmrkplcfg(const string &marketplace, const name &owner,
                         const string &owner_public_key, const uint64_t &commissionfee,
                         const uint64_t &listingfee, const uint64_t &duration,
                         const uint64_t &warn_days) {
            require_auth(owner);
            // TODO: 3/15/21 check validity of duration and warn days and add to the table

            fio_400_assert(isPubKeyValid(owner_public_key), "owner_public_key", owner_public_key,
                           "Invalid FIO Public Key", ErrorPubKeyValid);

            eosio_assert(owner.length() == 12, "Length of account name should be 12");
            eosio_assert(marketplace.length() >= 1 && marketplace.length() <= 25, "Length of marketplace name should be between 1 and 25 characters");
            eosio_assert(commissionfee > -1 && commissionfee <= 25, "Commission fee should be between 0 and 25");
            eosio_assert(listingfee > -1 && listingfee <= 10, "Listing fee should be between 0 and 25");

            const bool accountExists = is_account(owner);

            auto acctmap_itr = accountmap.find(owner.value);

            fio_400_assert(acctmap_itr != accountmap.end(), "owner", owner.to_string(),
                           "Account is not bound on the fio chain",
                           ErrorPubAddressExist);
            fio_400_assert(accountExists, "owner", owner.to_string(),
                           "Account does not yet exist on the fio chain",
                           ErrorPubAddressExist);

            uint128_t marketplaceHash = string_to_uint128_hash(marketplace.c_str());

            auto marketplaceByMarketplace = mrkplconfigs.get_index<"bymarketplace"_n>();
            auto marketplace_iter         = marketplaceByMarketplace.find(marketplaceHash);

            uint64_t  id        = mrkplconfigs.available_primary_key();
            uint128_t ownerHash = string_to_uint128_hash(owner.to_string());

            if (marketplace_iter == marketplaceByMarketplace.end()) {
                // not found, emplace
                mrkplconfigs.emplace(EscrowContract, [&](auto &row) {
                    row.id               = id;
                    row.marketplace      = marketplace;
                    row.marketplacehash  = marketplaceHash;
                    row.owner            = owner.value;
                    row.ownerhash        = ownerHash;
                    row.owner_public_key = owner_public_key;
                    row.commission_fee   = commissionfee;
                    row.listing_fee      = listingfee;
                });
            } else {
                fio_400_assert(marketplace_iter == marketplaceByMarketplace.end(), "marketplace", marketplace,
                               "Marketplace already exists. Use rmmrkplcfg or setmkplfee.",
                               ErrorNoWork);
            }

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }
        // setmrkplcfg

        /***********
        * this action will remove the marketplace config from the table.
        * It will need to check if there are any listings by that marketplace and if there are prevent it
        * from being deleted.
        * @param
        */
        [[eosio::action]]
        void rmmrkplcfg(const name &actor, const string &marketplace) {
            require_auth(actor);

            eosio_assert(marketplace.length() >= 1, "Length of marketplace name should be between 1 and 25 characters");

            uint128_t marketplaceHash = string_to_uint128_hash(marketplace.c_str());
            uint128_t ownerHash       = string_to_uint128_hash(actor.to_string());

            auto marketplaceByOwner = mrkplconfigs.get_index<"byowner"_n>();
            auto marketplace_iter   = marketplaceByOwner.find(ownerHash);

            fio_400_assert(marketplace_iter == marketplaceByOwner.end(), "marketplace", marketplace,
                           "Marketplace cannot be found",
                           ErrorNoWork);

            fio_400_assert(marketplace_iter->marketplace == marketplace, "marketplace", marketplace,
                           "Only owner of marketplace can modify config",
                           ErrorNoWork);

            marketplaceByOwner.erase(marketplace_iter);

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }
        // rmmrkplcfg

        /***********
        * this action will simply update the marketplace commission fee that is subtracted from the sale price. Default is 6%
        * @param actor `name`
        * @param marketplace `string`
        * @param fee `uint64`
        */
        [[eosio::action]]
        void setmkpcomfee(const name &actor, const string &marketplace, const uint64_t &fee) {
            require_auth(actor);

            // sanity check of parameters
            eosio_assert(marketplace.length() >= 1, "Length of marketplace name should be 1 or more characters");
            eosio_assert(fee >= 0 && fee <= 50, "Fee may only be between 0 and 50. ");

            uint128_t marketplaceHash = string_to_uint128_hash(marketplace.c_str());

            // find marketplace on the `mrkplconfig` table
            auto marketplaceByMarketplace = mrkplconfigs.get_index<"bymarketplace"_n>();
            auto marketplace_iter         = marketplaceByMarketplace.find(marketplaceHash);

            fio_400_assert(marketplace_iter == marketplaceByMarketplace.end(), "marketplace", marketplace,
                           "Marketplace cannot be found",
                           ErrorNoWork);


            // verify the `actor` is the owner of the marketplace
            fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                           "Only owner of marketplace can modify config",
                           ErrorNoWork);

            // change the commission fee value
            marketplaceByMarketplace.modify(marketplace_iter, same_payer, [&](auto &row) {
                row.commission_fee = fee;
            });

            // verify the listing fee changed
            fio_400_assert(marketplace_iter->commission_fee == fee, "fee", to_string(fee),
                           "The table update failed.",
                           ErrorNoWork);

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }
        // setmkpcomfee

        /***********
        * this action will simply update the marketplace listing fee that is charged up front. Default is 4 FIO
        * @param
        */
        [[eosio::action]]
        void setmkplstfee(const name &actor, const string &marketplace, const uint64_t &fee) {
            require_auth(actor);

            // sanity check of parameters
            eosio_assert(marketplace.length() >= 1, "Length of marketplace name should be 1 or more characters");
            eosio_assert(fee >= 0, "Fee must 0 or more. ");

            uint128_t marketplaceHash = string_to_uint128_hash(marketplace.c_str());

            // find marketplace on the `mrkplconfig` table
            auto marketplaceByMarketplace = mrkplconfigs.get_index<"bymarketplace"_n>();
            auto marketplace_iter         = marketplaceByMarketplace.find(marketplaceHash);

            fio_400_assert(marketplace_iter == marketplaceByMarketplace.end(), "marketplace", marketplace,
                           "Marketplace cannot be found",
                           ErrorNoWork);

            // verify the `actor` is the owner of the marketplace
            fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                           "Only owner of marketplace can modify config",
                           ErrorNoWork);

            // change the listing fee value
            marketplaceByMarketplace.modify(marketplace_iter, same_payer, [&](auto &row) {
                row.listing_fee = fee;
            });

            // verify the listing fee changed
            fio_400_assert(marketplace_iter->listing_fee == fee, "fee", to_string(fee),
                           "The table update failed.",
                           ErrorNoWork);

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }
        // setmkplstfee

        /***********
        * this action will simply update the marketplace listing duration time. default is 90 days.
        * @param
        */
        [[eosio::action]]
        void setmkplstdur(const name &actor, const string &marketplace, const uint64_t &listing_dur) {
            require_auth(actor);

            // sanity check of parameters
            eosio_assert(marketplace.length() >= 1, "Length of marketplace name should be 1 or more characters");
            eosio_assert(listing_dur >= 1 && listing_dur <= 365,
                         "Listing duration must be greater than 1 day and less than 365 days.");

            uint128_t marketplaceHash = string_to_uint128_hash(marketplace.c_str());

            // find marketplace on the `mrkplconfig` table
            auto marketplaceByMarketplace = mrkplconfigs.get_index<"bymarketplace"_n>();
            auto marketplace_iter         = marketplaceByMarketplace.find(marketplaceHash);

            fio_400_assert(marketplace_iter == marketplaceByMarketplace.end(), "marketplace", marketplace,
                           "Marketplace cannot be found",
                           ErrorNoWork);

            // verify the `actor` is the owner of the marketplace
            fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                           "Only owner of marketplace can modify config",
                           ErrorNoWork);

            // change the listing fee value
            marketplaceByMarketplace.modify(marketplace_iter, same_payer, [&](auto &row) {
                row.duration = listing_dur;
            });

            // verify the listing fee changed
            fio_400_assert(marketplace_iter->duration == listing_dur, "listing_dur", to_string(listing_dur),
                           "The table update failed.",
                           ErrorNoWork);

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }
        // setmkplstfee

        /***********
        * this action will simply update the marketplace warn time. By default this is 10 days.
        * @param
        */
        [[eosio::action]]
        void setmkpwrntm(const name &actor, const string &marketplace, const uint64_t &warn_days) {
            require_auth(actor);

            // sanity check of parameters
            eosio_assert(marketplace.length() >= 1, "Length of marketplace name should be 1 or more characters");


            uint128_t marketplaceHash = string_to_uint128_hash(marketplace.c_str());

            // find marketplace on the `mrkplconfig` table
            auto marketplaceByMarketplace = mrkplconfigs.get_index<"bymarketplace"_n>();
            auto marketplace_iter         = marketplaceByMarketplace.find(marketplaceHash);

            fio_400_assert(marketplace_iter == marketplaceByMarketplace.end(), "marketplace", marketplace,
                           "Marketplace cannot be found",
                           ErrorNoWork);

            eosio_assert(warn_days >= 1 && warn_days <= (marketplace_iter->duration / 2),
                         "Cannot warn sooner than half way through listing duration");

            // verify the `actor` is the owner of the marketplace
            fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                           "Only owner of marketplace can modify config",
                           ErrorNoWork);

            // change the listing fee value
            marketplaceByMarketplace.modify(marketplace_iter, same_payer, [&](auto &row) {
                row.warn_days = warn_days;
            });

            // verify the listing fee changed
            fio_400_assert(marketplace_iter->duration == warn_days, "warn_days", to_string(warn_days),
                           "The table update failed.",
                           ErrorNoWork);

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }
        // setmkplstfee

        /***********
        * This action will need to be run at least once a day and will check the listing table for any listings
         * that expire within X days (set in the config table). If a listing is expiring a FIO request will be sent
         * to the user that listed the domain. They can either let it expire or call the `rnlistdomain` (renew listing)
         * action. If the domain expires within 90 days they will not be able to renew the listing and must cancel
         * and then renew the domain and re-list.
         *
         * anyone can run this action, but it will need to be on an automated cronjob to run at least once a day or else
         * listings can get stale and be expired.
        */
        [[eosio::action]]
        void chkexplistng() {
            // TODO: 3/15/21

            // search the listing table and make sure that `fio_domain` is listed for sale

            // check that `actor` is  the one that listed the domain and their for the previous owner

            // retrieve the `warn_days` from the config table

            // check to see if the expiration date is within `warn_days` days (e.g. 5 days)

            // check if `warned_expire` is set to 1, if so and the expiration is passed,
            // then call cxlistdomain to transfer the domain back and cancel the listing

            // send a fio request to the account that listed the domain for sale with the memo that their listing will expire in `list_warn_time` days

            // mark the listing as `warned_expire`

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }
        // chkexplistng


    }; // class FioEscrow

    EOSIO_DISPATCH(FioEscrow, (listdomain)(cxlistdomain)(buydomain)
                              (rnlistdomain)(setmrkplcfg)(rmmrkplcfg)
                              (sethldacct)(setmkpcomfee)(setmkplstfee)
                              (setmkpwrntm)(setmkplstdur)(chkexplistng))
}