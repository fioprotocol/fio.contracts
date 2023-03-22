/** FioName Token implementation file
 *  Description: FioName smart contract allows issuance of unique domains and names for easy public address resolution
 *  @author Adam Androulidakis, Casey Gardiner, Ciju John, Ed Rotthoff, Phil Mesnier
 *  @file fio.address.hpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE ) Dapix
 */

#include "fio.address.hpp"
#include <fio.fee/fio.fee.hpp>
#include <fio.common/fio.common.hpp>
#include <fio.common/fiotime.hpp>
#include <fio.token/include/fio.token/fio.token.hpp>
#include <eosiolib/asset.hpp>
#include <fio.request.obt/fio.request.obt.hpp> //TEMP FOR XFERADDRESS
#include <fio.escrow/fio.escrow.hpp>

namespace fioio {

    class [[eosio::contract("FioAddressLookup")]]  FioNameLookup : public eosio::contract {

    private:
        const int MIN_VOTES_FOR_AVERAGING = 15;
        domains_table domains;
        domainsales_table domainsales;
        fionames_table fionames;
        fiofee_table fiofees;
        eosio_names_table accountmap;
        bundlevoters_table bundlevoters;
        tpids_table tpids;
        nftburnq_table nftburnqueue;
        eosiosystem::voters_table voters;
        eosiosystem::top_producers_table topprods;
        eosiosystem::producers_table producers;
        eosiosystem::locked_tokens_table lockedTokensTable;
        nfts_table nftstable;
        config appConfig;

        //FIP-39 begin
        fionameinfo_table fionameinfo;
        //FIP-39 end


    public:
        using contract::contract;

        FioNameLookup(name s, name code, datastream<const char *> ds) : contract(s, code, ds),
                                                                        domains(_self, _self.value),
                                                                        domainsales(EscrowContract, EscrowContract.value),
                                                                        fionames(_self, _self.value),
                                                                        fiofees(FeeContract, FeeContract.value),
                                                                        bundlevoters(FeeContract, FeeContract.value),
                                                                        accountmap(_self, _self.value),
                                                                        nftstable(_self, _self.value),
                                                                        nftburnqueue(get_self(), get_self().value),
                                                                        tpids(TPIDContract, TPIDContract.value),
                                                                        voters(SYSTEMACCOUNT, SYSTEMACCOUNT.value),
                                                                        topprods(SYSTEMACCOUNT, SYSTEMACCOUNT.value),
                                                                        producers(SYSTEMACCOUNT, SYSTEMACCOUNT.value),
                                                                        lockedTokensTable(SYSTEMACCOUNT,
                                                                                          SYSTEMACCOUNT.value),
                //FIP-39 begin
                                                                        fionameinfo(_self, _self.value){
                //FIP-39 end

            configs_singleton configsSingleton(FeeContract, FeeContract.value);
            appConfig = configsSingleton.get_or_default(config());
        }

        inline name accountmgnt(const name &actor, const string &owner_fio_public_key) {
            require_auth(actor);

            name owner_account_name;

            if (owner_fio_public_key.length() == 0) {
                const bool accountExists = is_account(actor);

                auto other = accountmap.find(actor.value);

                fio_400_assert(other != accountmap.end(), "owner_account", actor.to_string(),
                               "Account is not bound on the fio chain",
                               ErrorPubAddressExist);
                fio_400_assert(accountExists, "owner_account", actor.to_string(),
                               "Account does not yet exist on the fio chain",
                               ErrorPubAddressExist);

                owner_account_name = actor;
            } else {
                string owner_account;
                key_to_account(owner_fio_public_key, owner_account);
                owner_account_name = name(owner_account.c_str());

                eosio_assert(owner_account.length() == 12, "Length of account name should be 12");

                const bool accountExists = is_account(owner_account_name);
                auto other = accountmap.find(owner_account_name.value);

                if (other == accountmap.end()) { //the name is not in the table.
                    fio_400_assert(!accountExists, "owner_account", owner_account,
                                   "Account exists on FIO chain but is not bound in accountmap",
                                   ErrorPubAddressExist);

                    const auto owner_pubkey = abieos::string_to_public_key(owner_fio_public_key);

                    eosiosystem::key_weight pubkey_weight = {
                            .key = owner_pubkey,
                            .weight = 1,
                    };

                    const auto owner_auth = authority{1, {pubkey_weight}, {}, {}};

                    INLINE_ACTION_SENDER(call::eosio, newaccount)
                            ("eosio"_n, {{_self, "active"_n}},
                             {_self, owner_account_name, owner_auth, owner_auth}
                            );

                    const uint64_t nmi = owner_account_name.value;

                    accountmap.emplace(_self, [&](struct eosio_name &p) {
                        p.account = nmi;
                        p.clientkey = owner_fio_public_key;
                        p.keyhash = string_to_uint128_hash(owner_fio_public_key.c_str());
                    });

                } else {
                    fio_400_assert(accountExists, "owner_account", owner_account,
                                   "Account does not exist on FIO chain but is bound in accountmap",
                                   ErrorPubAddressExist);
                    eosio_assert_message_code(owner_fio_public_key == other->clientkey, "FIO account already bound",
                                              ErrorPubAddressExist);
                }
            }
            return owner_account_name;
        }

        inline void addburnq(const string &fio_address, const uint128_t &fioaddhash) {

          auto contractsbyname = nftstable.get_index<"byaddress"_n>();
          if(contractsbyname.find(fioaddhash) != contractsbyname.end()) {

            auto burnqbyname = nftburnqueue.get_index<"byaddress"_n>();
            auto nftburnq_iter = burnqbyname.find(fioaddhash);

            fio_400_assert(nftburnq_iter ==  burnqbyname.end(), "fio_address", fio_address,
                           "FIO Address NFTs are being burned", ErrorInvalidValue);

            if (nftburnq_iter == burnqbyname.end() ) {
              nftburnqueue.emplace(get_self(), [&](auto &n) {
                n.id = nftburnqueue.available_primary_key();
                n.fio_address_hash = fioaddhash;
              });
            }
          }

        }

        // FIP-39 begin
        inline void updfionminf(const string &datavalue, const string &datadesc, const uint64_t &fionameid, const name &actor) {
            auto fionameinfobynameid = fionameinfo.get_index<"byfionameid"_n>();
            auto fionameinfo_iter = fionameinfobynameid.find(fionameid);
            if(fionameinfo_iter == fionameinfobynameid.end()){
                uint64_t id = fionameinfo.available_primary_key();
                fionameinfo.emplace(actor, [&](struct fioname_info_item &d) {
                    d.id = id;
                    d.fionameid = fionameid;
                    d.datadesc = datadesc;
                    d.datavalue = datavalue;
                });
            }else {
                auto matchdesc_iter = fionameinfo_iter;
                //now check for multiples. no duplicates permitted in table.
                int countem = 0;
                while (fionameinfo_iter != fionameinfobynameid.end()) {
                    if ( (fionameinfo_iter->datadesc.compare(datadesc) == 0) && (fionameinfo_iter->fionameid == fionameid)) {
                        countem++;
                        matchdesc_iter = fionameinfo_iter;
                    }else if (fionameinfo_iter->fionameid != fionameid){
                        break;
                    }
                    fionameinfo_iter++;
                }

                if(countem == 0){
                    uint64_t id = fionameinfo.available_primary_key();
                    fionameinfo.emplace(actor, [&](struct fioname_info_item &d) {
                        d.id = id;
                        d.fionameid = fionameid;
                        d.datadesc = datadesc;
                        d.datavalue = datavalue;
                    });
                }
                else {
                    //we found one to get into this block so if more than one then error.
                    fio_400_assert(countem == 1, "datadesc", datadesc,
                                   "handle info error -- multiple data values present for datadesc ",
                                   ErrorInvalidValue);
                    fionameinfobynameid.modify(matchdesc_iter, actor, [&](struct fioname_info_item &d) {
                        d.datavalue = datavalue;
                    });
                }
            }

        }

        inline void remhandleinf(const uint64_t &fionameid) {
            auto fionameinfobynameid = fionameinfo.get_index<"byfionameid"_n>();
            auto fionameinfo_iter = fionameinfobynameid.find(fionameid);
            if(fionameinfo_iter != fionameinfobynameid.end()){
                auto next_iter = fionameinfo_iter;
                next_iter++;
                fionameinfobynameid.erase(fionameinfo_iter);
                fionameinfo_iter = next_iter;
            }
        }
        //FIP-39 end






        inline void register_errors(const FioAddress &fa, bool domain) const {
            string fioname = "fio_address";
            string fioerror = "Invalid FIO address";
            if (domain) {
                fioname = "fio_domain";
                fioerror = "Invalid FIO domain";
            }
            fio_400_assert(validateFioNameFormat(fa), fioname, fa.fioaddress, fioerror, ErrorInvalidFioNameFormat);
        }

        inline uint64_t getBundledAmount() {
            int totalcount = 0;
            vector <uint64_t> votes;
            uint64_t returnvalue = 0;

            if (bundlevoters.end() == bundlevoters.begin()) {
                return DEFAULTBUNDLEAMT;
            }

            for (const auto &itr : topprods) {
                auto vote_iter = bundlevoters.find(itr.producer.value);
                if (vote_iter != bundlevoters.end()) {
                    votes.push_back(vote_iter->bundledbvotenumber);
                }
            }

            size_t size = votes.size();

            if (size < MIN_VOTES_FOR_AVERAGING) {
                return DEFAULTBUNDLEAMT;
            } else if (size >= MIN_VOTES_FOR_AVERAGING) {
                sort(votes.begin(), votes.end());
                if (size % 2 == 0) {
                    return (votes[size / 2 - 1] + votes[size / 2]) / 2;
                } else {
                    return votes[size / 2];
                }
            }
            return DEFAULTBUNDLEAMT;
        }

        uint32_t fio_address_update(const name &actor, const name &owner, const uint64_t max_fee, const FioAddress &fa,
                                    const string &tpid) {

            const uint32_t expiration_time = 4294967295; //Sunday, February 7, 2106 6:28:15 AM GMT+0000 (Max 32 bit expiration)
            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());

            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            fio_400_assert(!fa.domainOnly, "fio_address", fa.fioaddress, "Invalid FIO address",
                           ErrorInvalidFioNameFormat);

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter != domainsbyname.end(), "fio_address", fa.fioaddress,
                           "FIO Domain not registered",
                           ErrorDomainNotRegistered);

            const bool isPublic = domains_iter->is_public;
            uint64_t domain_owner = domains_iter->account;

            if (!isPublic) {
                fio_400_assert(domain_owner == actor.value, "fio_address", fa.fioaddress,
                               "FIO Domain is not public. Only owner can create FIO Addresses.",
                               ErrorInvalidFioNameFormat);
            }

            const uint32_t domain_expiration = domains_iter->expiration;
            const uint32_t present_time = now();
            fio_400_assert(present_time <= domain_expiration, "fio_address", fa.fioaddress, "FIO Domain expired",
                           ErrorDomainExpired);

            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter == namesbyname.end(), "fio_address", fa.fioaddress,
                           "FIO address already registered", ErrorFioNameAlreadyRegistered);

            auto key_iter = accountmap.find(owner.value);

            fio_400_assert(key_iter != accountmap.end(), "owner", to_string(owner.value),
                           "Owner is not bound in the account map.", ErrorActorNotInFioAccountMap);

            uint64_t id = fionames.available_primary_key();
            vector <tokenpubaddr> pubaddresses;
            tokenpubaddr t1;
            t1.public_address = key_iter->clientkey;
            t1.token_code = "FIO";
            t1.chain_code = "FIO";
            pubaddresses.push_back(t1);

            fionames.emplace(actor, [&](struct fioname &a) {
                a.id = id;
                a.name = fa.fioaddress;
                a.addresses = pubaddresses;
                a.namehash = nameHash;
                a.domain = fa.fiodomain;
                a.domainhash = domainHash;
                a.expiration = expiration_time;
                a.owner_account = owner.value;
                a.bundleeligiblecountdown = getBundledAmount();
            });

            //FIP-39 begin
            //update the encryption key to use.
            updfionminf(key_iter->clientkey, FIO_REQUEST_CONTENT_ENCRYPTION_PUB_KEY_DATA_DESC,id,actor);
            //FIP-39 end

            uint64_t fee_amount = chain_data_update(fa.fioaddress, pubaddresses, max_fee, fa, actor, owner,
                                                    true, tpid);

            return expiration_time;
        }

        uint32_t fio_domain_update(const name &owner,
                                   const FioAddress &fa,
                                   const name &actor) {

            uint128_t domainHash = string_to_uint128_hash(fa.fioaddress.c_str());
            uint32_t expiration_time;

            fio_400_assert(fa.domainOnly, "fio_address", fa.fioaddress, "Invalid FIO domain",
                           ErrorInvalidFioNameFormat);

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter == domainsbyname.end(), "fio_name", fa.fioaddress,
                           "FIO domain already registered", ErrorDomainAlreadyRegistered);

            expiration_time = get_now_plus_one_year();

            uint64_t id = domains.available_primary_key();

            domains.emplace(actor, [&](struct domain &d) {
                d.id = id;
                d.name = fa.fiodomain;
                d.domainhash = domainHash;
                d.expiration = expiration_time;
                d.account = owner.value;
            });
            return expiration_time;
        }


        uint64_t perform_remove_address
                (const string &fioaddress, const vector <tokenpubaddr> &pubaddresses,
                 const uint64_t &max_fee, const FioAddress &fa,
                 const name &actor, const string &tpid) {

            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());

            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fioaddress, "Invalid FIO Address",
                           ErrorFioNameNotRegistered);

            const uint64_t account = fioname_iter->owner_account;
            fio_403_assert(account == actor.value, ErrorSignature);

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_404_assert(domains_iter != domainsbyname.end(), "FIO Domain not found", ErrorDomainNotFound);

            //add 30 days to the domain expiration, this call will work until 30 days past expire.
            const uint32_t expiration = get_time_plus_seconds(domains_iter->expiration, SECONDS30DAYS);

            fio_400_assert(now() <= expiration, "domain", fa.fiodomain, "FIO Domain expired",
                           ErrorDomainExpired);

            tokenpubaddr tempStruct;
            string token;
            string chaincode;
            string public_address;

            for (auto tpa = pubaddresses.begin(); tpa != pubaddresses.end(); ++tpa) {
                bool wasFound = false;
                token = tpa->token_code.c_str();
                chaincode = tpa->chain_code.c_str();
                public_address = tpa->public_address.c_str();

                fio_400_assert(validateTokenNameFormat(token), "token_code", tpa->token_code,
                               "Invalid token code format",
                               ErrorInvalidFioNameFormat);
                fio_400_assert(validateChainNameFormat(chaincode), "chain_code", tpa->chain_code,
                               "Invalid chain code format",
                               ErrorInvalidFioNameFormat);
                fio_400_assert(validatePubAddressFormat(tpa->public_address), "public_address", tpa->public_address,
                               "Invalid public address format",
                               ErrorChainAddressEmpty);

                int idx = 0;
                for (auto it = fioname_iter->addresses.begin(); it != fioname_iter->addresses.end(); ++it) {
                    if ((it->token_code == token) && (it->chain_code == chaincode) &&
                        it->public_address == public_address) {
                        wasFound = true;
                        break;
                    }
                    idx++;
                }
                fio_400_assert(wasFound, "public_address", public_address, "Invalid public address",
                               ErrorInvalidFioNameFormat);

                namesbyname.modify(fioname_iter, actor, [&](struct fioname &a) {
                    a.addresses.erase(a.addresses.begin() + idx);
                });

            }

            uint64_t fee_amount = 0;

            //begin new fees, bundle eligible fee logic
            const uint128_t endpoint_hash = string_to_uint128_hash(REMOVE_PUB_ADDRESS_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);

            //if the fee isnt found for the endpoint, then 400 error.
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", REMOVE_PUB_ADDRESS_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const int64_t reg_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 1, "fee_type", to_string(fee_type),
                           "remove_fio_address unexpected fee type for endpoint remove_pub_address, expected 1",
                           ErrorNoEndpoint);

            const uint64_t bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;

            if (bundleeligiblecountdown > 0) {
                namesbyname.modify(fioname_iter, _self, [&](struct fioname &a) {
                    a.bundleeligiblecountdown = (bundleeligiblecountdown - 1);
                });
            } else {
                fee_amount = fee_iter->suf_amount;
                fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                               "Fee exceeds supplied maximum.",
                               ErrorMaxFeeExceeded);

                //NOTE -- question here, should we always record the transfer for the fees, even when its zero,
                //or should we do as this code does and not do a transaction when the fees are 0.
                fio_fees(actor, asset(reg_amount, FIOSYMBOL), REMOVE_PUB_ADDRESS_ENDPOINT);
                process_rewards(tpid, reg_amount, get_self(), actor);

                if (reg_amount > 0) {
                    INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                            ("eosio"_n, {{_self, "active"_n}},
                             {actor, true}
                            );
                }
            }
            return fee_amount;
        }


        uint64_t perform_remove_all_addresses
                (const string &fioaddress,
                 const uint64_t &max_fee, const FioAddress &fa,
                 const name &actor, const string &tpid) {

            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());

            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_404_assert(fioname_iter != namesbyname.end(), "FIO Address not found", ErrorFioNameNotRegistered);

            const uint64_t account = fioname_iter->owner_account;
            fio_403_assert(account == actor.value, ErrorSignature);

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_404_assert(domains_iter != domainsbyname.end(), "FIO Domain not found", ErrorDomainNotFound);

            //add 30 days to the domain expiration, this call will work until 30 days past expire.
            const uint32_t expiration = get_time_plus_seconds(domains_iter->expiration, SECONDS30DAYS);

            fio_400_assert(now() <= expiration, "domain", fa.fiodomain, "FIO Domain expired",
                           ErrorDomainExpired);

            int idx = 0;
            bool wasFound = false;
            for (auto it = fioname_iter->addresses.begin(); it != fioname_iter->addresses.end(); ++it) {
                if ((it->token_code == "FIO") && (it->chain_code == "FIO")) {
                    wasFound = true;
                    break;
                }
                idx++;
            }

            if (!wasFound) {//remove em all
                namesbyname.modify(fioname_iter, actor, [&](struct fioname &a) {
                    a.addresses.clear();
                });
            } else {
                namesbyname.modify(fioname_iter, actor, [&](struct fioname &a) {
                    a.addresses.erase(a.addresses.begin() + (idx + 1), a.addresses.end());
                });

                if (idx > 0) {
                    namesbyname.modify(fioname_iter, actor, [&](struct fioname &a) {
                        a.addresses.erase(a.addresses.begin(), a.addresses.begin() + idx);
                    });
                }
            }

            uint64_t fee_amount = 0;

            //begin new fees, bundle eligible fee logic
            const uint128_t endpoint_hash = string_to_uint128_hash(REMOVE_ALL_PUB_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);

            //if the fee isnt found for the endpoint, then 400 error.
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", REMOVE_ALL_PUB_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const int64_t reg_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 1, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint remove_all_pub_addresses, expected 1",
                           ErrorNoEndpoint);

            const uint64_t bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;

            if (bundleeligiblecountdown > 0) {
                namesbyname.modify(fioname_iter, _self, [&](struct fioname &a) {
                    a.bundleeligiblecountdown = (bundleeligiblecountdown - 1);
                });
            } else {
                fee_amount = fee_iter->suf_amount;
                fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                               "Fee exceeds supplied maximum.",
                               ErrorMaxFeeExceeded);

                //NOTE -- question here, should we always record the transfer for the fees, even when its zero,
                //or should we do as this code does and not do a transaction when the fees are 0.
                fio_fees(actor, asset(reg_amount, FIOSYMBOL), REMOVE_ALL_PUB_ENDPOINT);
                process_rewards(tpid, reg_amount, get_self(), actor);

                if (reg_amount > 0) {
                    INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                            ("eosio"_n, {{_self, "active"_n}},
                             {actor, true}
                            );
                }
            }
            return fee_amount;
        }

        uint64_t chain_data_update
                (const string &fioaddress, const vector <tokenpubaddr> &pubaddresses,
                 const uint64_t &max_fee, const FioAddress &fa,
                 const name &actor, const name &owner, const bool &isFIO, const string &tpid) {

            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());

            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_404_assert(fioname_iter != namesbyname.end(), "FIO Address not found", ErrorFioNameNotRegistered);

            const uint64_t account = fioname_iter->owner_account;
            fio_403_assert(account == owner.value, ErrorSignature);

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_404_assert(domains_iter != domainsbyname.end(), "FIO Domain not found", ErrorDomainNotFound);

            //add 30 days to the domain expiration, this call will work until 30 days past expire.
            const uint32_t expiration = get_time_plus_seconds(domains_iter->expiration, SECONDS30DAYS);

            fio_400_assert(now() <= expiration, "domain", fa.fiodomain, "FIO Domain expired",
                           ErrorDomainExpired);

            tokenpubaddr tempStruct;
            string token;
            string chaincode;

            for (auto tpa = pubaddresses.begin(); tpa != pubaddresses.end(); ++tpa) {
                bool wasFound = false;
                token = tpa->token_code.c_str();
                chaincode = tpa->chain_code.c_str();

                fio_400_assert(validateTokenNameFormat(token), "token_code", tpa->token_code,
                               "Invalid token code format",
                               ErrorInvalidFioNameFormat);
                fio_400_assert(validateChainNameFormat(chaincode), "chain_code", tpa->chain_code,
                               "Invalid chain code format",
                               ErrorInvalidFioNameFormat);
                fio_400_assert(validatePubAddressFormat(tpa->public_address), "public_address", tpa->public_address,
                               "Invalid public address format",
                               ErrorChainAddressEmpty);

                auto it = std::find_if(fioname_iter->addresses.begin(), fioname_iter->addresses.end(),
                                       find_token(token));
                if ((it->token_code == token) && (it->chain_code == chaincode)) {
                    namesbyname.modify(fioname_iter, actor, [&](struct fioname &a) {
                        a.addresses[it - fioname_iter->addresses.begin()].public_address = tpa->public_address;
                    });
                    wasFound = true;
                } else if (it->token_code == token && it->chain_code != chaincode) {
                    for (auto it = fioname_iter->addresses.begin(); it != fioname_iter->addresses.end(); ++it) {
                        if ((it->token_code == token) && (it->chain_code == chaincode)) {
                            namesbyname.modify(fioname_iter, actor, [&](struct fioname &a) {
                                a.addresses[it - fioname_iter->addresses.begin()].public_address = tpa->public_address;
                            });
                            wasFound = true;
                            break;
                        }
                    }
                }

                if (!wasFound) {
                    fio_400_assert(fioname_iter->addresses.size() != MAX_SET_ADDRESSES, "token_code", tpa->token_code,
                                   "Maximum token codes mapped to single FIO Address reached. Only 200 can be mapped.",
                                   ErrorInvalidFioNameFormat); // Don't forget to set the error amount if/when changing MAX_SET_ADDRESSES

                    tempStruct.public_address = tpa->public_address;
                    tempStruct.token_code = tpa->token_code;
                    tempStruct.chain_code = tpa->chain_code;

                    namesbyname.modify(fioname_iter, actor, [&](struct fioname &a) {
                        a.addresses.push_back(tempStruct);
                    });
                }
            }

            uint64_t fee_amount = 0;

            if (isFIO) {
                return fee_amount;
            }

            //begin new fees, bundle eligible fee logic
            const uint128_t endpoint_hash = string_to_uint128_hash(ADD_PUB_ADDRESS_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);

            //if the fee isnt found for the endpoint, then 400 error.
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", ADD_PUB_ADDRESS_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const int64_t reg_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 1, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint add_pub_address, expected 0",
                           ErrorNoEndpoint);

            const uint64_t bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;

            if (bundleeligiblecountdown > 0) {
                namesbyname.modify(fioname_iter, _self, [&](struct fioname &a) {
                    a.bundleeligiblecountdown = (bundleeligiblecountdown - 1);
                });
            } else {
                fee_amount = fee_iter->suf_amount;
                fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                               "Fee exceeds supplied maximum.",
                               ErrorMaxFeeExceeded);

                //NOTE -- question here, should we always record the transfer for the fees, even when its zero,
                //or should we do as this code does and not do a transaction when the fees are 0.
                fio_fees(actor, asset(reg_amount, FIOSYMBOL), ADD_PUB_ADDRESS_ENDPOINT);
                process_rewards(tpid, reg_amount, get_self(), actor);

                if (reg_amount > 0) {
                    INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                            ("eosio"_n, {{_self, "active"_n}},
                             {actor, true}
                            );
                }
            }
            return fee_amount;
        }

        inline uint32_t get_time_plus_one_year(const uint32_t timein) {
            return timein + YEARTOSECONDS;
        }

        /***
         * This method will return now plus one year.
         * the result is the present block time, which is number of seconds since 1970
         * incremented by secondss per year.
         */
        inline uint32_t get_now_plus_one_year() {
            return now() + YEARTOSECONDS;
        }

        /***
         * This method will decrement the now time by the specified number of years.
         * @param nyearsago   this is the number of years ago from now to return as a value
         * @return  the decremented now() time by nyearsago
         */
        inline uint32_t get_now_minus_years(const uint32_t nyearsago) {
            return now() - (YEARTOSECONDS * nyearsago);
        }

        /***
         * This method will increment the now time by the specified number of years.
         * @param nyearsago   this is the number of years from now to return as a value
         * @return  the decremented now() time by nyearsago
         */
        inline uint32_t get_now_plus_years(const uint32_t nyearsago) {

            return now() + (YEARTOSECONDS * nyearsago);
        }

        /********* CONTRACT ACTIONS ********/

        //FIP-39 begin
        [[eosio::action]]
        void
        updcryptkey(const string &fio_address, const string &encrypt_public_key, const int64_t &max_fee,
                    const name &actor,
                    const string &tpid) {


            print("updcryptkey --      called. \n");

            //VERIFY INPUTS
            FioAddress fa;
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);

            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            //requirement, allow empty string to be used!
            if (encrypt_public_key.length() > 0) {
                fio_400_assert(isPubKeyValid(encrypt_public_key), "encrypt_public_key", encrypt_public_key,
                               "Encrypt key not a valid FIO Public Key",
                               ErrorPubKeyValid);
            }

            getFioAddressStruct(fio_address, fa);
            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());

            fio_400_assert(!fa.domainOnly, "fio_address", fa.fioaddress, "FIO Address invalid or does not exist.",
                           ErrorInvalidFioNameFormat);

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter != domainsbyname.end(), "fio_address", fa.fioaddress,
                           "FIO Domain not registered",
                           ErrorDomainNotRegistered);

            //add 30 days to the domain expiration, this call will work until 30 days past expire.
            const uint32_t domain_expiration = get_time_plus_seconds(domains_iter->expiration, SECONDS30DAYS);

            const uint32_t present_time = now();
            fio_400_assert(present_time <= domain_expiration, "fio_address", fa.fioaddress, "FIO Domain expired",
                           ErrorDomainExpired);

            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fa.fioaddress,
                           "FIO Address invalid or does not exist", ErrorInvalidFioNameFormat);
            fio_403_assert(fioname_iter->owner_account == actor.value,
                           ErrorSignature); // check if actor owns FIO Address




            //FEE PROCESSING
            uint64_t fee_amount = 0;

            //begin fees, bundle eligible fee logic
            const uint128_t endpoint_hash = string_to_uint128_hash(UPDATE_ENCRYPT_KEY_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);

            //if the fee isnt found for the endpoint, then 400 error.
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", UPDATE_ENCRYPT_KEY_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const int64_t reg_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 1, "fee_type", to_string(fee_type),
                           "update_encrypt_key unexpected fee type for endpoint update_encrypt_key, expected 1",
                           ErrorNoEndpoint);

            const uint64_t bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;

            if (bundleeligiblecountdown > 0) {
                namesbyname.modify(fioname_iter, _self, [&](struct fioname &a) {
                    a.bundleeligiblecountdown = (bundleeligiblecountdown - 1);
                });
            } else {
                fee_amount = fee_iter->suf_amount;
                fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                               "Fee exceeds supplied maximum.",
                               ErrorMaxFeeExceeded);

                fio_fees(actor, asset(reg_amount, FIOSYMBOL), UPDATE_ENCRYPT_KEY_ENDPOINT);
                process_rewards(tpid, reg_amount, get_self(), actor);

                if (reg_amount > 0) {
                    INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                            ("eosio"_n, {{_self, "active"_n}},
                             {actor, true}
                            );
                }
            }

            if (UPDENCRYPTKEYRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, UPDENCRYPTKEYRAM)
                ).send();
            }

            updfionminf(encrypt_public_key, FIO_REQUEST_CONTENT_ENCRYPTION_PUB_KEY_DATA_DESC,fioname_iter->id,actor);

            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", UPDATE_ENCRYPT_KEY_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(reg_amount) + string("}");

            send_response(response_string.c_str());

        }
        //FIP-39 end










        [[eosio::action]]
        void
        regaddress(const string &fio_address, const string &owner_fio_public_key, const int64_t &max_fee,
                   const name &actor,
                   const string &tpid) {

            FioAddress fa;
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            if (owner_fio_public_key.length() > 0) {
                fio_400_assert(isPubKeyValid(owner_fio_public_key), "owner_fio_public_key", owner_fio_public_key,
                               "Invalid FIO Public Key",
                               ErrorPubKeyValid);
            }

            name owner_account_name = accountmgnt(actor, owner_fio_public_key);

            getFioAddressStruct(fio_address, fa);
            register_errors(fa, false);
            const name nm = name{owner_account_name};

            const uint64_t expiration_time = fio_address_update(actor, nm, max_fee, fa, tpid);

            struct tm timeinfo;
            fioio::convertfiotime(expiration_time, &timeinfo);
            std::string timebuffer = fioio::tmstringformat(timeinfo);

            const uint128_t endpoint_hash = string_to_uint128_hash(REGISTER_ADDRESS_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", REGISTER_ADDRESS_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const uint64_t reg_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint register_fio_address, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) reg_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(reg_amount, FIOSYMBOL), REGISTER_ADDRESS_ENDPOINT);
            processbucketrewards(tpid, reg_amount, get_self(), actor);

            if (REGADDRESSRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, REGADDRESSRAM)
                ).send();
            }


            const string response_string = string("{\"status\": \"OK\",\"expiration\":\"") +
                                           timebuffer + string("\",\"fee_collected\":") +
                                           to_string(reg_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void regdomain(const string &fio_domain, const string &owner_fio_public_key,
                       const int64_t &max_fee, const name &actor, const string &tpid) {
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            if (owner_fio_public_key.length() > 0) {
                fio_400_assert(isPubKeyValid(owner_fio_public_key), "owner_fio_public_key", owner_fio_public_key,
                               "Invalid FIO Public Key",
                               ErrorPubKeyValid);
            }

            name owner_account_name = accountmgnt(actor, owner_fio_public_key);

            FioAddress fa;
            getFioAddressStruct(fio_domain, fa);
            register_errors(fa, true);
            const name nm = name{owner_account_name};

            const uint32_t expiration_time = fio_domain_update(nm, fa, actor);

            struct tm timeinfo;
            fioio::convertfiotime(expiration_time, &timeinfo);
            std::string timebuffer = fioio::tmstringformat(timeinfo);

            const uint128_t endpoint_hash = string_to_uint128_hash(REGISTER_DOMAIN_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", REGISTER_DOMAIN_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const uint64_t reg_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint register_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) reg_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(reg_amount, FIOSYMBOL), REGISTER_DOMAIN_ENDPOINT);
            processbucketrewards(tpid, reg_amount, get_self(), actor);

            const string response_string = string("{\"status\": \"OK\",\"expiration\":\"") +
                                           timebuffer + string("\",\"fee_collected\":") +
                                           to_string(reg_amount) + string("}");

            if (REGDOMAINRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, REGDOMAINRAM)
                ).send();
            }

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }


        /***********
         * This action will register a fio domain and fio handle on the domain. T
         * @param fio_address this is the fio address that will be registered to the domain. Domain must not already be registered.
         * @param is_public 0 - the domain will be private; 1 - the new domain will be public
         * @param owner_fio_public_key this is the public key that will own the registered fio address and fio domain
         * @param max_fee  this is the maximum fee that is willing to be paid for this transaction on the blockchain.
         * @param tpid  this is the fio address of the owner of the domain.
         * @param actor this is the fio account that has sent this transaction.
         */
        [[eosio::action]]
        void
        regdomadd(const string &fio_address, const uint8_t &is_public, const string &owner_fio_public_key, const int64_t &max_fee, const string &tpid, const name &actor) 
        {

            FioAddress fa;
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);


            fio_400_assert((is_public == 1 || is_public == 0), "is_public", to_string(is_public), "Only 0 or 1 allowed",
                           ErrorMaxFeeInvalid);

            if (owner_fio_public_key.length() > 0) {
                fio_400_assert(isPubKeyValid(owner_fio_public_key), "owner_fio_public_key", owner_fio_public_key,
                               "Invalid FIO Public Key",
                               ErrorPubKeyValid);
            }
            
            name owner_account_name = accountmgnt(actor, owner_fio_public_key);

            getFioAddressStruct(fio_address, fa);

            fio_400_assert(validateFioNameFormat(fa), "fio_address", fa.fioaddress, "Invalid FIO Address format", ErrorInvalidFioNameFormat);

            uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter == domainsbyname.end(), "fio_name", fa.fioaddress,
                           "Domain already registered, use regaddress instead.", ErrorDomainAlreadyRegistered);
            
            uint32_t domain_expiration = get_now_plus_one_year();
            domains.emplace(actor, [&](struct domain &d) {
                d.id = domains.available_primary_key();;
                d.name = fa.fiodomain;
                d.domainhash = domainHash;
                d.expiration = domain_expiration;
                d.account = owner_account_name.value;
                d.is_public = is_public;
            });

            auto key_iter = accountmap.find(owner_account_name.value);

            fio_400_assert(key_iter != accountmap.end(), "owner", to_string(owner_account_name.value),
                           "Owner is not bound in the account map.", ErrorActorNotInFioAccountMap);

            vector <tokenpubaddr> pubaddresses;
            tokenpubaddr t1;
            t1.public_address = key_iter->clientkey;
            t1.token_code = "FIO";
            t1.chain_code = "FIO";
            pubaddresses.push_back(t1);

            fionames.emplace(actor, [&](struct fioname &a) {
                a.id = fionames.available_primary_key();;
                a.name = fa.fioaddress;
                a.addresses = pubaddresses;
                a.namehash = string_to_uint128_hash(fa.fioaddress.c_str());;
                a.domain = fa.fiodomain;
                a.domainhash = domainHash;
                a.expiration = 4294967295; //Sunday, February 7, 2106 6:28:15 AM GMT+0000 (Max 32 bit expiration)
                a.owner_account = owner_account_name.value;
                a.bundleeligiblecountdown = getBundledAmount();
            });
            
            const uint128_t endpoint_hash = string_to_uint128_hash(REGISTER_FIO_DOMAIN_ADDRESS_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", REGISTER_FIO_DOMAIN_ADDRESS_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const uint64_t reg_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint register_fio_address, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) reg_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(reg_amount, FIOSYMBOL), REGISTER_FIO_DOMAIN_ADDRESS_ENDPOINT);
            processbucketrewards(tpid, reg_amount, get_self(), actor);

            if (REGDOMADDRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, REGDOMADDRAM)
                ).send();
            }
            
            struct tm timeinfo;
            fioio::convertfiotime(domain_expiration, &timeinfo);
            std::string timebuffer = fioio::tmstringformat(timeinfo);

            const string response_string = string("{\"status\": \"OK\",\"expiration\":\"") +
                                           timebuffer + string("\",\"fee_collected\":") +
                                           to_string(reg_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str()); 

        }

        /***********
         * This action will renew a fio domain, the domains expiration time will be extended by one year.
         * @param fio_domain this is the fio domain to be renewed.
         * @param max_fee  this is the maximum fee that is willing to be paid for this transaction on the blockchain.
         * @param tpid  this is the fio address of the owner of the domain.
         * @param actor this is the fio account that has sent this transaction.
         */
        [[eosio::action]]
        void
        renewdomain(const string &fio_domain, const int64_t &max_fee, const string &tpid, const name &actor) {
            require_auth(actor);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            FioAddress fa;
            getFioAddressStruct(fio_domain, fa);
            register_errors(fa, true);

            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());


            fio_400_assert(fa.domainOnly, "fio_address", fa.fioaddress, "Invalid FIO domain",
                           ErrorInvalidFioNameFormat);

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter != domainsbyname.end(), "fio_domain", fa.fioaddress,
                           "FIO domain not found", ErrorDomainNotRegistered);

            const uint32_t expiration_time = domains_iter->expiration;
            const uint128_t endpoint_hash = string_to_uint128_hash(RENEW_DOMAIN_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", RENEW_DOMAIN_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const uint64_t reg_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint renew_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) reg_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(reg_amount, FIOSYMBOL), RENEW_DOMAIN_ENDPOINT);
            processbucketrewards(tpid, reg_amount, get_self(), actor);

            const uint64_t new_expiration_time = get_time_plus_one_year(expiration_time);

            struct tm timeinfo;
            fioio::convertfiotime(new_expiration_time, &timeinfo);
            std::string timebuffer = fioio::tmstringformat(timeinfo);

            domainsbyname.modify(domains_iter, _self, [&](struct domain &a) {
                a.expiration = new_expiration_time;
            });

            const string response_string = string("{\"status\": \"OK\",\"expiration\":\"") +
                                           timebuffer + string("\",\"fee_collected\":") +
                                           to_string(reg_amount) + string("}");


            if (RENEWDOMAINRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, RENEWDOMAINRAM)
                ).send();
            }


            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        /**********
         * This action will renew a fio address, the expiration will be extended by one year from the
         * previous value of the expiration
         * @param fio_address this is the fio address to be renewed.
         * @param max_fee this is the maximum fee the user is willing to pay for this transaction
         * @param tpid this is the owner of the domain
         * @param actor this is the account for the user requesting the renewal.
         */
        [[eosio::action]]
        void
        renewaddress(const string &fio_address, const int64_t &max_fee, const string &tpid, const name &actor) {
            require_auth(actor);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            FioAddress fa;

            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            getFioAddressStruct(fio_address, fa);
            register_errors(fa, false);

            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());

            fio_400_assert(!fa.domainOnly, "fio_address", fa.fioaddress, "Invalid FIO address",
                           ErrorInvalidFioNameFormat);

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_400_assert(domains_iter != domainsbyname.end(), "fio_address", fa.fioaddress,
                           "FIO Domain not registered",
                           ErrorDomainNotRegistered);

            //add 30 days to the domain expiration, this call will work until 30 days past expire.
            const uint32_t domain_expiration = get_time_plus_seconds(domains_iter->expiration, SECONDS30DAYS);

            const uint32_t present_time = now();
            fio_400_assert(present_time <= domain_expiration, "fio_address", fa.fioaddress, "FIO Domain expired",
                           ErrorDomainExpired);

            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fa.fioaddress,
                           "FIO address not registered", ErrorFioNameNotRegistered);

            const uint64_t expiration_time = fioname_iter->expiration;
            const uint64_t bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;
            const uint128_t endpoint_hash = string_to_uint128_hash(RENEW_ADDRESS_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);

            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", RENEW_ADDRESS_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const uint64_t reg_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint renew_fio_address, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) reg_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(reg_amount, FIOSYMBOL), RENEW_ADDRESS_ENDPOINT);
            processbucketrewards(tpid, reg_amount, get_self(), actor);

            const uint64_t new_expiration_time = 4294967295; //Sunday, February 7, 2106 6:28:15 AM GMT+0000 (Max 32 bit expiration)

            struct tm timeinfo;
            fioio::convertfiotime(new_expiration_time, &timeinfo);
            std::string timebuffer = fioio::tmstringformat(timeinfo);

            namesbyname.modify(fioname_iter, _self, [&](struct fioname &a) {
                a.expiration = new_expiration_time;
                a.bundleeligiblecountdown = getBundledAmount() + bundleeligiblecountdown;
            });

            const string response_string = string("{\"status\": \"OK\",\"expiration\":\"") +
                                           timebuffer + string("\",\"fee_collected\":") +
                                           to_string(reg_amount) + string("}");

            if (RENEWADDRESSRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, RENEWADDRESSRAM)
                ).send();
            }

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        /*
         * This action will look for expired domains, then look for expired addresses, it will burn a total
         * of 25 addresses each time called. please see the code for the logic of identifying expired domains
         * and addresses.
         */
        [[eosio::action]]
        void burnexpired(const uint64_t &offset = 0, const uint32_t &limit = 15) {
            uint32_t numbertoburn = limit;
            if (numbertoburn > 15) { numbertoburn = 15; }
            unsigned int recordProcessed = 0;
            const uint64_t nowtime = now();
            uint32_t minexpiration = nowtime - DOMAINWAITFORBURNDAYS;
            uint32_t currentWork = 0;

            uint64_t index = offset;
            auto domainiter = domains.find(index);

            while (domainiter != domains.end()) {
                const uint64_t expire = domainiter->expiration;
                if ((expire + DOMAINWAITFORBURNDAYS) < nowtime) {
                    const auto domainhash = domainiter->domainhash;
                    auto nameexpidx = fionames.get_index<"bydomain"_n>();
                    auto nameiter = nameexpidx.find(domainhash);

                    while (nameiter != nameexpidx.end()) {
                        auto nextname = nameiter;
                        nextname++;
                        if (nameiter->domainhash == domainhash) {
                            const uint64_t burner = nameiter->namehash;
                            auto tpidbyname = tpids.get_index<"byname"_n>();
                            auto tpiditer = tpidbyname.find(burner);
                            auto burnqbyname = nftburnqueue.get_index<"byaddress"_n>();
                            auto nftburnq_iter = burnqbyname.find(burner);

                            if (nftburnq_iter == burnqbyname.end()) {
                                nftburnqueue.emplace(SYSTEMACCOUNT, [&](auto &n) {
                                    n.id = nftburnqueue.available_primary_key();
                                    n.fio_address_hash = burner;
                                });
                            }

                            if (tpiditer != tpidbyname.end()) { tpidbyname.erase(tpiditer); }

                            auto producersbyaddress = producers.get_index<"byaddress"_n>();
                            auto prod_iter = producersbyaddress.find(burner);
                            auto proxybyaddress = voters.get_index<"byaddress"_n>();
                            auto proxy_iter = proxybyaddress.find(burner);

                            if (proxy_iter != proxybyaddress.end() || prod_iter != producersbyaddress.end()) {
                                action(
                                        permission_level{AddressContract, "active"_n},
                                        "eosio"_n,
                                        "burnaction"_n,
                                        std::make_tuple(burner)
                                ).send();
                            }

                            nameexpidx.erase(nameiter);
                            recordProcessed++;
                        }
                        if (recordProcessed == numbertoburn) { break; }
                        nameiter = nextname;
                    }

                    if (nameiter == nameexpidx.end()) {
                        domains.erase(domainiter);
                        recordProcessed++;

                        // Find any domains listed for sale on the fio.escrow contract table
                        auto domainsalesbydomain = domainsales.get_index<"bydomain"_n>();
                        auto domainsaleiter = domainsalesbydomain.find(domainhash);
                        // if found, call cxburned on fio.escrow
                        if(domainsaleiter != domainsalesbydomain.end()){
                            if(domainsaleiter->status == 1) {
                                action(permission_level{get_self(), "active"_n},
                                       EscrowContract, "cxburned"_n,
                                       make_tuple(domainhash)
                                ).send();
                            }
                        }
                    }

                    if (recordProcessed == numbertoburn) { break; }
                }
                index++;
                domainiter = domains.find(index);
                recordProcessed++;
                currentWork++;
            }

            if (currentWork > 0) { recordProcessed -= currentWork; }
            fio_400_assert(recordProcessed != 0, "burnexpired", "burnexpired",
                           "No work.", ErrorNoWork);
            const string response_string = string("{\"status\": \"OK\",\"items_burned\":") +
                                           to_string(recordProcessed) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        /***
         * Given a fio user name, chain name and chain specific address will attach address to the user's FIO fioname.
         *
         * @param fioaddress The FIO user name e.g. "adam@fio"
         * @param tokencode The chain name e.g. "btc"
         * @param pubaddress The chain specific user address
         */
        [[eosio::action]]
        void
        addaddress(const string &fio_address, const vector <tokenpubaddr> &public_addresses, const int64_t &max_fee,
                   const name &actor, const string &tpid) {
            require_auth(actor);
            FioAddress fa;

            getFioAddressStruct(fio_address, fa);

            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);
            fio_400_assert(validateFioNameFormat(fa), "fio_address", fa.fioaddress, "FIO Address not found",
                           ErrorDomainAlreadyRegistered);
            fio_400_assert(public_addresses.size() <= 5 && public_addresses.size() > 0, "public_addresses",
                           "public_addresses",
                           "Min 1, Max 5 public addresses are allowed",
                           ErrorInvalidNumberAddresses);

            const uint64_t fee_amount = chain_data_update(fio_address, public_addresses, max_fee, fa, actor, actor,
                                                          false,
                                                          tpid);

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            if (ADDADDRESSRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, ADDADDRESSRAM)
                ).send();
            }

            send_response(response_string.c_str());
        } //addaddress

        /***
        * Given a fio user name, chain name and chain specific address will remove the specified addresses from the user's FIO fioname.
        *
        * @param fioaddress The FIO user name e.g. "adam@fio"
        * @param tokencode The chain name e.g. "btc"
        * @param pubaddress The vector of chain specific user address
        */
        [[eosio::action]]
        void
        remaddress(const string &fio_address, const vector <tokenpubaddr> &public_addresses, const int64_t &max_fee,
                   const name &actor, const string &tpid) {
            require_auth(actor);
            FioAddress fa;

            getFioAddressStruct(fio_address, fa);

            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);
            fio_400_assert(validateFioNameFormat(fa), "fio_address", fa.fioaddress, "Invalid FIO Address",
                           ErrorDomainAlreadyRegistered);
            fio_400_assert(public_addresses.size() <= 5 && public_addresses.size() > 0, "public_addresses",
                           "public_addresses",
                           "Min 1, Max 5 public addresses are allowed",
                           ErrorInvalidNumberAddresses);

            //we want to check pub addresses, collect fee....
            const uint64_t fee_amount = perform_remove_address(fio_address, public_addresses, max_fee, fa, actor, tpid);

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);


            send_response(response_string.c_str());
        } //remaddress

        /***
        * Given a fio address, remove all pub addresses associated with that name except the FIO pub address.
        *
        * @param fioaddress The FIO user name e.g. "adam@fio"
        * @param tokencode The chain name e.g. "btc"
        * @param pubaddress The chain specific user address
        */
        [[eosio::action]]
        void
        remalladdr(const string &fio_address, const int64_t &max_fee,
                   const name &actor, const string &tpid) {
            require_auth(actor);
            FioAddress fa;

            getFioAddressStruct(fio_address, fa);

            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);
            fio_400_assert(validateFioNameFormat(fa), "fio_address", fa.fioaddress, "FIO Address not found",
                           ErrorDomainAlreadyRegistered);

            const uint64_t fee_amount = perform_remove_all_addresses(fio_address, max_fee, fa, actor, tpid);

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        } //remalladdr

        [[eosio::action]]
        void
        addnft(const string &fio_address, const vector <nftparam> &nfts, const int64_t &max_fee,
               const name &actor, const string &tpid) {
            require_auth(actor);

            FioAddress fa;
            getFioAddressStruct(fio_address, fa);
            fio_400_assert(!fa.domainOnly && validateFioNameFormat(fa), "fio_address", fa.fioaddress,
                           "Invalid FIO Address",
                           ErrorInvalidFioNameFormat);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            fio_400_assert(nfts.size() <= 3 && nfts.size() >= 1, "fio_address", fio_address,
                           "Min 1, Max 3 NFTs are allowed",
                           ErrorInvalidFioNameFormat); // Don't forget to set the error amount if/when changing MAX_SET_ADDRESSES

            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());
            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fio_address, "Invalid FIO Address",
                           ErrorFioNameNotRegistered);

            fio_403_assert(fioname_iter->owner_account == actor.value,
                           ErrorSignature); // check if actor owns FIO Address

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_404_assert(domains_iter != domainsbyname.end(), "FIO Domain not found", ErrorDomainNotFound);

            fio_400_assert(now() <= get_time_plus_seconds(domains_iter->expiration, SECONDS30DAYS),
                           "domain", fa.fiodomain, "FIO Domain expired", ErrorDomainExpired);

          auto burnqbyname = nftburnqueue.get_index<"byaddress"_n>();
          fio_400_assert(burnqbyname.find(nameHash) == burnqbyname.end(), "fio_address", fio_address,
                         "FIO Address NFTs are being burned", ErrorInvalidValue);

          auto nftbyid = nftstable.get_index<"bytokenid"_n>();


            for (auto nftobj = nfts.begin(); nftobj != nfts.end(); ++nftobj) {

                fio_400_assert(validateChainNameFormat(nftobj->chain_code.c_str()), "chain_code", nftobj->chain_code,
                               "Invalid chain code format",
                               ErrorInvalidFioNameFormat);

                if (!nftobj->url.empty()) {
                    fio_400_assert(validateRFC3986Chars(nftobj->url.c_str()), "url", nftobj->url.c_str(), "Invalid URL",
                                   ErrorInvalidFioNameFormat);
                }

                if (!nftobj->hash.empty()) {
                    fio_400_assert(validateHexChars(nftobj->hash) && nftobj->hash.length() == 64, "hash",
                                   nftobj->hash.c_str(), "Invalid hash",
                                   ErrorInvalidFioNameFormat);
                }

                if (!nftobj->metadata.empty()) {
                    fio_400_assert(nftobj->metadata.length() <= 128, "metadata", nftobj->metadata, "Invalid metadata",
                                   ErrorInvalidFioNameFormat);
                }

                fio_400_assert(!nftobj->contract_address.empty(),
                               "contract_address", nftobj->contract_address.c_str(), "Invalid Contract Address",
                               ErrorInvalidFioNameFormat);

                // now check for chain_code, token_id
                // If the contract does not exist for fio address, emplace a new record
                // If the contract does exist for fio address, test the existance of the token_id and chain_code pair then
                // only update if the hash, url or metadata is different.
                auto nft_iter = nftbyid.find(string_to_uint128_hash(string(fio_address.c_str()) +
                                                                    string(nftobj->contract_address.c_str()) +
                                                                    string(nftobj->token_id.c_str()) +
                                                                    string(nftobj->chain_code.c_str())));
                if (nft_iter == nftbyid.end()) {

                    //Create a new NFT record

                    nftstable.emplace(actor, [&](auto &n) {
                        n.id = nftstable.available_primary_key();
                        n.fio_address = fio_address;
                        n.chain_code = nftobj->chain_code;
                        n.chain_code_hash = string_to_uint64_hash(nftobj->chain_code.c_str());
                        if (!nftobj->token_id.empty()) {
                          fio_400_assert(nftobj->token_id.length() <= 128, "token_id", nftobj->token_id.c_str(), "Invalid Token ID",
                                        ErrorInvalidFioNameFormat);
                            n.token_id = nftobj->token_id.c_str();
                            n.token_id_hash = string_to_uint128_hash(string(fio_address.c_str()) +
                                                                     string(nftobj->contract_address.c_str()) +
                                                                     string(nftobj->token_id.c_str()) +
                                                                     string(nftobj->chain_code.c_str()));
                        }
                        if (!nftobj->contract_address.empty()) {
                            n.contract_address = nftobj->contract_address;
                            n.contract_address_hash = string_to_uint128_hash(nftobj->contract_address.c_str());
                        }
                        if (!nftobj->hash.empty()) {
                            n.hash = nftobj->hash;
                            n.hash_index = string_to_uint128_hash(nftobj->hash.c_str());
                        }
                        n.metadata = nftobj->metadata.empty() ? "" : nftobj->metadata;
                        n.url = nftobj->url.empty() ? "" : nftobj->url;
                        n.fio_address_hash = string_to_uint128_hash(fio_address);

                    });


                } else {

                    fio_400_assert(nft_iter->hash != nftobj->hash ||
                                   nft_iter->url != nftobj->url ||
                                   nft_iter->metadata != nftobj->metadata, "token_id", nftobj->token_id.c_str(),
                                   "Nothing to update for this token_id",
                                   ErrorInvalidFioNameFormat);

                    nftbyid.modify(nft_iter, actor, [&](auto &n) {
                        if (!nftobj->hash.empty()) {
                            n.hash = nftobj->hash;
                            n.hash_index = string_to_uint128_hash(nftobj->hash.c_str());
                        }
                        n.url = nftobj->url.empty() ? "" : nftobj->url;
                        n.metadata = nftobj->metadata.empty() ? "" : nftobj->metadata;
                    });

                }

            } // for nftobj

            uint64_t fee_amount = 0;

            if (fioname_iter->bundleeligiblecountdown > 1) {
                action{
                        permission_level{_self, "active"_n},
                        AddressContract,
                        "decrcounter"_n,
                        make_tuple(fio_address, 2)
                }.send();

            } else {

                const uint128_t endpoint_hash = string_to_uint128_hash(ADD_NFT_ENDPOINT);

                auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
                auto fee_iter = fees_by_endpoint.find(endpoint_hash);

                //if the fee isnt found for the endpoint, then 400 error.
                fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", ADD_NFT_ENDPOINT,
                               "FIO fee not found for endpoint", ErrorNoEndpoint);


                const uint64_t fee_type = fee_iter->type;
                fio_400_assert(fee_type == 1, "fee_type", to_string(fee_type),
                               "unexpected fee type for endpoint add_nft, expected 1", ErrorNoEndpoint);


                fee_amount = fee_iter->suf_amount;
                fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                               "Fee exceeds supplied maximum.",
                               ErrorMaxFeeExceeded);

                fio_fees(actor, asset(fee_amount, FIOSYMBOL), ADD_NFT_ENDPOINT);
                process_rewards(tpid, fee_amount, get_self(), actor);

                if (fee_amount > 0) {
                    INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                            (SYSTEMACCOUNT, {{_self, "active"_n}},
                             {actor, true}
                            );
                }
            }

            if (ADDNFTRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, ADDNFTRAMBASE + (ADDNFTRAM * nfts.size()))
                ).send();
            }


            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");


            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void
        remnft(const string &fio_address, const vector <remnftparam> &nfts, const int64_t &max_fee,
               const name &actor, const string &tpid) {

            require_auth(actor);

            FioAddress fa;
            getFioAddressStruct(fio_address, fa);
            fio_400_assert(!fa.domainOnly && validateFioNameFormat(fa), "fio_address", fa.fioaddress,
                           "Invalid FIO Address",
                           ErrorInvalidFioNameFormat);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            fio_400_assert(nfts.size() <= 3 && nfts.size() >= 1, "fio_address", fio_address,
                           "Min 1, Max 3 NFTs are allowed",
                           ErrorInvalidFioNameFormat); // Don't forget to set the error amount if/when changing MAX_SET_ADDRESSES

            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());
            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fio_address, "Invalid FIO Address",
                           ErrorFioNameNotRegistered);

            fio_403_assert(fioname_iter->owner_account == actor.value,
                           ErrorSignature); // check if actor owns FIO Address

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_404_assert(domains_iter != domainsbyname.end(), "FIO Domain not found", ErrorDomainNotFound);

            fio_400_assert(now() <= get_time_plus_seconds(domains_iter->expiration, SECONDS30DAYS),
                           "domain", fa.fiodomain, "FIO Domain expired", ErrorDomainExpired);

            auto nftbyid = nftstable.get_index<"bytokenid"_n>();
            auto nftbycontract = nftstable.get_index<"bycontract"_n>();

            uint32_t count_erase = 0;

            for (auto nftobj = nfts.begin(); nftobj != nfts.end(); nftobj++) {

                fio_400_assert(validateChainNameFormat(nftobj->chain_code.c_str()), "chain_code", nftobj->chain_code,
                               "Invalid chain code format",
                               ErrorInvalidFioNameFormat);

                fio_400_assert(!nftobj->contract_address.empty(), "contract_address", nftobj->contract_address.c_str(),
                               "Invalid Contract Address",
                               ErrorInvalidFioNameFormat);

                if (!nftobj->token_id.empty()) {
                    fio_400_assert(nftobj->token_id.length() <= 128, "token_id", nftobj->token_id, "Invalid Token ID",
                                   ErrorInvalidFioNameFormat);


					auto thehash = string_to_uint128_hash(string(fio_address.c_str()) +
																		string(nftobj->contract_address.c_str()) +
																		string(nftobj->token_id.c_str()) +
																		string(nftobj->chain_code.c_str()));

					auto nft_iter = nftbyid.find(thehash);

					fio_400_assert(nft_iter != nftbyid.end(), "fio_address", fio_address, "NFT not found",
								ErrorInvalidValue);

					if (nft_iter != nftbyid.end()) {
						fio_403_assert(nft_iter->fio_address == fio_address, ErrorSignature);
						nft_iter = nftbyid.erase(nft_iter);
						count_erase++;
					}

         	  }
                //bugfix BD-3826 - removes NFTs that did not correctly map a token_id hash

                if (nftobj->token_id.empty()) {
					auto contract_iter = nftbycontract.find(string_to_uint128_hash(nftobj->contract_address));

					fio_400_assert(contract_iter != nftbycontract.end(), "fio_address", fio_address, "NFT not found",
					ErrorInvalidValue);
                    for (auto idx = nftbycontract.begin(); idx != nftbycontract.end(); idx++) {
                        //if contract address, token_code match aparam and token_id_hash is 0x0000000 then remove
                        if(idx->contract_address == nftobj->contract_address && idx->chain_code == nftobj->chain_code &&
                         idx->token_id_hash == uint128_t() && 
                         idx->fio_address == fio_address ) {
                            idx = nftbycontract.erase(idx);
                            count_erase++;
                            break;
                         }
                    }

                }

            } // for auto nftobj

            fio_400_assert(count_erase > 0, "fio_address", fio_address, "No NFTs",
                           ErrorInvalidFioNameFormat);

            uint64_t fee_amount = 0;

            if (fioname_iter->bundleeligiblecountdown > 1) {
                action{
                        permission_level{_self, "active"_n},
                        AddressContract,
                        "decrcounter"_n,
                        make_tuple(fio_address, 1)
                }.send();

            } else {

                const uint128_t endpoint_hash = string_to_uint128_hash(REM_NFT_ENDPOINT);

                auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
                auto fee_iter = fees_by_endpoint.find(endpoint_hash);

                //if the fee isnt found for the endpoint, then 400 error.
                fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", REM_NFT_ENDPOINT,
                               "FIO fee not found for endpoint", ErrorNoEndpoint);


                const uint64_t fee_type = fee_iter->type;
                fio_400_assert(fee_type == 1, "fee_type", to_string(fee_type),
                               "unexpected fee type for endpoint rem_nft, expected 1", ErrorNoEndpoint);


                fee_amount = fee_iter->suf_amount;
                fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                               "Fee exceeds supplied maximum.",
                               ErrorMaxFeeExceeded);

                fio_fees(actor, asset(fee_amount, FIOSYMBOL), REM_NFT_ENDPOINT);
                process_rewards(tpid, fee_amount, get_self(), actor);

                if (fee_amount > 0) {
                    INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                            (SYSTEMACCOUNT, {{_self, "active"_n}},
                             {actor, true}
                            );
                }
            }

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());

        }


        [[eosio::action]]
        void
        remallnfts(const string &fio_address, const int64_t &max_fee,
                   const name &actor, const string &tpid) {

            require_auth(actor);

            FioAddress fa;
            getFioAddressStruct(fio_address, fa);
            fio_400_assert(!fa.domainOnly && validateFioNameFormat(fa), "fio_address", fa.fioaddress,
                           "Invalid FIO Address",
                           ErrorInvalidFioNameFormat);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);


            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());
            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fio_address, "Invalid FIO Address",
                           ErrorFioNameNotRegistered);

            fio_403_assert(fioname_iter->owner_account == actor.value,
                           ErrorSignature); // check if actor owns FIO Address

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            fio_404_assert(domains_iter != domainsbyname.end(), "FIO Domain not found", ErrorDomainNotFound);

            fio_400_assert(now() <= get_time_plus_seconds(domains_iter->expiration, SECONDS30DAYS),
                           "domain", fa.fiodomain, "FIO Domain expired", ErrorDomainExpired);


            auto contractsbyname = nftstable.get_index<"byaddress"_n>();
            auto nft_iter = contractsbyname.find(nameHash);


            fio_404_assert(nft_iter != contractsbyname.end(), "No NFTs.",
                           ErrorDomainNotFound);

            //// NEW inline function call ////
            addburnq(fio_address, nameHash);

            uint64_t fee_amount = 0;

            if (fioname_iter->bundleeligiblecountdown > 1) {
                action{
                        permission_level{_self, "active"_n},
                        AddressContract,
                        "decrcounter"_n,
                        make_tuple(fio_address, 1)
                }.send();

            } else {

                const uint128_t endpoint_hash = string_to_uint128_hash(REM_ALL_NFTS_ENDPOINT);

                auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
                auto fee_iter = fees_by_endpoint.find(endpoint_hash);

                //if the fee isnt found for the endpoint, then 400 error.
                fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", REM_ALL_NFTS_ENDPOINT,
                               "FIO fee not found for endpoint", ErrorNoEndpoint);


                const uint64_t fee_type = fee_iter->type;
                fio_400_assert(fee_type == 1, "fee_type", to_string(fee_type),
                               "unexpected fee type for endpoint rem_all_nfts, expected 1", ErrorNoEndpoint);


                fee_amount = fee_iter->suf_amount;
                fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                               "Fee exceeds supplied maximum.",
                               ErrorMaxFeeExceeded);

                fio_fees(actor, asset(fee_amount, FIOSYMBOL), REM_ALL_NFTS_ENDPOINT);
                process_rewards(tpid, fee_amount, get_self(), actor);

                if (fee_amount > 0) {
                    INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                            (SYSTEMACCOUNT, {{_self, "active"_n}},
                             {actor, true}
                            );
                }
            }

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");


            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());

        }


        [[eosio::action]]
        void
        burnnfts(const name &actor) {

            require_auth(actor);

            auto burnqbyname = nftburnqueue.get_index<"byaddress"_n>();
            auto nftburnq_iter = burnqbyname.begin();
            auto contractsbyname = nftstable.get_index<"byaddress"_n>();
            uint16_t counter = 0;
            auto nft_iter = contractsbyname.begin();
            while (nftburnq_iter != burnqbyname.end()) {
                nft_iter = contractsbyname.find(nftburnq_iter->fio_address_hash);
                counter++;
                if (nft_iter != contractsbyname.end()) { // if row, delete an nft
                    nft_iter = contractsbyname.erase(nft_iter);
                }

                if (nft_iter == contractsbyname.end()) {
                  nftburnq_iter = burnqbyname.erase(nftburnq_iter); // if no more rows, delete from nftburnqueue
                }

                if (counter == 50) break;
            }

            fio_400_assert(counter > 0, "nftburnq", std::to_string(counter),
                           "Nothing to burn", ErrorTransactionTooLarge);

            const string response_string = string("{\"status\": \"OK\"}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());

        }

        [[eosio::action]]
        void
        setdomainpub(const string &fio_domain, const int8_t is_public, const int64_t &max_fee, const name &actor,
                     const string &tpid) {
            require_auth(actor);
            FioAddress fa;
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            fio_400_assert((is_public == 1 || is_public == 0), "is_public", to_string(is_public), "Only 0 or 1 allowed",
                           ErrorMaxFeeInvalid);

            uint32_t present_time = now();
            getFioAddressStruct(fio_domain, fa);
            register_errors(fa, true);

            const uint128_t domainHash = string_to_uint128_hash(fio_domain.c_str());
            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domain_iter = domainsbyname.find(domainHash);

            fio_400_assert(domain_iter != domainsbyname.end(), "fio_domain", fa.fioaddress, "Invalid FIO domain",
                           ErrorDomainNotRegistered);
            fio_400_assert(fa.domainOnly, "fio_domain", fa.fioaddress, "Invalid FIO domain",
                           ErrorInvalidFioNameFormat);

            const uint64_t expiration = domain_iter->expiration;
            fio_400_assert(present_time <= expiration, "fio_domain", fa.fiodomain, "FIO Domain expired",
                           ErrorDomainExpired);

            //this is put into place to support the genesis of the fio chain, we need
            //to create domains and also addresses on those domains at genesis, but we only
            //have the public key for the owner of the domain, so at genesis, the eosio account
            //can make domains public and not public so as to add addresses to the domains
            //during FIO genesis. After genesis this conditional surrounding this assertion should
            //be removed.
            if (actor != SYSTEMACCOUNT) {
                fio_400_assert(domain_iter->account == actor.value, "fio_domain", fa.fioaddress,
                               "actor is not domain owner.",
                               ErrorInvalidFioNameFormat);
            }

            domainsbyname.modify(domain_iter, _self, [&](struct domain &a) {
                a.is_public = is_public;
            });

            const uint128_t endpoint_hash = string_to_uint128_hash(SET_DOMAIN_PUBLIC);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);
            const uint64_t fee_type = fee_iter->type;
            const int64_t reg_amount = fee_iter->suf_amount;

            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", SET_DOMAIN_PUBLIC,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            uint64_t fee_amount = fee_iter->suf_amount;
            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(reg_amount, FIOSYMBOL), SET_DOMAIN_PUBLIC);
            process_rewards(tpid, reg_amount, get_self(), actor);
            if (reg_amount > 0) {
                //MAS-522 remove staking from voting.
                INLINE_ACTION_SENDER(eosiosystem::system_contract, updatepower)
                        ("eosio"_n, {{_self, "active"_n}},
                         {actor, true}
                        );
            }


            if (SETDOMAINPUBRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, SETDOMAINPUBRAM)
                ).send();
            }

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");


            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransactionTooLarge);

            send_response(response_string.c_str());
        }

        /**
         *
         * Separate out the management of platform-specific identities from the fio names
         * and domains. bind2eosio, the space restricted variant of "Bind to EOSIO"
         * takes a platform-specific account name and a wallet generated public key.
         *
         * First it verify that either its is a new account and none other exists, or this
         * is an existing eosio account and it is indeed bound to this key. If it is a new,
         * unbound account name, then bind name to the key and add it to the list.
         *
         **/
        [[eosio::action]]
        void bind2eosio(const name &account, const string &client_key, const bool &existing) {
            eosio_assert((has_auth(AddressContract) || has_auth(TokenContract) || has_auth(SYSTEMACCOUNT)),
                         "missing required authority of fio.address,  fio.token, or eosio");

            fio_400_assert(isPubKeyValid(client_key), "client_key", client_key,
                           "Invalid FIO Public Key", ErrorPubKeyValid);
            auto other = accountmap.find(account.value);
            if (other != accountmap.end()) {
                eosio_assert_message_code(existing && client_key == other->clientkey, "EOSIO account already bound",
                                          ErrorPubAddressExist);
            } else {
                eosio_assert_message_code(!existing, "existing EOSIO account not bound to a key", ErrorPubAddressExist);
                accountmap.emplace(get_self(), [&](struct eosio_name &p) {
                    p.account = account.value;
                    p.clientkey = client_key;
                    p.keyhash = string_to_uint128_hash(client_key.c_str());
                });
            }
        }

        [[eosio::action]]
        void xferaddress(const string &fio_address, const string &new_owner_fio_public_key, const int64_t &max_fee,
                         const name &actor, const string &tpid) {
            require_auth(actor);
            FioAddress fa;
            getFioAddressStruct(fio_address, fa);

            fio_400_assert(validateFioNameFormat(fa) && !fa.domainOnly, "fio_address", fa.fioaddress,
                           "Invalid FIO Address",
                           ErrorDomainAlreadyRegistered);
            fio_400_assert(isPubKeyValid(new_owner_fio_public_key), "new_owner_fio_public_key",
                           new_owner_fio_public_key,
                           "Invalid FIO Public Key", ErrorChainAddressEmpty);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fa.fioaddress,
                           "FIO Address not registered", ErrorFioNameAlreadyRegistered);

            fio_403_assert(fioname_iter->owner_account == actor.value, ErrorSignature);
            const uint128_t endpoint_hash = string_to_uint128_hash(TRANSFER_ADDRESS_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", TRANSFER_ADDRESS_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            string owner_account;
            key_to_account(new_owner_fio_public_key, owner_account);
            const name nm = accountmgnt(actor, new_owner_fio_public_key);

            auto producersbyaddress = producers.get_index<"byaddress"_n>();
            auto prod_iter = producersbyaddress.find(nameHash);
            if (prod_iter != producersbyaddress.end()) {
                fio_400_assert(!prod_iter->is_active, "fio_address", fio_address,
                               "FIO Address is active producer. Unregister first.", ErrorNoEndpoint);
            }

            auto proxybyaddress = voters.get_index<"byaddress"_n>();
            auto proxy_iter = proxybyaddress.find(nameHash);
            if (proxy_iter != proxybyaddress.end()) {
                fio_400_assert(!proxy_iter->is_proxy, "fio_address", fio_address,
                               "FIO Address is proxy. Unregister first.", ErrorNoEndpoint);
            }

            vector <tokenpubaddr> pubaddresses;
            tokenpubaddr t1;
            t1.public_address = new_owner_fio_public_key;
            t1.token_code = "FIO";
            t1.chain_code = "FIO";
            pubaddresses.push_back(t1);

            //Transfer the address
            namesbyname.modify(fioname_iter, actor, [&](struct fioname &a) {
                a.owner_account = nm.value;
                a.addresses = pubaddresses;
            });

            //FIP-39 begin
            //update the encryption key to use.
            updfionminf(new_owner_fio_public_key, FIO_REQUEST_CONTENT_ENCRYPTION_PUB_KEY_DATA_DESC,fioname_iter->id,actor);
            //FIP-39 end

            // Burn the NFTs belonging to the FIO address that was just transferred

            auto contractsbyname = nftstable.get_index<"byaddress"_n>();
            auto nft_iter = contractsbyname.find(nameHash);

            //// NEW inline function call ////
            addburnq(fio_address, nameHash);

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_address, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), TRANSFER_ADDRESS_ENDPOINT);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (XFERRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, XFERRAM)
                ).send();
            }

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void burnaddress(const string &fio_address, const int64_t &max_fee, const string &tpid, const name &actor) {
            require_auth(actor);
            FioAddress fa;
            getFioAddressStruct(fio_address, fa);

            fio_400_assert(validateFioNameFormat(fa) && !fa.domainOnly, "fio_address", fa.fioaddress,
                           "Invalid FIO Address",
                           ErrorDomainAlreadyRegistered);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fa.fioaddress,
                           "FIO Address not registered", ErrorFioNameAlreadyRegistered);

            fio_403_assert(fioname_iter->owner_account == actor.value, ErrorSignature);

            auto producersbyaddress = producers.get_index<"byaddress"_n>();
            auto prod_iter = producersbyaddress.find(nameHash);
            if (prod_iter != producersbyaddress.end()) {
                fio_400_assert(!prod_iter->is_active, "fio_address", fio_address,
                               "FIO Address is active producer. Unregister first.", ErrorNoEndpoint);
            }

            auto proxybyaddress = voters.get_index<"byaddress"_n>();
            auto proxy_iter = proxybyaddress.find(nameHash);
            if (proxy_iter != proxybyaddress.end()) {
                fio_400_assert(!proxy_iter->is_proxy, "fio_address", fio_address,
                               "FIO Address is proxy. Unregister first.", ErrorNoEndpoint);
            }

            auto tpid_by_name = tpids.get_index<"byname"_n>();
            auto tpid_iter = tpid_by_name.find(nameHash);

            //do the burn
            const uint64_t bundleeligiblecountdown = fioname_iter->bundleeligiblecountdown;
            namesbyname.erase(fioname_iter);
            if (tpid_iter != tpid_by_name.end()) { tpid_by_name.erase(tpid_iter); }

            //FIP-39 begin
            //remove the associated handle information.
            remhandleinf(fioname_iter->id);
            //FIP-39 end

            //// NEW inline function call ////
            addburnq(fio_address, nameHash);

            //fees
            uint64_t fee_amount = 0;
            const uint128_t endpoint_hash = string_to_uint128_hash("burn_fio_address");
            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);

            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", BURN_FIO_ADDRESS_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 1, "fee_type", to_string(fee_type),
                           "burn_fio_address unexpected fee type for endpoint burn_fio_address, expected 1",
                           ErrorNoEndpoint);

            if (bundleeligiblecountdown == 0) {
                fee_amount = fee_iter->suf_amount;
                fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                               "Fee exceeds supplied maximum.",
                               ErrorMaxFeeExceeded);

                fio_fees(actor, asset(fee_amount, FIOSYMBOL), BURN_FIO_ADDRESS_ENDPOINT);
                process_rewards(tpid, fee_amount, get_self(), actor);
            }

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);

            send_response(response_string.c_str());

        }

        [[eosio::action]]
        void xferdomain(const string &fio_domain, const string &new_owner_fio_public_key, const int64_t &max_fee,
                        const name &actor, const string &tpid) {
            require_auth(actor);
            FioAddress fa;
            getFioAddressStruct(fio_domain, fa);

            register_errors(fa, true);
            fio_400_assert(isPubKeyValid(new_owner_fio_public_key), "new_owner_fio_public_key",
                           new_owner_fio_public_key,
                           "Invalid FIO Public Key", ErrorChainAddressEmpty);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(string_to_uint128_hash(fio_domain));
            fio_400_assert(domains_iter != domainsbyname.end(), "fio_domain", fio_domain,
                           "FIO Domain not registered", ErrorDomainNotRegistered);

            fio_403_assert(domains_iter->account == actor.value, ErrorSignature);
            const uint128_t endpoint_hash = string_to_uint128_hash(TRANSFER_DOMAIN_ENDPOINT);

            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", TRANSFER_DOMAIN_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            //Transfer the domain
            string owner_account;
            key_to_account(new_owner_fio_public_key, owner_account);
            const name nm = accountmgnt(actor, new_owner_fio_public_key);
            domainsbyname.modify(domains_iter, actor, [&](struct domain &a) {
                a.account = nm.value;
            });

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "unexpected fee type for endpoint transfer_fio_domain, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), TRANSFER_DOMAIN_ENDPOINT);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            if (XFERRAM > 0) {
                action(
                        permission_level{SYSTEMACCOUNT, "active"_n},
                        "eosio"_n,
                        "incram"_n,
                        std::make_tuple(actor, XFERRAM)
                ).send();
            }

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void addbundles(const string &fio_address, const int64_t &bundle_sets, const int64_t &max_fee,
                        const string &tpid, const name &actor) {
            require_auth(actor);
            FioAddress fa;
            getFioAddressStruct(fio_address, fa);

            fio_400_assert(validateFioNameFormat(fa) && !fa.domainOnly, "fio_address", fa.fioaddress,
                           "Invalid FIO Address",
                           ErrorDomainAlreadyRegistered);
            fio_400_assert(validateTPIDFormat(tpid), "tpid", tpid,
                           "TPID must be empty or valid FIO address",
                           ErrorPubKeyValid);
            fio_400_assert(max_fee >= 0, "max_fee", to_string(max_fee), "Invalid fee value",
                           ErrorMaxFeeInvalid);
            fio_400_assert(bundle_sets > 0, "bundle_sets", to_string(bundle_sets), "Invalid bundle_sets value",
                           ErrorMaxFeeInvalid);

            const uint128_t nameHash = string_to_uint128_hash(fa.fioaddress.c_str());
            const uint128_t domainHash = string_to_uint128_hash(fa.fiodomain.c_str());

            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);
            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fa.fioaddress,
                           "FIO Address not registered", ErrorFioNameAlreadyRegistered);

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(domainHash);

            const uint32_t domain_expiration = domains_iter->expiration;
            const uint32_t present_time = now();
            fio_400_assert(present_time <= domain_expiration, "fio_address", fa.fioaddress, "FIO Domain expired",
                           ErrorDomainExpired);

            const uint128_t endpoint_hash = string_to_uint128_hash("add_bundled_transactions");
            auto fees_by_endpoint = fiofees.get_index<"byendpoint"_n>();
            auto fee_iter = fees_by_endpoint.find(endpoint_hash);
            fio_400_assert(fee_iter != fees_by_endpoint.end(), "endpoint_name", ADD_BUNDLED_TRANSACTION_ENDPOINT,
                           "FIO fee not found for endpoint", ErrorNoEndpoint);

            //Add bundle
            uint64_t current_bundle = fioname_iter->bundleeligiblecountdown;
            uint64_t single_bundle = getBundledAmount();
            uint64_t set_bundle = current_bundle + (bundle_sets * single_bundle);

            namesbyname.modify(fioname_iter, actor, [&](struct fioname &a) {
                a.bundleeligiblecountdown = set_bundle;
            });

            //fees
            const uint64_t fee_amount = fee_iter->suf_amount * bundle_sets;
            const uint64_t fee_type = fee_iter->type;

            fio_400_assert(fee_type == 0, "fee_type", to_string(fee_type),
                           "add_bundled_transactions unexpected fee type for endpoint add_bundled_transactions, expected 0",
                           ErrorNoEndpoint);

            fio_400_assert(max_fee >= (int64_t) fee_amount, "max_fee", to_string(max_fee),
                           "Fee exceeds supplied maximum.",
                           ErrorMaxFeeExceeded);

            fio_fees(actor, asset(fee_amount, FIOSYMBOL), ADD_BUNDLED_TRANSACTION_ENDPOINT);
            processbucketrewards(tpid, fee_amount, get_self(), actor);

            const string response_string = string("{\"status\": \"OK\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);

            send_response(response_string.c_str());
        }

        void decrcounter(const string &fio_address, const int32_t &step) {

        check(step > 0, "step must be greater than 0");
        check((has_auth(AddressContract) || has_auth(TokenContract) || has_auth(TREASURYACCOUNT) || has_auth(STAKINGACCOUNT) ||
                     has_auth(REQOBTACCOUNT) || has_auth(SYSTEMACCOUNT) || has_auth(FeeContract)),
                     "missing required authority of fio.address, fio.token, fio.fee, fio.treasury, fio.reqobt, fio.system, fio.staking ");

            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(string_to_uint128_hash(fio_address.c_str()));
            fio_400_assert(fioname_iter != namesbyname.end(), "fio_address", fio_address,
                           "FIO address not registered", ErrorFioNameAlreadyRegistered);

            if (fioname_iter->bundleeligiblecountdown > step - 1) {
                namesbyname.modify(fioname_iter, _self, [&](struct fioname &a) {
                    a.bundleeligiblecountdown = (fioname_iter->bundleeligiblecountdown - step);
                });
            } else
                check(false, "Failed to decrement eligible bundle counter"); // required to fail the parent transaction
        }

        [[eosio::action]]
        void xferescrow(const string &fio_domain, const string &public_key, const bool isEscrow, const name &actor){
            name nm;
            // This inline permissioned action is used during wrapping and marketplace operations.
            if(has_auth(EscrowContract)){
                nm = name("fio.escrow");
            } else{
                require_auth(FIOORACLEContract);
                nm = name("fio.oracle");
            }

            FioAddress fa;
            getFioAddressStruct(fio_domain, fa);

            register_errors(fa, true);
            if(!isEscrow) {
                fio_400_assert(isPubKeyValid(public_key), "public_key", public_key,
                               "Invalid FIO Public Key", ErrorChainAddressEmpty);
            }

            auto domainsbyname = domains.get_index<"byname"_n>();
            auto domains_iter = domainsbyname.find(string_to_uint128_hash(fio_domain));
            fio_400_assert(domains_iter != domainsbyname.end(), "fio_domain", fio_domain,
                           "FIO Domain not registered", ErrorDomainNotRegistered);

            const uint32_t domain_expiration = domains_iter->expiration;
            const uint32_t present_time = now();
            fio_400_assert(present_time <= domain_expiration, "fio_domain", fio_domain, "FIO Domain expired. Renew first.",
                           ErrorDomainExpired);

            //Transfer the domain
            if(!isEscrow){
                string owner_account;
                key_to_account(public_key, owner_account);
                nm = name(owner_account);
            }

            domainsbyname.modify(domains_iter, _self, [&](struct domain &a) {
                a.account = nm.value;
            });

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);
        }

    };

    EOSIO_DISPATCH(FioNameLookup, (regaddress)(addaddress)(remaddress)(remalladdr)(regdomain)(renewdomain)(renewaddress)
    (setdomainpub)(burnexpired)(decrcounter)(bind2eosio)(burnaddress)(xferdomain)(xferaddress)(addbundles)(xferescrow)
    (addnft)(remnft)(remallnfts)(burnnfts)(regdomadd)(updcryptkey))
}
