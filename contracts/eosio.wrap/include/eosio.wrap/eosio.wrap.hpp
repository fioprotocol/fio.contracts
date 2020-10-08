#pragma once

#include <eosio/eosio.hpp>
#include <eosio/ignore.hpp>
#include <eosio/transaction.hpp>

namespace eosio {

    class [[eosio::contract("eosio.wrap")]] wrap : public contract {
    public:
        using contract::contract;

        [[eosio::action]]
        void execute(ignore <name> executer, ignore <transaction> trx);

        using exec_action = eosio::action_wrapper<"execute"_n, &wrap::execute>;
    };

} /// namespace eosio
