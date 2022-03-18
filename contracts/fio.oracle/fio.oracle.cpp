/** Fio Oracle implementation file
 *  Description:
 *  @author Casey Gardiner
 *  @modifedby
 *  @file fio.oracle.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE )
 */

#include <eosiolib/asset.hpp>
#include "fio.oracle.hpp"
#include <fio.fee/fio.fee.hpp>
#include <fio.address/fio.address.hpp>
#include <fio.common/fiotime.hpp>
#include <fio.common/fio.common.hpp>
#include <fio.common/fioerror.hpp>

namespace fioio {

    class [[eosio::contract("FIOOracle")]]  FIOOracle : public eosio::contract {

    private:
        oracleledger_table receipts;
        oraclevoters_table voters;
        oracles_table oracles;
        fionames_table fionames;
        domains_table domains;
        eosiosystem::producers_table producers;
        eosio_names_table accountmap;
        fiofee_table fiofees;
        config appConfig;
    public:
        using contract::contract;

        FIOOracle(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                receipts(_self, _self.value),
                voters(_self, _self.value),
                domains(AddressContract, AddressContract.value),
                oracles(_self, _self.value),
                producers(SYSTEMACCOUNT, SYSTEMACCOUNT.value),
                accountmap(AddressContract, AddressContract.value),
                fiofees(FeeContract, FeeContract.value),
                fionames(AddressContract, AddressContract.value) {
            configs_singleton configsSingleton(FeeContract, FeeContract.value);
            appConfig = configsSingleton.get_or_default(config());
        }

        [[eosio::action]]
        void wraptokens(int64_t &amount, string &chain_code, string &public_address, int64_t &max_oracle_fee,
                        int64_t &max_fee, string &tpid, name &actor) {
            require_auth(actor);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);
            fio_400_assert(public_address.length() > 0, "public_address", public_address,
                           "Invalid public address", ErrorInvalidFioNameFormat);
            fio_400_assert(validateChainNameFormat(chain_code), "chain_code", chain_code, "Invalid chain code format",
                           ErrorInvalidFioNameFormat);
            fio_400_assert(max_oracle_fee >= 0, "max_oracle_fee", to_string(max_oracle_fee), "Invalid oracle fee value",
                           ErrorMaxFeeInvalid);

            uint8_t oracle_size = std::distance(oracles.cbegin(), oracles.cend());
            fio_400_assert(3 <= oracle_size, "actor", actor.to_string(), "Not enough registered oracles.",
                           ErrorMaxFeeInvalid);

            //force uppercase chain code
            std::transform(chain_code.begin(), chain_code.end(),chain_code.begin(), ::toupper);

            //max amount?
            fio_400_assert(amount >= 0, "amount", to_string(amount), "Invalid amount",
                           ErrorMaxFeeInvalid);
            const uint32_t present_time = now();

            //Oracle fee is transferred from actor account to all registered oracles in even amount.
            auto idx = oracles.begin();
            fio_400_assert(idx != oracles.end(), "max_oracle_fee", to_string(max_oracle_fee), "No Oracles registered or fees set",
                           ErrorMaxFeeInvalid);

            int index = 0;
            vector<uint64_t> totalfees;
            uint64_t feeFinal;

            while( idx != oracles.end() ){
                uint64_t tempfee = idx->fees[1].fee_amount;
                totalfees.push_back(tempfee);
                idx++;
            }

            fio_400_assert(totalfees.size() == oracle_size, "max_oracle_fee", to_string(max_oracle_fee), "Not all oracles have voted for fees",
                           ErrorMaxFeeInvalid);

            // median fee / oracle_info.size = fee paid
            sort(totalfees.begin(), totalfees.end());
            if (oracle_size % 2 == 0) {
                feeFinal = (totalfees[oracle_size / 2 - 1] + totalfees[oracle_size / 2]) / 2;
            } else {
                feeFinal = totalfees[oracle_size / 2];
            }

            idx = oracles.begin();
            uint64_t feeTotal = feeFinal * oracle_size;
            fio_400_assert(max_oracle_fee >= feeTotal, "max_oracle_fee", to_string(max_oracle_fee), "Invalid oracle fee value",
                           ErrorMaxFeeInvalid);

            while( idx != oracles.end() ){
                action(permission_level{get_self(), "active"_n},
                       TokenContract, "transfer"_n,
                       make_tuple(actor, name{idx->actor}, asset(feeFinal, FIOSYMBOL), string("Token Wrapping Oracle Fee"))
                ).send();

                idx++;
            }

            //Copy information to receipt table
            receipts.emplace(actor, [&](struct oracleledger &p) {
                p.id = receipts.available_primary_key();
                p.actor = actor.value;
                p.chaincode = chain_code;
                p.pubaddress = public_address;
                p.amount = amount;
                p.timestamp = present_time;
            });

            //Tokens are transferred to fio.wrapping.
            action(permission_level{get_self(), "active"_n},
                   TokenContract, "transfer"_n,
                   make_tuple(actor, FIOORACLEContract, asset(amount, FIOSYMBOL), string("Token Wrapping"))
            ).send();

            //Chain wrap_fio_token fee is collected.
            const uint128_t endpoint_hash = string_to_uint128_hash(WRAP_FIO_TOKENS_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);
            const uint64_t fee_type = fee_iter->type;
            const int64_t wrap_amount = fee_iter->suf_amount;

            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", WRAP_FIO_TOKENS_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            uint64_t fee_amount = fee_iter->suf_amount;
            fio_400_assert(max_fee >= (int64_t)fee_amount, "max_fee", to_string(max_fee), "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(wrap_amount, FIOSYMBOL), WRAP_FIO_TOKENS_ENDPOINT);
            process_rewards(tpid, wrap_amount,get_self(), actor);

            //RAM of signer is increased (512) more?
            action(
                    permission_level{SYSTEMACCOUNT, "active"_n},
                    "eosio"_n,
                    "incram"_n,
                    std::make_tuple(actor, WRAPTOKENRAM)
            ).send();

            const string response_string = string("{\"status\": \"OK\",\"oracle_fee_collected\":\"") +
                                           to_string(feeTotal) + string("\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void unwraptokens(int64_t &amount, string &obt_id, string &fio_address, name &actor) {
            require_auth(actor);
            //max amount would go here
            fio_400_assert(amount >= 0, "amount", to_string(amount), "Invalid amount",
                           ErrorMaxFeeInvalid);

            FioAddress fa;
            getFioAddressStruct(fio_address, fa);
            fio_400_assert(validateFioNameFormat(fa), "fio_address", fa.fioaddress, "Invalid FIO Address",
                           ErrorDomainAlreadyRegistered);

            uint8_t oracle_size = std::distance(oracles.cbegin(), oracles.cend());
            fio_400_assert(3 <= oracle_size, "actor", actor.to_string(), "Not enough registered oracles.",
                           ErrorMaxFeeInvalid);

            auto oraclesearch = oracles.find(actor.value);
            fio_400_assert(oraclesearch != oracles.end(), "actor", actor.to_string(),
                           "Not a registered Oracle", ErrorPubAddressExist);

            const uint128_t nameHash = string_to_uint128_hash(fio_address);
            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);

            const uint128_t idHash = string_to_uint128_hash(obt_id);
            auto votesbyid = voters.get_index<"byidhash"_n>();
            auto voters_iter = votesbyid.find(idHash);

            fio_404_assert(fioname_iter != namesbyname.end(), "FIO Address not found", ErrorFioNameNotRegistered);
            const uint64_t recAcct = fioname_iter->owner_account;

            vector<name> tempvoters;

            //if found, search for actor in table
            if (voters_iter != votesbyid.end()) {
                tempvoters = voters_iter->voters;

                fio_400_assert(voters_iter->amount == amount, "amount", std::to_string(amount),
                               "Token amount mismatch.", ErrorPubAddressExist);

                auto it = std::find(tempvoters.begin(), tempvoters.end(), actor);
                fio_400_assert(it == tempvoters.end(), "actor", actor.to_string(),
                               "Oracle has already voted.", ErrorPubAddressExist);

                tempvoters.push_back(actor);

                votesbyid.modify(voters_iter, actor, [&](auto &p) {
                    p.voters = tempvoters;
                });
            } else {
                tempvoters.push_back(actor);
                uint64_t currenttime = now();

                voters.emplace(actor, [&](struct oracle_votes &p) {
                    p.id = voters.available_primary_key();
                    p.idhash = idHash;
                    p.voters = tempvoters;
                    p.obt_id = obt_id;
                    p.fio_address = fio_address;
                    p.amount = amount;
                    p.timestamp = currenttime;
                });
            }

            voters_iter = votesbyid.find(idHash); // 1 oracle hack?
            
            //verify obt and address match other entries
            uint8_t size = tempvoters.size();
            // if entries vs. number of regoracles meet consensus.
            if (oracle_size == size && !voters_iter->isComplete) {
                votesbyid.modify(voters_iter, actor, [&](auto &p) {
                    p.isComplete = true;
                });
                //Tokens are transferred to fio.wrapping.
                action(permission_level{get_self(), "active"_n},
                       TokenContract, "transfer"_n,
                       make_tuple(FIOORACLEContract, recAcct, asset(amount, FIOSYMBOL), string("Token Unwrapping"))
                ).send();
            }

            const string response_string = string("{\"status\": \"OK\"}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void regoracle(name oracle_actor, name &actor) {
            require_auth(SYSTEMACCOUNT);

            const bool accountExists = is_account(oracle_actor);
            auto other = accountmap.find(oracle_actor.value);
            fio_400_assert(other != accountmap.end(), "oracle_actor", oracle_actor.to_string(),
                           "Account is not bound on the fio chain",
                           ErrorPubAddressExist);
            fio_400_assert(accountExists, "oracle_actor", oracle_actor.to_string(),
                           "Account does not yet exist on the fio chain",
                           ErrorPubAddressExist);

            auto prodbyowner = producers.get_index<"byowner"_n>();
            auto proditer = prodbyowner.find(oracle_actor.value);

            fio_400_assert(proditer != prodbyowner.end(), "oracle_actor", oracle_actor.to_string(),
                           "Oracle not active producer", ErrorNoFioAddressProducer);

            std::vector <oraclefees> tempVec;
            oracles.emplace(actor, [&](struct oracles &p) {
                p.actor = oracle_actor.value;
                p.fees = tempVec;
            });

            const string response_string = string("{\"status\": \"OK\"}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void unregoracle(name oracle_actor) {
            require_auth(SYSTEMACCOUNT);

            auto oraclesearch = oracles.find(oracle_actor.value);
            fio_400_assert(oraclesearch != oracles.end(), "oracle_actor", oracle_actor.to_string(),
                           "Oracle is not registered", ErrorPubAddressExist);

            oracles.erase(oraclesearch);

            const string response_string = string("{\"status\": \"OK\"}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void setoraclefee(int64_t &wrap_fio_domain, int64_t &wrap_fio_tokens, name &actor) {
            require_auth(actor);

            //add check for < 0 on both fees

            auto oraclesearch = oracles.find(actor.value);
            fio_400_assert(oraclesearch != oracles.end(), "actor", actor.to_string(),
                           "Oracle is not registered", ErrorPubAddressExist);

            //search if fee is already set.
            std::vector <oraclefees> fees = oraclesearch->fees;

            for (int it = 0; it < fees.size(); it++) {
                if (fees[it].fee_name == "wrap_fio_domain") {
                    fees[it].fee_amount = wrap_fio_domain;
                } else if (fees[it].fee_name == "wrap_fio_tokens") {
                    fees[it].fee_amount = wrap_fio_tokens;
                }
            }

            if (fees.size() == 0) {
                oraclefees domain = {"wrap_fio_domain", static_cast<uint64_t>(wrap_fio_domain)};
                oraclefees tokens = {"wrap_fio_tokens", static_cast<uint64_t>(wrap_fio_tokens)};
                fees.push_back(domain);
                fees.push_back(tokens);
            }

            oracles.modify(oraclesearch, actor, [&](auto &p) {
                p.fees = fees;
            });

            const string response_string = string("{\"status\": \"OK\"}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void wrapdomain(string &fio_domain, string &chain_code, string &public_address, int64_t &max_oracle_fee,
                        int64_t &max_fee, string &tpid, name &actor) {
            require_auth(actor);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);
            fio_400_assert(public_address.length() > 0, "public_address", public_address,
                           "Invalid public address", ErrorInvalidFioNameFormat);
            fio_400_assert(validateChainNameFormat(chain_code), "chain_code", chain_code, "Invalid chain code format",
                           ErrorInvalidFioNameFormat);
            fio_400_assert(max_oracle_fee >= 0, "max_oracle_fee", to_string(max_oracle_fee), "Invalid oracle fee value",
                           ErrorMaxFeeInvalid);

            uint8_t oracle_size = std::distance(oracles.cbegin(), oracles.cend());
            fio_400_assert(3 <= oracle_size, "actor", actor.to_string(), "Not enough registered oracles.",
                           ErrorMaxFeeInvalid);

            //force uppercase chain code
            std::transform(chain_code.begin(), chain_code.end(),chain_code.begin(), ::toupper);

            FioAddress fa;
            getFioAddressStruct(fio_domain, fa);
            uint128_t domainHash = string_to_uint128_hash(fio_domain);
            fio_400_assert(fa.domainOnly, "fio_domain", fio_domain, "Invalid FIO domain",
                           ErrorInvalidFioNameFormat);

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter != domainsbyname.end(), "fio_domain", fio_domain,
                           "FIO Domain not registered",
                           ErrorDomainNotRegistered);

            fio_400_assert(domains_iter->account == actor.value, "fio_domain", fio_domain,
                           "Actor and domain owner mismatch.",
                           ErrorDomainNotRegistered);

            //Oracle fee is transferred from actor account to all registered oracles in even amount.
            auto idx = oracles.begin();
            fio_400_assert(idx != oracles.end(), "max_oracle_fee", to_string(max_oracle_fee), "No Oracles registered or fees set",
                           ErrorMaxFeeInvalid);

            int index = 0;
            vector<uint64_t> totalfees;
            uint64_t feeFinal;

            while( idx != oracles.end() ){
                uint64_t tempfee = idx->fees[0].fee_amount; //0 is domain in fee vector
                totalfees.push_back(tempfee);
                idx++;
            }

            fio_400_assert(totalfees.size() == oracle_size, "max_oracle_fee", to_string(max_oracle_fee), "Not all oracles have voted for fees",
                           ErrorMaxFeeInvalid);

            // median fee / oracle_info.size = fee paid
            sort(totalfees.begin(), totalfees.end());
            if (oracle_size % 2 == 0) {
                feeFinal = (totalfees[oracle_size / 2 - 1] + totalfees[oracle_size / 2]) / 2;
            } else {
                feeFinal = totalfees[oracle_size / 2];
            }

            idx = oracles.begin();
            uint64_t feeTotal = feeFinal * oracle_size;
            fio_400_assert(max_oracle_fee >= feeTotal, "max_oracle_fee", to_string(max_oracle_fee), "Invalid oracle fee value",
                           ErrorMaxFeeInvalid);

            while( idx != oracles.end() ){
                action(permission_level{get_self(), "active"_n},
                       TokenContract, "transfer"_n,
                       make_tuple(actor, name{idx->actor}, asset(feeFinal, FIOSYMBOL), string("Token Wrapping Oracle Fee"))
                ).send();

                idx++;
            }

            const uint32_t present_time = now();

            //Copy information to receipt table
            receipts.emplace(actor, [&](struct oracleledger &p) {
                p.id = receipts.available_primary_key();
                p.actor = actor.value;
                p.chaincode = chain_code;
                p.pubaddress = public_address;
                p.nftname = fio_domain;
                p.timestamp = present_time;
            });

            //Transfer Domain to escrow
            bool isTransferToEscrow = true;
            action(
                    permission_level{FIOORACLEContract, "active"_n},
                    AddressContract,
                    "xferescrow"_n,
                    std::make_tuple(fio_domain, nullptr, isTransferToEscrow, actor)
            ).send();

            //Chain wrap_fio_token fee is collected.
            const uint128_t endpoint_hash = string_to_uint128_hash(WRAP_FIO_DOMAIN_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);
            const uint64_t fee_type = fee_iter->type;
            const int64_t wrap_amount = fee_iter->suf_amount;

            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", WRAP_FIO_DOMAIN_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            uint64_t fee_amount = fee_iter->suf_amount;
            fio_400_assert(max_fee >= (int64_t)fee_amount, "max_fee", to_string(max_fee), "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(wrap_amount, FIOSYMBOL), WRAP_FIO_TOKENS_ENDPOINT);
            process_rewards(tpid, wrap_amount,get_self(), actor);

            //RAM of signer is increased (512) more?
            action(
                    permission_level{SYSTEMACCOUNT, "active"_n},
                    "eosio"_n,
                    "incram"_n,
                    std::make_tuple(actor, WRAPTOKENRAM)
            ).send();

            const string response_string = string("{\"status\": \"OK\",\"oracle_fee_collected\":\"") +
                                           to_string(feeTotal) + string("\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void unwrapdomain(string &fio_domain, string &obt_id, string &fio_address, name &actor) {
            require_auth(actor);

            FioAddress fa;
            getFioAddressStruct(fio_domain, fa);
            fio_400_assert(fa.domainOnly, "fio_domain", fa.fioaddress, "Invalid FIO domain",
                           ErrorInvalidFioNameFormat);

            const uint128_t domainHash    = string_to_uint128_hash(fio_domain.c_str());
            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter  = domainsbyname.find(domainHash);
            name nm = name("fio.oracle");

            fio_400_assert(obt_id.size() > 0 && obt_id.size() <= 128, "obt_it", obt_id,
                           "Invalid obt_id",
                           ErrorContentLimit);

            fio_400_assert(domains_iter != domainsbyname.end(), "fio_domain", fio_domain,
                           "FIO domain not found", ErrorDomainNotRegistered);

            fio_400_assert(domains_iter->account == nm.value, "fio_domain", fio_domain,
                           "FIO domain not owned by Oracle contract.", ErrorDomainNotRegistered);

            uint8_t oracle_size = std::distance(oracles.cbegin(), oracles.cend());
            fio_400_assert(3 <= oracle_size, "actor", actor.to_string(), "Not enough registered oracles.",
                           ErrorMaxFeeInvalid);

            auto oraclesearch = oracles.find(actor.value);
            fio_400_assert(oraclesearch != oracles.end(), "actor", actor.to_string(),
                           "Not a registered Oracle", ErrorPubAddressExist);

            const uint128_t nameHash = string_to_uint128_hash(fio_address);
            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);

            const uint128_t idHash = string_to_uint128_hash(obt_id);
            auto votesbyid = voters.get_index<"byidhash"_n>();
            auto voters_iter = votesbyid.find(idHash);

            fio_404_assert(fioname_iter != namesbyname.end(), "FIO Address not found", ErrorFioNameNotRegistered);
            const uint64_t recAcct = fioname_iter->owner_account;

            vector<name> tempvoters;

            //if found, search for actor in table
            if (voters_iter != votesbyid.end()) {
                tempvoters = voters_iter->voters;

                auto it = std::find(tempvoters.begin(), tempvoters.end(), actor);
                fio_400_assert(it == tempvoters.end(), "actor", actor.to_string(),
                               "Oracle has already voted.", ErrorPubAddressExist);
                fio_400_assert(fio_domain == voters_iter->nftname, "fio_domain", fio_domain,
                               "Domain name mismatch.", ErrorPubAddressExist);

                tempvoters.push_back(actor);

                votesbyid.modify(voters_iter, actor, [&](auto &p) {
                    p.voters = tempvoters;
                });
            } else {
                tempvoters.push_back(actor);
                uint64_t currenttime = now();

                voters.emplace(actor, [&](struct oracle_votes &p) {
                    p.id = voters.available_primary_key();
                    p.idhash = idHash;
                    p.voters = tempvoters;
                    p.obt_id = obt_id;
                    p.fio_address = fio_address;
                    p.nftname = fio_domain;
                    p.timestamp = currenttime;
                });
            }

            voters_iter = votesbyid.find(idHash);

            //verify obt and address match other entries
            uint8_t size = tempvoters.size();
            // if entries vs. number of regoracles meet consensus.
            if (oracle_size == size && !voters_iter->isComplete) {
                votesbyid.modify(voters_iter, actor, [&](auto &p) {
                    p.isComplete = true;
                });

                //transfer ownership to new owner
                auto owner = accountmap.find(recAcct);
                bool isTransferToEscrow = false;

                action(
                        permission_level{FIOORACLEContract, "active"_n},
                        AddressContract,
                        "xferescrow"_n,
                        std::make_tuple(fio_domain, owner->clientkey, isTransferToEscrow, actor)
                ).send();
            }

            const string response_string = string("{\"status\": \"OK\"}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);

            send_response(response_string.c_str());
        }
    };

    EOSIO_DISPATCH(FIOOracle, (wraptokens)(unwraptokens)(regoracle)(unregoracle)
    (setoraclefee)(wrapdomain)(unwrapdomain)
    )
}
