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
        fiofee_table       fiofees;
    public:
        using contract::contract;

        FioEscrow(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                domainsales(_self, _self.value),
                mrkplconfigs(_self, _self.value),
                holderaccts(_self, _self.value),
                domains(AddressContract, AddressContract.value),
                fiofees(FeeContract, FeeContract.value),
                accountmap(AddressContract, AddressContract.value) {}

        uint32_t listdomain_update(const name &actor, const string &fio_domain,
                                   const uint128_t &domainhash, const uint64_t &sale_price,
                                   const uint64_t &commission_fee) {

            uint128_t ownerHash = string_to_uint128_hash(actor.to_string());
            uint64_t  id        = domainsales.available_primary_key();
            domainsales.emplace(actor, [&](struct domainsale &d) {
                d.id             = id;
                d.owner          = actor.value;
                d.ownerhash      = ownerHash;
                d.domain         = fio_domain;
                d.domainhash     = domainhash;
                d.sale_price     = sale_price;
                d.commission_fee = commission_fee;
            });
            return id;
        }
        // listdomain_update

        /***********
        * This action will list a fio domain for sale. It also collects a fee for listing that is sent to the
         * marketplace. It transfers the domain ownership to a holderaccount that is set in the holder account table
        * @param actor this is the fio account that has sent this transaction.
        * @param fio_domain this is the fio domain to be sold.
        * @param sale_price this is the amount of FIO the seller wants for the domain
        */
        [[eosio::action]]
        void listdomain(const name &actor, const string &fio_domain,
                        const uint64_t &sale_price, const uint64_t &max_fee,
                        const string &tpid) {
            require_auth(actor);

            fio_400_assert(sale_price >= 1000000000, "sale_price", std::to_string(sale_price),
                           "Sale price should be greater than 1 FIO (1,000,000,000 SUF)", ErrorInvalidAmount);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            mrkplconfigs_table marketplaceTable(_self, _self.value);
            auto               marketplace_iter = marketplaceTable.find(0); // only 1 marketplace, use 0th index

            fio_400_assert(marketplace_iter != marketplaceTable.end(), "marketplace_iter", "marketplace_iter",
                           "Marketplace not found", ErrorDomainOwner);

            fio_400_assert(marketplace_iter->e_break == 0, "marketplace_iter->e_break",
                           to_string(marketplace_iter->e_break),
                           "E-Break Enabled, action disabled", ErrorNoWork);

            // found
            auto listingfee         = asset(marketplace_iter->listing_fee, FIOSYMBOL);
            auto marketplaceAccount = name(marketplace_iter->owner);

            const bool accountExists = is_account(marketplaceAccount);
            auto       acctmap_itr   = accountmap.find(marketplaceAccount.value);

            fio_400_assert(acctmap_itr != accountmap.end(), "acctmap_itr", marketplaceAccount.to_string(),
                           "Account not found", ErrorNoWork);

            action(permission_level{EscrowContract, "active"_n},
                   TokenContract, "transfer"_n,
                   make_tuple(actor, marketplaceAccount, listingfee, string("Listing fee"))
            ).send();

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

            fio_400_assert(hold_account_itr != table.end(), "hold_account_itr", "hold_account_itr",
                           "Holder account private key not found", ErrorDomainOwner);

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
                                                   sale_price, marketplace_iter->commission_fee);

            const uint128_t endpoint_hash = string_to_uint128_hash(LIST_DOMAIN_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter         = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", LIST_DOMAIN_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type   = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), LIST_DOMAIN_ENDPOINT);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (FIOESCROWRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, FIOESCROWRAM)
                ).send();
            }

            const string response_string = string("{\"status\": \"OK\","
                                                  "\"domainsale_id\":\"") + to_string(domainsale_id) +
                                           string(",\"fee_collected\":") + to_string(fee_amount) + string("}");

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size",
                           std::to_string(transaction_size()),
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
        void cxlistdomain(const name &actor, const string &fio_domain,
                          const uint64_t &max_fee, const string &tpid) {
            check(has_auth(actor) || has_auth(EscrowContract), "Permission Denied");

            mrkplconfigs_table marketplaceTable(_self, _self.value);
            auto               marketplace_iter = marketplaceTable.find(0); // only 1 marketplace, use 0th index

            fio_400_assert(marketplace_iter != marketplaceTable.end(), "marketplace_iter", "marketplace_iter",
                           "Marketplace not found", ErrorDomainOwner);

            fio_400_assert(marketplace_iter->e_break == 0, "marketplace_iter->e_break",
                           to_string(marketplace_iter->e_break),
                           "E-Break Enabled, action disabled", ErrorNoWork);

            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());

            auto domainsalesbydomain = domainsales.get_index<"bydomain"_n>();
            auto domainsale_iter     = domainsalesbydomain.find(domainHash);
            fio_400_assert(domainsale_iter != domainsalesbydomain.end(), "domainsale", fio_domain,
                           "Domain not found", ErrorDomainSaleNotFound);

            fio_400_assert(domainsale_iter->owner == actor.value, "actor", actor.to_string(),
                           "Only owner of domain may cancel listing", ErrorNoWork);

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

            const uint128_t endpoint_hash = string_to_uint128_hash(CANCEL_LIST_DOMAIN_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter         = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", CANCEL_LIST_DOMAIN_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type   = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), CANCEL_LIST_DOMAIN_ENDPOINT);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (FIOESCROWRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, FIOESCROWRAM)
                ).send();
            }

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size",
                           std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }
        // cxlistdomain

        /***********
        * This action will list a fio domain for sale
        * @param actor this is the account name that listed the domain
        * @param this is the name of the fio_domain
        */
        [[eosio::action]]
        void buydomain(const name &actor, const int64_t &sale_id,
                       const string &fio_domain, const int64_t &buy_price,
                       const int64_t &max_fee, const string &tpid) {

            require_auth(actor);

            mrkplconfigs_table marketplaceTable(_self, _self.value);
            auto               marketplace_iter = marketplaceTable.find(0); // only 1 marketplace, use 0th index
            fio_400_assert(marketplace_iter != marketplaceTable.end(), "marketplace_iter", "marketplace_iter",
                           "Marketplace not found", ErrorDomainOwner);

            fio_400_assert(marketplace_iter->e_break == 0, "marketplace_iter->e_break",
                           to_string(marketplace_iter->e_break),
                           "E-Break Enabled, action disabled", ErrorNoWork);

            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());

            auto domainsalesbydomain = domainsales.get_index<"bydomain"_n>();
            auto domainsale_iter     = domainsalesbydomain.find(domainHash);
            fio_400_assert(domainsale_iter != domainsalesbydomain.end(), "domainsale", fio_domain,
                           "Domain not found", ErrorDomainSaleNotFound);

            auto saleprice           = asset(domainsale_iter->sale_price, FIOSYMBOL);
            auto marketCommissionFee = marketplace_iter->commission_fee / 100.0;
            auto marketCommission    = (marketCommissionFee > 0) ?
                                       asset(saleprice.amount * marketCommissionFee, FIOSYMBOL) :
                                       asset(0, FIOSYMBOL);

            auto toBuyer = asset(saleprice.amount - marketCommission.amount, FIOSYMBOL);

            const bool accountExists = is_account(actor);
            auto       buyerAcct     = accountmap.find(actor.value);
            fio_400_assert(buyerAcct != accountmap.end(), "actor", actor.to_string(),
                           "Account is not bound on the fio chain",
                           ErrorPubAddressExist);
            fio_400_assert(accountExists, "actor", actor.to_string(),
                           "Account does not yet exist on the fio chain",
                           ErrorPubAddressExist);

            action(permission_level{EscrowContract, "active"_n},
                   TokenContract, "transfer"_n,
                   make_tuple(actor, domainsale_iter->owner, toBuyer, string("Domain Purchase"))
            ).send();

            action(permission_level{EscrowContract, "active"_n},
                   TokenContract, "transfer"_n,
                   make_tuple(actor, marketplace_iter->owner, marketCommission, string("Marketplace Commission"))
            ).send();

            // transfer the domain to holder account
            action(
                    permission_level{EscrowContract, "active"_n},
                    AddressContract,
                    "xferescrow"_n,
                    std::make_tuple(fio_domain, buyerAcct->clientkey, actor)
            ).send();

            domainsalesbydomain.erase(domainsale_iter);

            domainsale_iter = domainsalesbydomain.find(domainHash);
            fio_400_assert(domainsale_iter == domainsalesbydomain.end(), "domainsale", fio_domain,
                           "Domain listing not removed properly", ErrorDomainSaleNotFound);

            const uint128_t endpoint_hash = string_to_uint128_hash(LIST_DOMAIN_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter         = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", LIST_DOMAIN_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type   = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), BUY_DOMAIN_ENDPOINT);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (FIOESCROWRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, FIOESCROWRAM)
                ).send();
            }

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size",
                           std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            send_response(response_string.c_str());

        }
        // buydomain

        /***********
        * this action can only be called by the system contract or fio.escrow and it sets
        * the pubkey that will be the trusted owner of domains/addresses when they are listed on sale.
        * @param
        */
        [[eosio::action]]
        void sethldacct(const string &public_key, const int64_t &max_fee,
                        const string &tpid, const name &actor) {
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
                //  domains/addresses the old pubkey owns to this new pubkey. Or should this be
                //  removed because the holder account key should never need to be changed
                table.modify(hold_account_itr, same_payer, [&](auto &row) {
                    row.holder_public_key = public_key.c_str();
                });
            }

            const uint128_t endpoint_hash = string_to_uint128_hash(SET_HOLDER_ACCOUNT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter         = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", SET_HOLDER_ACCOUNT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type   = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), SET_HOLDER_ACCOUNT);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (FIOESCROWRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, FIOESCROWRAM)
                ).send();
            }

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size",
                           std::to_string(transaction_size()),
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
                         const uint64_t &listingfee, const uint64_t &e_break, const int64_t &max_fee,
                         const string &tpid, const name &actor) {
            require_auth(owner);

            fio_400_assert(isPubKeyValid(owner_public_key), "owner_public_key", owner_public_key,
                           "Invalid FIO Public Key", ErrorPubKeyValid);

            fio_400_assert(owner.length() == 12,
                           "owner", owner.to_string(),
                           "Length of account name should be 12", ErrorNoWork);
            fio_400_assert(commissionfee <= 25,
                           "commissionfee", to_string(commissionfee),
                           "Commission fee should be between 0 and 25", ErrorNoWork);
            fio_400_assert(listingfee <= 25000000000,
                           "listingfee", to_string(listingfee),
                           "Listing fee should be between 0 and 25,000,000,000 (25 FIO in SUF)", ErrorNoWork);

            const bool accountExists = is_account(owner);

            auto acctmap_itr = accountmap.find(owner.value);

            fio_400_assert(acctmap_itr != accountmap.end(), "owner", owner.to_string(),
                           "Account is not bound on the fio chain",
                           ErrorPubAddressExist);
            fio_400_assert(accountExists, "owner", owner.to_string(),
                           "Account does not yet exist on the fio chain",
                           ErrorPubAddressExist);

            uint128_t marketplaceHash           = string_to_uint128_hash(marketplace.c_str());

            mrkplconfigs_table marketplaceTable(_self, _self.value);
            auto               marketplace_iter = marketplaceTable.find(0); // only 1 marketplace, use 0th index

            uint64_t  id        = mrkplconfigs.available_primary_key();
            uint128_t ownerHash = string_to_uint128_hash(owner.to_string());

            if (marketplace_iter == marketplaceTable.end()) {
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
                    row.e_break          = e_break;
                });
            } else {
                fio_400_assert(marketplace_iter == marketplaceTable.end(), "marketplace", marketplace,
                               "Marketplace already exists. Use the set actions for the individual settings.",
                               ErrorNoWork);
            }

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size",
                           std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const uint128_t endpoint_hash = string_to_uint128_hash(SET_MARKETPLACE_CONFIG);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter         = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", SET_MARKETPLACE_CONFIG,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type   = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), SET_MARKETPLACE_CONFIG);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (FIOESCROWRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, FIOESCROWRAM)
                ).send();
            }

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

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
        void rmmrkplcfg(const name &actor, const string &marketplace,
                        const int64_t &max_fee, const string &tpid) {
            require_auth(actor);

            eosio_assert(marketplace.length() >= 1, "Length of marketplace name should be between 1 and 25 characters");

            mrkplconfigs_table marketplaceTable(_self, _self.value);
            auto               marketplace_iter = marketplaceTable.find(0); // only 1 marketplace, use 0th index

            fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                           "Only owner of marketplace can modify config", ErrorNoWork);

            fio_400_assert(marketplace_iter != marketplaceTable.end(), "marketplace", marketplace,
                           "Marketplace cannot be found",
                           ErrorNoWork);

            fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                           "Actor must own marketplace",
                           ErrorNoWork);

            fio_400_assert(marketplace_iter->e_break == 0, "marketplace_iter->e_break",
                           to_string(marketplace_iter->e_break),
                           "E-Break Enabled, action disabled", ErrorNoWork);

            marketplaceTable.erase(marketplace_iter);

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size",
                           std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const uint128_t endpoint_hash = string_to_uint128_hash(REMOVE_MARKETPLACE_CONFIG);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter         = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", REMOVE_MARKETPLACE_CONFIG,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type   = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), REMOVE_MARKETPLACE_CONFIG);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (FIOESCROWRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, FIOESCROWRAM)
                ).send();
            }

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

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
        void setmkpcomfee(const name &actor, const uint64_t &fee,
                          const int64_t &max_fee, const string &tpid) {
            require_auth(actor);

            // sanity check of parameters
            fio_400_assert(fee <= 25,
                           "fee", to_string(fee),
                           "Commission fee should be between 0 and 25", ErrorNoWork);

            mrkplconfigs_table marketplaceTable(_self, _self.value);
            auto               marketplace_iter = marketplaceTable.find(0); // only 1 marketplace, use 0th index

            fio_400_assert(marketplace_iter != marketplaceTable.end(), "marketplace", "marketplace",
                           "Marketplace cannot be found",
                           ErrorNoWork);

            fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                           "Actor must own marketplace",
                           ErrorNoWork);

            fio_400_assert(marketplace_iter->e_break == 0, "marketplace_iter->e_break",
                           to_string(marketplace_iter->e_break),
                           "E-Break Enabled, action disabled", ErrorNoWork);

            // verify the `actor` is the owner of the marketplace
            fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                           "Only owner of marketplace can modify config",
                           ErrorNoWork);

            // change the commission fee value
            marketplaceTable.modify(marketplace_iter, actor, [&](auto &row) {
                row.commission_fee = fee;
            });

            // verify the listing fee changed
            fio_400_assert(marketplace_iter->commission_fee == fee, "fee", to_string(fee),
                           "The table update failed.",
                           ErrorNoWork);

            const uint128_t endpoint_hash = string_to_uint128_hash(SET_MARKETPLACE_COMMISSION_FEE);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter         = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", SET_MARKETPLACE_COMMISSION_FEE,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type   = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), SET_MARKETPLACE_COMMISSION_FEE);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (FIOESCROWRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, FIOESCROWRAM)
                ).send();
            }

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size",
                           std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            send_response(response_string.c_str());
        }
        // setmkpcomfee

        /***********
        * this action will simply update the marketplace listing fee that is charged up front. Default is 4 FIO
        * @param
        */
        [[eosio::action]]
        void setmkplstfee(const name &actor, const uint64_t &fee,
                          const int64_t &max_fee, const string &tpid) {
            require_auth(actor);

            // sanity check of parameters
            fio_400_assert(fee <= 25000000000,
                           "fee", to_string(fee),
                           "Listing fee should be between 0 and 25,000,000,000 (25 FIO in SUF)", ErrorNoWork);

            mrkplconfigs_table marketplaceTable(_self, _self.value);
            auto               marketplace_iter = marketplaceTable.find(0); // only 1 marketplace, use 0th index

            fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                           "Actor must own marketplace",
                           ErrorNoWork);

            fio_400_assert(marketplace_iter != marketplaceTable.end(), "marketplace", "marketplace",
                           "Marketplace cannot be found",
                           ErrorNoWork);

            fio_400_assert(marketplace_iter->e_break == 0, "marketplace_iter->e_break",
                           to_string(marketplace_iter->e_break),
                           "E-Break Enabled, action disabled", ErrorNoWork);

            // verify the `actor` is the owner of the marketplace
            fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                           "Only owner of marketplace can modify config",
                           ErrorNoWork);

            // change the listing fee value
            marketplaceTable.modify(marketplace_iter, actor, [&](auto &row) {
                row.listing_fee = fee;
            });

            // verify the listing fee changed
            fio_400_assert(marketplace_iter->listing_fee == fee, "fee", to_string(fee),
                           "The table update failed.",
                           ErrorNoWork);

            const uint128_t endpoint_hash = string_to_uint128_hash(SET_MARKETPLACE_LISTING_FEE);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter         = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", SET_MARKETPLACE_LISTING_FEE,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type   = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), SET_MARKETPLACE_LISTING_FEE);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (FIOESCROWRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, FIOESCROWRAM)
                ).send();
            }

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size",
                           std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            send_response(response_string.c_str());
        }
        // setmkplstfee

        /***********
        * this action will change the e-break setting
        * @param
        */
        [[eosio::action]]
        void setmkpebreak(const name &actor, const int64_t &max_fee, const string &tpid) {
            require_auth(actor);

            // sanity check of parameters

            mrkplconfigs_table marketplaceTable(_self, _self.value);
            auto               marketplace_iter = marketplaceTable.find(0); // only 1 marketplace, use 0th index

            fio_400_assert(marketplace_iter != marketplaceTable.end(), "marketplace", "marketplace",
                           "Marketplace cannot be found",
                           ErrorNoWork);

            // verify the `actor` is the owner of the marketplace
            fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                           "Only owner of marketplace can modify config",
                           ErrorNoWork);

            int64_t newVal = 0;

            (marketplace_iter->e_break > 0) ? newVal = 0 : newVal = 1;

            // change the listing fee value
            marketplaceTable.modify(marketplace_iter, actor, [&](auto &row) {
                row.e_break = newVal;
            });

            // verify the listing fee changed
            fio_400_assert(marketplace_iter->e_break == newVal, "newVal", to_string(newVal),
                           "The table update failed.",
                           ErrorNoWork);

            const uint128_t endpoint_hash = string_to_uint128_hash(SET_MARKETPLACE_E_BREAK);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter         = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", SET_MARKETPLACE_E_BREAK,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type   = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), SET_MARKETPLACE_E_BREAK);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (FIOESCROWRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, FIOESCROWRAM)
                ).send();
            }

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size",
                           std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            send_response(response_string.c_str());
        }
        // setmkpebreak
    }; // class FioEscrow

    EOSIO_DISPATCH(FioEscrow, (listdomain)(cxlistdomain)(buydomain)
                              (setmrkplcfg)(rmmrkplcfg)(sethldacct)
                              (setmkpcomfee)(setmkplstfee)(setmkpebreak))
}