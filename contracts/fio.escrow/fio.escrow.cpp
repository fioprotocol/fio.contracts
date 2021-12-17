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
        domains_table      domains;
        eosio_names_table  accountmap;
        fiofee_table       fiofees;
    public:
        using contract::contract;

        FioEscrow(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                domainsales(_self, _self.value),
                mrkplconfigs(_self, _self.value),
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
                d.date_listed    = now();
                d.date_updated   = now();
                d.status         = 1; // status = 1: on sale, status = 2: Sold, status = 3; Cancelled
            });
            return id;
        }
        // listdomain_update

        /***********
        * This action will list a fio domain for sale. It also collects a fee for listing that is sent to the
         * marketplace. It transfers the domain ownership to `fio.escrow` while listed
         * @param actor this is the fio account that has sent this transaction.
         * @param fio_domain this is the fio domain to be sold.
         * @param sale_price this is the amount of FIO the seller wants for the domain
         * @param max_fee this is the max fee paid to the network for calling this action
         * @param tpid this is the technology provider id that helped facilitate this action to the end user
        */
        [[eosio::action]]
        void listdomain(const name &actor, const string &fio_domain,
                        const uint64_t &sale_price, const int64_t &max_fee,
                        const string &tpid) {
            require_auth(actor);

            fio_400_assert(sale_price >= 1000000000, "sale_price", std::to_string(sale_price),
                           "Sale price should be greater than 1 FIO (1,000,000,000 SUF)", ErrorInvalidAmount);
            fio_400_assert(sale_price <= 999999000000000, "sale_price", std::to_string(sale_price),
                           "Sale price should be less than 999,999 FIO (999,999,000,000,000 SUF)", ErrorInvalidAmount);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            auto marketplace_iter = mrkplconfigs.begin();

            fio_400_assert(marketplace_iter != mrkplconfigs.end(), "marketplace_iter", "marketplace_iter",
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

            action(permission_level{get_self(), "active"_n},
                   TokenContract, "transfer"_n,
                   make_tuple(actor, marketplaceAccount, listingfee, string("Listing fee"))
            ).send();

            // hash domain for easy querying
            const uint128_t domainHash    = string_to_uint128_hash(fio_domain.c_str());
            // verify domain exists in the fio_domain table
            auto            domainsbyname = domains.get_index<"byname"_n>();
            auto            domains_iter  = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter != domainsbyname.end(), "fio_domain", fio_domain,
                           "FIO domain not found", ErrorDomainNotRegistered);

            // check that the `actor` owners `fio_domain`
            fio_400_assert(domains_iter->account == actor.value, "fio_domain", fio_domain,
                           "FIO domain not owned by actor", ErrorDomainOwner);

            bool isTransferToEscrow = true;

            action(
                    permission_level{EscrowContract, "active"_n},
                    AddressContract,
                    "xferescrow"_n,
                    std::make_tuple(fio_domain, nullptr, isTransferToEscrow, actor)
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
                                                  "\"domainsale_id\":") + to_string(domainsale_id) +
                                           string(",\"fee_collected\":") + to_string(fee_amount) + string("}");

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size",
                           std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }
        // listdomain

        /***********
        * This action will cancel a listing a fio domain
         * @param actor this is the account name that listed the domain
         * @param fio_domain this is the name of the fio_domain
         * @param max_fee this is the max fee paid to the network for calling this action
         * @param tpid this is the technology provider id that helped facilitate this action to the end user
        */
        [[eosio::action]]
        void cxlistdomain(const name &actor, const string &fio_domain,
                          const int64_t &max_fee, const string &tpid) {
            check(has_auth(actor) || has_auth(EscrowContract), "Permission Denied");

            auto marketplace_iter = mrkplconfigs.begin();

            fio_400_assert(marketplace_iter != mrkplconfigs.end(), "marketplace_iter", "marketplace_iter",
                           "Marketplace not found", ErrorDomainOwner);

            fio_400_assert(marketplace_iter->e_break == 0, "marketplace_iter->e_break",
                           to_string(marketplace_iter->e_break),
                           "E-Break Enabled, action disabled", ErrorNoWork);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());

            auto domainsalesbydomain = domainsales.get_index<"bydomain"_n>();
            auto domainsale_iter     = domainsalesbydomain.find(domainHash);
            fio_400_assert(domainsale_iter != domainsalesbydomain.end(), "domainsale", fio_domain,
                           "Domain not found", ErrorDomainSaleNotFound);

            fio_400_assert(domainsale_iter->owner == actor.value, "actor", actor.to_string(),
                           "Only owner of domain may cancel listing", ErrorNoWork);

            fio_400_assert(domainsale_iter->status == 1, "status", to_string(domainsale_iter->status),
                           "domain has already been bought or cancelled", ErrorNoWork);

            domainsalesbydomain.modify(domainsale_iter, EscrowContract, [&](auto &row) {
                row.status       = 3; // status = 1: on sale, status = 2: Sold, status = 3; Cancelled
                row.date_updated = now();
            });

            const bool accountExists = is_account(actor);

            auto owner = accountmap.find(actor.value);

            fio_400_assert(owner != accountmap.end(), "owner_account", actor.to_string(),
                           "Account is not bound on the fio chain",
                           ErrorPubAddressExist);
            fio_400_assert(accountExists, "owner_account", actor.to_string(),
                           "Account does not yet exist on the fio chain",
                           ErrorPubAddressExist);

            bool isTransferToEscrow = false;

            action(
                    permission_level{EscrowContract, "active"_n},
                    AddressContract,
                    "xferescrow"_n,
                    std::make_tuple(fio_domain, owner->clientkey, isTransferToEscrow, actor)
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
         * @param sale_id this is the sale_id of the fio domain. This is used to make sure it's the correct
         * @param fio_domain this is the name of the fio_domain
         * @param max_buy_price this is the max buy price the buyer is willing to pay.
         * @param max_fee this is the max fee paid to the network for calling this action
         * @param tpid this is the technology provider id that helped facilitate this action to the end user
        */
        [[eosio::action]]
        void buydomain(const name &actor, const int64_t &sale_id,
                       const string &fio_domain, const int64_t &max_buy_price,
                       const int64_t &max_fee, const string &tpid) {

            require_auth(actor);

            auto marketplace_iter = mrkplconfigs.begin();
            fio_400_assert(marketplace_iter != mrkplconfigs.end(), "marketplace_iter", "marketplace_iter",
                           "Marketplace not found", ErrorDomainOwner);

            fio_400_assert(marketplace_iter->e_break == 0, "marketplace_iter->e_break",
                           to_string(marketplace_iter->e_break),
                           "E-Break Enabled, action disabled", ErrorNoWork);

            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());

            auto domainsalesbydomain = domainsales.get_index<"bydomain"_n>();
            auto domainsale_iter     = domainsalesbydomain.find(domainHash);
            fio_400_assert(domainsale_iter != domainsalesbydomain.end(), "domainsale", fio_domain,
                           "Domain not found", ErrorDomainSaleNotFound);

            fio_400_assert(domainsale_iter->status == 1, "status", to_string(domainsale_iter->status),
                           "domain has already been bought or cancelled", ErrorNoWork);

            fio_400_assert(domainsale_iter->id == sale_id, "sale_id", to_string(sale_id),
                           "Sale ID does not match", ErrorDomainSaleNotFound);

            auto saleprice           = asset(domainsale_iter->sale_price, FIOSYMBOL);
            auto buyer_max_buy_price = asset(max_buy_price, FIOSYMBOL);
            fio_400_assert(buyer_max_buy_price.amount <= saleprice.amount, "saleprice", to_string(saleprice.amount),
                           "Sale Price is greater than submitted buyer max buy price", ErrorNoWork);

            auto marketCommissionFee = domainsale_iter->commission_fee / 100.0;
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

            if (marketCommission.amount > 0) {
                action(permission_level{EscrowContract, "active"_n},
                       TokenContract, "transfer"_n,
                       make_tuple(actor, marketplace_iter->owner, marketCommission, string("Marketplace Commission"))
                ).send();
            }

            bool isTransferToEscrow = false;

            action(
                    permission_level{EscrowContract, "active"_n},
                    AddressContract,
                    "xferescrow"_n,
                    std::make_tuple(fio_domain, buyerAcct->clientkey, isTransferToEscrow, actor)
            ).send();

//            domainsalesbydomain.erase(domainsale_iter);
            domainsalesbydomain.modify(domainsale_iter, EscrowContract, [&](auto &row) {
                row.status       = 2; // status = 1: on sale, status = 2: Sold, status = 3; Cancelled
                row.date_updated = now();
            });

            domainsale_iter = domainsalesbydomain.find(domainHash);

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
                           "Fee exceeds supplied maximum. " + to_string(fee_amount),
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
        * this action is to set the config for a marketplace.
        * @param
        */
        [[eosio::action]]
        void setmrkplcfg(const name &actor, const double &commission_fee, const uint64_t &listing_fee,
                         const uint64_t &e_break, const int64_t &max_fee) {

            bool isMsig;

            if (has_auth(SYSTEMACCOUNT) || has_auth(EscrowContract)) {
                isMsig = true;
            } else {
                require_auth(actor);
                isMsig = false;
            }

            fio_400_assert(actor.length() == 12,
                           "actor", actor.to_string(),
                           "Length of account name should be 12", ErrorNoWork);

            // sanity check of parameters
            fio_400_assert(commission_fee <= 25,
                           "commission_fee", to_string(commission_fee),
                           "Commission fee should be between 0 and 25", ErrorNoWork);

            fio_400_assert(listing_fee <= 25000000000,
                           "listing_fee", to_string(listing_fee),
                           "Listing fee should be between 0 and 25,000,000,000 (25 FIO in SUF)", ErrorNoWork);

            fio_400_assert(e_break >= 0 && e_break <= 1,
                           "e_break", to_string(e_break),
                           "E-break setting must be either 0 for disabled or 1 for enabled", ErrorNoWork);

            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            const bool accountExists = is_account(actor);
            auto       acctmap_itr   = accountmap.find(actor.value);

            fio_400_assert(acctmap_itr != accountmap.end(), "actor", actor.to_string(),
                           "Account is not bound on the fio chain",
                           ErrorPubAddressExist);
            fio_400_assert(accountExists, "actor", actor.to_string(),
                           "Account does not yet exist on the fio chain",
                           ErrorPubAddressExist);

            auto marketplace_iter = mrkplconfigs.begin();

            uint64_t  id        = mrkplconfigs.available_primary_key();
            uint128_t ownerHash = string_to_uint128_hash(actor.to_string());

            if (marketplace_iter == mrkplconfigs.end()) {
                // not found, emplace
                // if setting the record for the first time, it must be by msig
                eosio_assert((has_auth(SYSTEMACCOUNT) || has_auth(EscrowContract)),
                             "missing required authority of eosio or fio.escrow");
                mrkplconfigs.emplace(EscrowContract, [&](auto &row) {
                    row.id             = id;
                    row.owner          = actor.value;
                    row.ownerhash      = ownerHash;
                    row.commission_fee = commission_fee;
                    row.listing_fee    = listing_fee;
                    row.e_break        = e_break;
                });
            } else {
                // verify the `actor` is the owner of the marketplace
                fio_400_assert(marketplace_iter->owner == actor.value, "actor", actor.to_string(),
                               "Only owner of marketplace can modify config", ErrorNoWork);

                // found, modify
                mrkplconfigs.modify(marketplace_iter, EscrowContract, [&](auto &row) {
                    row.commission_fee = commission_fee;
                    row.listing_fee    = listing_fee;
                    row.e_break        = e_break;
                });
            }

            // charge fee if it isn't an msig
            if (!isMsig) {

                //fees
                const uint128_t endpoint_hash = string_to_uint128_hash(SET_MARKETPLACE_CONFIG_ENDPOINT);

                auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
                auto fee_iter         = fees_by_endpoint.find(endpoint_hash);
                fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", SET_MARKETPLACE_CONFIG_ENDPOINT,
                               "FIO fee not found for endpoint", ErrorNoEndpoint);

                const uint64_t fee_amount = fee_iter->suf_amount;
                const uint64_t fee_type   = fee_iter->type;

                fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                               "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                               ErrorNoEndpoint);

                fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                               "Fee exceeds supplied maximum.",
                               ErrorMaxFeeExceeded);

                fio_fees(actor, asset(fee_amount, FIOSYMBOL), SET_MARKETPLACE_CONFIG_ENDPOINT);
                processbucketrewards("", fee_amount, get_self(), actor);

                if (FIOESCROWRAM > 0) {
                    action(
                            permission_level{SYSTEMACCOUNT, "active"_n},
                            "eosio"_n,
                            "incram"_n,
                            std::make_tuple(actor, FIOESCROWRAM)
                    ).send();
                }
            }

            // if tx is too large, throw an error.
            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size",
                           std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());

        } // setmrkplcfg

        /*
         * This is an admin action only called from burnexpired in fio.address to cancel the listing if the listed
         * domain is expired and being burned.
         */
        [[eosio::action]]
        void cxburned(const uint128_t &domainhash){
            // make sure it's from the address contract
            has_auth(AddressContract);

            auto domainsalesbydomain = domainsales.get_index<"bydomain"_n>();
            auto domainsale_iter     = domainsalesbydomain.find(domainhash);

            if(domainsale_iter->status == 1) {
                domainsalesbydomain.modify(domainsale_iter, EscrowContract, [&](auto &row) {
                    row.status       = 3; // status = 1: on sale, status = 2: Sold, status = 3; Cancelled
                    row.date_updated = now();
                });
            }
        }
    }; // class FioEscrow

    EOSIO_DISPATCH(FioEscrow, (listdomain)(cxlistdomain)(buydomain)(setmrkplcfg)(cxburned))
}