#include <fio.common/fio.common.hpp>

namespace fioio {

    class [[eosio::contract("FioEscrow")]]  FioEscrow : public eosio::contract {
        [[eosio::action]]
        void hi(name nm) {
            print_f("Name : %\n", nm);
        }

        [[eosio::action]]
        void check(name nm) {
            print_f("Name : %\n", nm);
            eosio::check(nm == "hello"_n, "check name not equal to `hello`");
        }
    };
}