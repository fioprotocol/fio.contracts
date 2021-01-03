/** Fio Templete implementation file
 *  Description:
 *  @author Casey Gardiner
 *  @modifedby
 *  @file fio.templete.cpp
 *  @license FIO Foundation ( https://github.com/fioprotocol/fio/blob/master/LICENSE ) Dapix
 */

#include "fio.templete.hpp"

namespace fioio {

    class [[eosio::contract("FIOTemplete")]]  FIOTemplete: public eosio::contract {

    private:
        // Table Reference Go Here
    public:
        using contract::contract;

        FIOTemplete(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds){
        }

        [[eosio::action]]
        void tempfunction1(const string &tpid, const name owner, const uint64_t &amount) {
            print("temp function 1");
        }
    };     //class FIOTemplete

    EOSIO_DISPATCH(FIOTemplete, (tempfunction1))
}