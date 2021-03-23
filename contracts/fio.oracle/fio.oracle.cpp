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
        eosiosystem::producers_table producers;
        eosio_names_table accountmap;
        config appConfig;
    public:
        using contract::contract;

        FIOOracle(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                receipts(_self, _self.value),
                voters(_self, _self.value),
                oracles(_self, _self.value),
                producers(SYSTEMACCOUNT, SYSTEMACCOUNT.value),
                accountmap(AddressContract, AddressContract.value),
                fionames(AddressContract, AddressContract.value) {
            configs_singleton configsSingleton(FeeContract, FeeContract.value);
            appConfig = configsSingleton.get_or_default(config());
        }

        [[eosio::action]]
        void wraptokens(uint64_t &amount, string &chain_code, string &public_address, uint64_t &max_oracle_fee,
                        uint64_t &max_fee, string &tpid, name &actor) {

            //validation will go here
            //min/max amount?
            //chaincode check
            //public address check
            //fee checks
            //tpid validation
            //actor validation

            uint64_t oracle_fee = max_oracle_fee; //temp
            uint64_t fee_amount = max_fee; //temp
            const uint32_t present_time = now();

            //Oracle fee is transferred from actor account to all registered oracles in even amount.
            // median fee / oracle_info.size = fee paid
            // for ( oracle_info.size ) xfer oracle fee

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

            //RAM of signer is increased (512)

            const string response_string = string("{\"status\": \"OK\",\"oracle_fee_collected\":\"") +
                                           to_string(oracle_fee) + string("\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void unwraptokens(uint64_t &amount, string &obt_id, string &fio_address, name &actor) {

            //validation will go here
            //min/max amount?
            //fio address format check

            //actor validation (must be oracle)
            require_auth(actor);
            auto oraclesearch = oracles.find(actor.value);
            fio_400_assert(oraclesearch != oracles.end(), "actor", actor.to_string(),
                           "actor is not a registered Oracle", ErrorPubAddressExist);

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
            if(voters_iter != votesbyid.end()){
                tempvoters = voters_iter->voters;

                auto it = std::find(tempvoters.begin(), tempvoters.end(), actor);
                fio_400_assert(it == tempvoters.end(), "actor", actor.to_string(),
                               "Oracle has already voted.", ErrorPubAddressExist);

                tempvoters.push_back(actor);

                votesbyid.modify(voters_iter, actor, [&](auto &p) {
                    p.voters = tempvoters;
                });
            } else {
                tempvoters.push_back(actor);

                voters.emplace(actor, [&](struct oracle_votes &p) {
                    p.id = voters.available_primary_key();
                    p.idhash = idHash;
                    p.voters = tempvoters;
                    p.obt_id = obt_id;
                    p.fio_address = fio_address;
                    p.amount = amount;
                });
            }

            //verify obt and address match other entries
            auto oracle_size = std::distance(oracles.cbegin(),oracles.cend());
            uint8_t size = tempvoters.size();
            // if entries vs. number of regoracles meet consensus.
            if(oracle_size == size){
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
            //regoracle - must be topprod AND must be eosio perms
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

            std::vector<oraclefees> tempVec;
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
        void unregoracle(name oracle_actor, name &actor) {
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
    };

    EOSIO_DISPATCH(FIOOracle, (wraptokens)(unwraptokens)(regoracle)(unregoracle)
    //setoraclefee - force lower case
    //wrapdomain - xferdomain to fio.oracle
    //unwrapdomain - change owner to supplied fio address
    )
}
