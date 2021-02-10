/** Fio Oracle implementation file
 *  Description:
 *  @author Casey Gardiner
 *  @modifedby
 *  @file fio.oracle.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE )
 */

#include "fio.oracle.hpp"
#include <fio.fee/fio.fee.hpp>
#include <fio.common/fio.common.hpp>
#include <fio.common/fiotime.hpp>
#include <eosiolib/asset.hpp>

namespace fioio {

    class [[eosio::contract("FIOOracle")]]  FIOOracle : public eosio::contract {

    private:
        oraclelegder_table receipts;
    public:
        using contract::contract;

        FIOOracle(name s, name code, datastream<const char *> ds) :
                receipts(_self, _self.value),
                contract(s, code, ds) {
        }

        [[eosio::action]]
        void wraptokens(uint64_t &amount, string &chain_code, string &public_address, uint64_t &max_oracle_fee,
                        uint64_t &max_fee, string &tpid, name &actor) {

            //validation will go here
            uint64_t oracle_fee = max_oracle_fee; //temp
            uint64_t fee_amount = max_fee; //temp

            //Oracle fee is transferred from actor account to all registered oracles in even amount.
            //Chain wrap_fio_token fee is collected.
            //RAM of signer is increased

            //Copy information to receipt table
            receipts.emplace(_self, [&](struct oraclelegder &p) {
                p.id = receipts.available_primary_key();
                p.actor = actor.value;
                p.chaincode = chain_code;
                p.pubaddress = public_address;
                p.amount = amount;
            });

            //Tokens are transferred to fio.wrapping.
            action(permission_level{SYSTEMACCOUNT, "active"_n},
                   TokenContract, "transfer"_n,
                   make_tuple(actor, FIOORACLEContract, amount, "Token Wrapping")

            ).send();

            const string response_string = string("{\"status\": \"OK\",\"oracle_fee_collected\":\"") +
                                           to_string(oracle_fee) + string("\",\"fee_collected\":") +
                                           to_string(fee_amount) + string("}");

            fio_400_assert(transaction_size() <= MAX_TRX_SIZE, "transaction_size", std::to_string(transaction_size()),
                           "Transaction is too large", ErrorTransaction);

            send_response(response_string.c_str());
        }
    };

    EOSIO_DISPATCH(FIOOracle, (wraptokens)
    )
}
