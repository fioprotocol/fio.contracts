//
// Created by tvle83 on 20-Jan-21.
//
#include "fio.escrow.hpp"

namespace fioio {

    class [[eosio::contract("FioEscrow")]] FioEscrow : public eosio::contract {
    public:
        using contract::contract;

        FioEscrow(name s, name code, datastream<const char *> ds) :
                contract(s, code, ds) {
        }

        [[eosio::action]]
        void hi(name nm) {
            print_f("Name : %\n", nm);
        }

//        [[eosio::action]]
//        void check(name nm) {
//            print_f("Name : %\n", nm);
//            eosio::check(nm == "hello"_n, "check name not equal to `hello`");
//        }
    };

    EOSIO_DISPATCH(FioEscrow, (hi))
}