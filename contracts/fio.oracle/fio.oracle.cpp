/** Fio Oracle implementation file
 *  Description:
 *  @author Casey Gardiner
 *  @modifedby
 *  @file fio.oracle.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE )
 */

#include "fio.oracle.hpp"
#include <fio.fee/fio.fee.hpp>
#include <fio.address/fio.address.hpp>
#include <fio.common/fiotime.hpp>

namespace fioio {

    class [[eosio::contract("FIOOracle")]]  FIOOracle : public eosio::contract {

    private:
        oracleledger_table receipts;
        oraclevoters_table voters;
        oracles_table oracles;
        fionames_table fionames;
        config appConfig;
    public:
        using contract::contract;

        FIOOracle(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds),
                receipts(_self, _self.value),
                voters(_self, _self.value),
                oracles(_self, _self.value),
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
            action(permission_level{SYSTEMACCOUNT, "active"_n},
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

            const uint128_t nameHash = string_to_uint128_hash(fio_address);
            auto namesbyname = fionames.get_index<"byname"_n>();
            auto fioname_iter = namesbyname.find(nameHash);

            const uint128_t idHash = string_to_uint128_hash(obt_id);
            auto votesbyid = voters.get_index<"byidhash"_n>();
            auto voters_iter = votesbyid.find(idHash);

            fio_404_assert(fioname_iter != namesbyname.end(), "FIO Address not found", ErrorFioNameNotRegistered);
            const uint64_t recAcct = fioname_iter->owner_account;

            //log entry into table
            voters.emplace(actor, [&](struct oracle_votes &p) {
                p.id = voters.available_primary_key();
                p.voter = actor.value;
                p.idhash = idHash;
                p.obt_id = obt_id;
                p.fio_address = fio_address;
                p.amount = amount;
            });

            //verify obt and address match other entries
            //if( voters_iter.size > 0 ) {
            //

            // if entries vs. number of regoracles meet consensus.
            //Tokens are transferred to fio.wrapping.
            action(permission_level{SYSTEMACCOUNT, "active"_n},
                   TokenContract, "transfer"_n,
                   make_tuple(FIOORACLEContract, recAcct, asset(amount, FIOSYMBOL), string("Token Unwrapping"))
            ).send();

            const string response_string = string("{\"status\": \"OK\"}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);

            send_response(response_string.c_str());
        }
    };

    EOSIO_DISPATCH(FIOOracle, (wraptokens)(unwraptokens)
    //regoracle - must be topprod AND must be eosio perms
    //unregoracle - oracle or eosio can remove
    //setoraclefee - force lower case

    //wrapdomain - xferdomain to fio.oracle
    //unwrapdomain - change owner to supplied fio address
    )
}
