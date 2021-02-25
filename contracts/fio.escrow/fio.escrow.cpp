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
        void listdomain(const name &actor){
            require_auth(actor);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void cxlistdomain(const name &actor){
            require_auth(actor);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }

        [[eosio::action]]
        void buydomain(const name &actor){
            require_auth(actor);

            const string response_string = string("{\"status\": \"OK\"}");

            send_response(response_string.c_str());
        }
    };

    EOSIO_DISPATCH(FioEscrow, (hi))
}